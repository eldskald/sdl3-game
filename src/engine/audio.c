#include "engine/audio.h"
#include "defs.h"
#include "utils/logger.h"
#include <SDL3/SDL.h>

// Warnings inside stb_vorbis aren't ours to fix, and some only show up with
// optimizations on.
#define STB_VORBIS_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "stb_vorbis.c"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static SDL_AudioDeviceID devices[AUDIO_BUSES_TOTAL] = {0};
static float bus_volumes[AUDIO_BUSES_TOTAL] = {
    [audio_bus_gui_sounds] = AUDIO_STARTING_VOLUME,
    [audio_bus_gameplay_sounds] = AUDIO_STARTING_VOLUME,
    [audio_bus_ambience_tracks] = AUDIO_STARTING_VOLUME,
    [audio_bus_music_tracks] = AUDIO_STARTING_VOLUME,
};
static float master_volume = 1.0f;
static bool audio_started = false;



audio_wav*
AUDIO_create_wav(const char* name, SDL_IOStream* stream, bool closeio) {
    SDL_AudioSpec spec;
    Uint8* buffer = NULL;
    Uint32 len = 0;
    bool loaded = SDL_LoadWAV_IO(stream, closeio, &spec, &buffer, &len);
    if (!log_creation(
            SDL_LOG_CATEGORY_AUDIO, loaded ? buffer : NULL, "wav %s", name))
        return NULL;

    audio_wav* wav = SDL_malloc(sizeof(audio_wav));
    *wav = (audio_wav){
        .spec = spec,
        .buffer = buffer,
        .len = len,
    };
    return wav;
}

void AUDIO_destroy_wav(audio_wav* wav) {
    if (!wav) return;
    SDL_free(wav->buffer);
    SDL_free(wav);
}

static audio_ogg*
open_ogg(const char* name, const Uint8* data, size_t size, bool owned) {
    int err = 0;
    stb_vorbis* vorbis = NULL;
    if (size <= SDL_MAX_SINT32)
        vorbis = stb_vorbis_open_memory(data, (int)size, &err, NULL);
    else
        SDL_SetError("File is too large.");
    if (!vorbis && err) SDL_SetError("stb_vorbis error %d.", err);
    if (!log_creation(SDL_LOG_CATEGORY_AUDIO, vorbis, "ogg %s", name))
        return NULL;

    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    audio_ogg* ogg = SDL_malloc(sizeof(audio_ogg));
    *ogg = (audio_ogg){
        .spec =
            {
                .format = SDL_AUDIO_F32,
                .channels = info.channels,
                .freq = (int)info.sample_rate,
            },
        .data = data,
        .size = size,
        .owned = owned,
        .vorbis = vorbis,
        .frames = stb_vorbis_stream_length_in_samples(vorbis),
    };
    return ogg;
}

audio_ogg*
AUDIO_create_ogg(const char* name, SDL_IOStream* stream, bool closeio) {
    size_t size = 0;
    Uint8* data = SDL_LoadFile_IO(stream, &size, closeio);
    if (!log_creation(SDL_LOG_CATEGORY_AUDIO, data, "ogg %s data", name))
        return NULL;
    audio_ogg* ogg = open_ogg(name, data, size, true);
    if (!ogg) SDL_free(data);
    return ogg;
}

audio_ogg*
AUDIO_create_ogg_from_mem(const char* name, const void* data, size_t size) {
    return open_ogg(name, data, size, false);
}

void AUDIO_destroy_ogg(audio_ogg* ogg) {
    if (!ogg) return;
    stb_vorbis_close(ogg->vorbis);
    if (ogg->owned) SDL_free((void*)ogg->data);
    SDL_free(ogg);
}

int AUDIO_decode_ogg(audio_ogg* ogg, float* out, int frames) {
    int channels = ogg->spec.channels;
    return stb_vorbis_get_samples_float_interleaved(
        ogg->vorbis, channels, out, frames * channels);
}

bool AUDIO_seek_ogg(audio_ogg* ogg, Uint32 frame) {
    if (stb_vorbis_seek(ogg->vorbis, frame)) return true;
    SDL_SetError("stb_vorbis error %d.", stb_vorbis_get_error(ogg->vorbis));
    return false;
}



void AUDIO_pause_bus(audio_bus bus) {
    if (devices[bus]) SDL_PauseAudioDevice(devices[bus]);
}

void AUDIO_resume_bus(audio_bus bus) {
    if (devices[bus]) SDL_ResumeAudioDevice(devices[bus]);
}

bool AUDIO_is_bus_paused(audio_bus bus) {
    return devices[bus] && SDL_AudioDevicePaused(devices[bus]);
}

void AUDIO_set_bus_volume(audio_bus bus, float volume) {
    bus_volumes[bus] = volume > 0.0f ? volume : 0.0f;
    if (devices[bus])
        SDL_SetAudioDeviceGain(devices[bus], bus_volumes[bus] * master_volume);
}

float AUDIO_get_bus_volume(audio_bus bus) {
    return bus_volumes[bus];
}

void AUDIO_set_master_volume(float volume) {
    master_volume = volume > 0.0f ? volume : 0.0f;
    for (int b = 0; b < AUDIO_BUSES_TOTAL; b++)
        if (devices[b])
            SDL_SetAudioDeviceGain(devices[b], bus_volumes[b] * master_volume);
}

float AUDIO_get_master_volume(void) {
    return master_volume;
}

SDL_AudioDeviceID AUDIO_get_bus_device(audio_bus bus) {
    return devices[bus];
}



void AUDIO_start(void) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Couldn't init audio, the game will be silent: %s",
                     SDL_GetError());
        return;
    }
    audio_started = true;
    const char* names[AUDIO_BUSES_TOTAL] = {
        [audio_bus_gui_sounds] = "gui sounds",
        [audio_bus_gameplay_sounds] = "gameplay sounds",
        [audio_bus_ambience_tracks] = "ambience tracks",
        [audio_bus_music_tracks] = "music tracks",
    };
    for (int b = 0; b < AUDIO_BUSES_TOTAL; b++) {
        devices[b] =
            SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
        if (log_creation(SDL_LOG_CATEGORY_AUDIO,
                         devices[b] ? &devices[b] : NULL,
                         "%s bus",
                         names[b]))
            SDL_SetAudioDeviceGain(devices[b], bus_volumes[b] * master_volume);
    }
}

void AUDIO_stop(void) {
    for (int b = 0; b < AUDIO_BUSES_TOTAL; b++) {
        if (devices[b]) SDL_CloseAudioDevice(devices[b]);
        devices[b] = 0;
    }
    if (audio_started) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audio_started = false;
}
