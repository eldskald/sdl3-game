#include "engine/bgs.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/audio.h"
#include "engine/game-manager.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/logger.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

typedef struct {
    const char* name;
    const Uint8* data;
    size_t size;
    audio_bus bus;
} track_source;

typedef struct {
    audio_ogg* ogg;
    SDL_AudioStream* stream;
    bool playing;
    bool decoded;
    Uint32 position;
    Uint32 loop_start, loop_end;
    float volume;
    bool stop_when_silent;
    bool fading;
    float fade_target;
    float fade_rate;
} track_data;



static const Uint8 cave_ogg[] = {
#embed BGS_CAVE_PATH
};
static const Uint8 water_streaming_ogg[] = {
#embed BGS_WATER_STREAMING_PATH
};
static const Uint8 lava_bubbling_ogg[] = {
#embed BGS_LAVA_BUBBLING_PATH
};
static const Uint8 acid_fizz_ogg[] = {
#embed BGS_ACID_FIZZ_PATH
};
static const Uint8 sludge_bubbling_ogg[] = {
#embed BGS_SLUDGE_BUBBLING_PATH
};
static const char tracks_csv[] = {
#embed BGS_TRACKS_CSV_PATH suffix(, '\0')
};

// Fluids' tracks are sounds of the gameplay, so they're on the same bus as the
// SFX and follow their volume instead of the ambience's.
static const track_source sources[BGS_TRACKS_TOTAL] = {
    [bgs_track_cave] = {"cave",
                        cave_ogg,
                        sizeof(cave_ogg),
                        audio_bus_ambience_tracks},
    [bgs_track_water_streaming] = {"water_streaming",
                                   water_streaming_ogg,
                                   sizeof(water_streaming_ogg),
                                   audio_bus_gameplay_sounds},
    [bgs_track_lava_bubbling] = {"lava_bubbling",
                                 lava_bubbling_ogg,
                                 sizeof(lava_bubbling_ogg),
                                 audio_bus_gameplay_sounds},
    [bgs_track_acid_fizz] = {"acid_fizz",
                             acid_fizz_ogg,
                             sizeof(acid_fizz_ogg),
                             audio_bus_gameplay_sounds},
    [bgs_track_sludge_bubbling] = {"sludge_bubbling",
                                   sludge_bubbling_ogg,
                                   sizeof(sludge_bubbling_ogg),
                                   audio_bus_gameplay_sounds},

};
static track_data tracks[BGS_TRACKS_TOTAL] = {0};
static SDL_Mutex* tracks_mutex = NULL;
static float scratch[BGS_DECODE_CHUNK_SAMPLES] = {0};

static bool is_track(bgs_track track) {
    return track > bgs_track_none && track < BGS_TRACKS_TOTAL;
}

static void stop(track_data* t) {
    SDL_ClearAudioStream(t->stream);
    t->playing = false;
    t->fading = false;
}

// Stops `t` once it's silent if it's set to.
static void set_volume(track_data* t, float volume) {
    t->volume = volume;
    SDL_SetAudioStreamGain(t->stream, volume);
    if (volume <= 0.0f && t->stop_when_silent) stop(t);
}

static Uint32 ms_to_frames(const track_data* t, Uint32 ms) {
    return (Uint32)((Uint64)ms * t->ogg->spec.freq / 1000);
}

// Frames queued on the stream that haven't been heard yet.
static Uint32 queued_frames(const track_data* t) {
    int queued = SDL_GetAudioStreamQueued(t->stream);
    int frame_size = (int)sizeof(float) * t->ogg->spec.channels;
    return queued > 0 ? (Uint32)(queued / frame_size) : 0;
}

static bool is_playing(const track_data* t) {
    return t->playing && !(t->decoded && queued_frames(t) == 0);
}

// Decodes up to `frames` frames from where the decoder is, going back to
// `loop_start` at `loop_end`. A file shorter than its loop points ends early,
// and is taken as decoded if that leaves nothing to loop.
static void decode(track_data* t, Uint32 frames) {
    int channels = t->ogg->spec.channels;
    Uint32 end = t->loop_end;
    Uint32 chunk_max = BGS_DECODE_CHUNK_SAMPLES / channels;
    while (frames > 0) {
        if (t->position >= end) {
            if (t->loop_start >= end ||
                !AUDIO_seek_ogg(t->ogg, t->loop_start)) {
                t->decoded = true;
                SDL_FlushAudioStream(t->stream);
                return;
            }
            t->position = t->loop_start;
        }
        Uint32 n = SDL_min(SDL_min(frames, end - t->position), chunk_max);
        int got = AUDIO_decode_ogg(t->ogg, scratch, (int)n);
        if (got <= 0) {
            end = t->position;
            continue;
        }
        SDL_PutAudioStreamData(
            t->stream, scratch, got * channels * (int)sizeof(float));
        t->position += got;
        frames -= got;
    }
}

static void fill(track_data* t) {
    if (!t->playing) return;
    if (t->decoded) {
        if (queued_frames(t) == 0) t->playing = false;
        return;
    }
    Uint32 target = ms_to_frames(t, BGS_BUFFER_MS);
    Uint32 queued = queued_frames(t);
    if (queued >= target) return;
    decode(t, SDL_min(target - queued, ms_to_frames(t, BGS_MAX_FILL_MS)));
}

static void fade(track_data* t, double dt) {
    if (!t->fading) return;
    float step = (float)(t->fade_rate * dt);
    float diff = t->fade_target - t->volume;
    if (t->fade_rate <= 0.0f || SDL_fabsf(diff) <= step) {
        t->fading = false;
        set_volume(t, t->fade_target);
        return;
    }
    set_volume(t, t->volume + (diff > 0.0f ? step : -step));
}

typedef struct {
    bgs_track track;
    Uint32 loop_start, loop_end;
    bool stop_when_silent;
} track_row;

static void set_row(const track_row* row) {
    track_data* t = &tracks[row->track];
    t->stop_when_silent = row->stop_when_silent;
    if (!t->ogg) return;
    t->loop_start = row->loop_start;
    t->loop_end = row->loop_end ? row->loop_end : t->ogg->frames;
}

static void load_rows(void) {
    dynarr parsed = CSV_parse(tracks_csv);
    track_row* rows = SDL_calloc(count_csv_entries(&parsed), sizeof(track_row));
    parsing_field fields[] = {
        {"track", parsing_bgs_track, &rows->track},
        {"loop_start", parsing_uint32, &rows->loop_start},
        {"loop_end", parsing_uint32, &rows->loop_end},
        {"stop_when_silent", parsing_bool, &rows->stop_when_silent},
    };
    size_t n = read_csv_table(
        &parsed, fields, SDL_arraysize(fields), sizeof(track_row));
    CSV_free(&parsed);
    for (size_t i = 0; i < n; i++)
        set_row(&rows[i]);
    SDL_free(rows);
}

// Its stream's output format is set to the device's when bound.
static void load_track(bgs_track track) {
    const track_source* s = &sources[track];
    track_data* t = &tracks[track];
    *t = (track_data){.volume = 1.0f};
    t->ogg = AUDIO_create_ogg_from_mem(s->name, s->data, s->size);
    if (!t->ogg) return;
    t->loop_end = t->ogg->frames;
    SDL_AudioDeviceID device = AUDIO_get_bus_device(s->bus);
    if (!device) return;
    t->stream = SDL_CreateAudioStream(&t->ogg->spec, NULL);
    if (!log_creation(SDL_LOG_CATEGORY_AUDIO, t->stream, "%s stream", s->name))
        return;
    SDL_BindAudioStream(device, t->stream);
}



void BGS_play_track(bgs_track track) {
    if (!is_track(track)) return;
    SDL_LockMutex(tracks_mutex);
    track_data* t = &tracks[track];
    if (t->stream && AUDIO_seek_ogg(t->ogg, 0)) {
        stop(t);
        t->position = 0;
        t->decoded = false;
        t->playing = true;
    }
    SDL_UnlockMutex(tracks_mutex);
}

void BGS_stop_track(bgs_track track) {
    if (!is_track(track)) return;
    SDL_LockMutex(tracks_mutex);
    stop(&tracks[track]);
    SDL_UnlockMutex(tracks_mutex);
}

bool BGS_is_track_playing(bgs_track track) {
    if (!is_track(track)) return false;
    SDL_LockMutex(tracks_mutex);
    bool playing = is_playing(&tracks[track]);
    SDL_UnlockMutex(tracks_mutex);
    return playing;
}

dynarr BGS_get_playing_tracks(void) {
    dynarr playing = (dynarr){0};
    init_dynarr_with(&playing, sizeof(bgs_track), BGS_TRACKS_TOTAL);
    SDL_LockMutex(tracks_mutex);
    for (bgs_track track = bgs_track_none + 1; track < BGS_TRACKS_TOTAL;
         track++)
        if (is_playing(&tracks[track])) push_to_dynarr(&track, &playing);
    SDL_UnlockMutex(tracks_mutex);
    return playing;
}

void BGS_set_track_volume(bgs_track track, float volume) {
    if (!is_track(track)) return;
    SDL_LockMutex(tracks_mutex);
    tracks[track].fading = false;
    set_volume(&tracks[track], volume > 0.0f ? volume : 0.0f);
    SDL_UnlockMutex(tracks_mutex);
}

float BGS_get_track_volume(bgs_track track) {
    if (!is_track(track)) return 0.0f;
    SDL_LockMutex(tracks_mutex);
    float volume = tracks[track].volume;
    SDL_UnlockMutex(tracks_mutex);
    return volume;
}

void BGS_fade_track(bgs_track track, float volume, float rate) {
    if (!is_track(track)) return;
    SDL_LockMutex(tracks_mutex);
    track_data* t = &tracks[track];
    t->fade_target = volume > 0.0f ? volume : 0.0f;
    t->fade_rate = rate;
    t->fading = true;
    SDL_UnlockMutex(tracks_mutex);
}

bool BGS_get_track_stops_when_silent(bgs_track track) {
    return is_track(track) && tracks[track].stop_when_silent;
}

hashmap BGS_get_string_tracks_map(void) {
    hashmap map;
    init_hashmap(&map, sizeof(bgs_track));
    for (bgs_track track = bgs_track_none + 1; track < BGS_TRACKS_TOTAL;
         track++)
        set_on_hashmap(&track, sources[track].name, &map);
    return map;
}



void BGS_start(void) {
    tracks_mutex = SDL_CreateMutex();
    log_creation(SDL_LOG_CATEGORY_AUDIO, tracks_mutex, "bgs tracks mutex");
    for (bgs_track track = bgs_track_none + 1; track < BGS_TRACKS_TOTAL;
         track++)
        load_track(track);
    load_rows();
}

void BGS_update(void) {
    double dt = GAME_MANAGER_get_current_dt();
    SDL_LockMutex(tracks_mutex);
    for (bgs_track track = bgs_track_none + 1; track < BGS_TRACKS_TOTAL;
         track++) {
        track_data* t = &tracks[track];
        if (!t->stream) continue;
        fade(t, dt);
        fill(t);
    }
    SDL_UnlockMutex(tracks_mutex);
}

// Streams go first, since destroying one waits for its device to be done with
// it, and only then their decoders.
void BGS_stop(void) {
    for (bgs_track track = bgs_track_none + 1; track < BGS_TRACKS_TOTAL;
         track++) {
        SDL_DestroyAudioStream(tracks[track].stream);
        AUDIO_destroy_ogg(tracks[track].ogg);
        tracks[track] = (track_data){0};
    }
    SDL_DestroyMutex(tracks_mutex);
    tracks_mutex = NULL;
}



SDL_AudioStream* BGS_get_track_stream(bgs_track track) {
    return is_track(track) ? tracks[track].stream : NULL;
}

void BGS_get_track_loop(bgs_track track, Uint32* loop_start, Uint32* loop_end) {
    *loop_start = is_track(track) ? tracks[track].loop_start : 0;
    *loop_end = is_track(track) ? tracks[track].loop_end : 0;
}
