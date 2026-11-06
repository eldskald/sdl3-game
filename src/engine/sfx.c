#include "engine/sfx.h"
#include "data-structs/pool.h"
#include "defs.h"
#include "engine/audio.h"
#include "utils/logger.h"
#include <SDL3/SDL.h>

typedef struct {
    SDL_AudioStream* stream;
    sfx_bus bus;
    float volume;
} voice;

static pool voices = (pool){0};
static SDL_Mutex* pool_mutex = NULL;
static SDL_AudioDeviceID devices[SFX_BUSES_TOTAL] = {0};
static bool audio_started = false;
static float volume = SFX_STARTING_VOLUME;

static voice* get_voice(size_t id) {
    return pool_at(id, &voices);
}

static int lock_voice(size_t id, const char* action, voice** v) {
    SDL_LockMutex(pool_mutex);
    if (id >= voices.cap) {
        SDL_UnlockMutex(pool_mutex);
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                    "Tried to %s voice_id=%lu but this is outside the buffer "
                    "range",
                    action,
                    id);
        return 3;
    }
    *v = get_voice(id);
    if (!*v) {
        SDL_UnlockMutex(pool_mutex);
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                    "Tried to %s inexistent voice at voice_id=%lu",
                    action,
                    id);
        return 1;
    }
    return 0;
}



int SFX_new_voice(sfx_bus bus, size_t* voice_id) {
    voice v = (voice){
        .stream = SDL_CreateAudioStream(
            &(SDL_AudioSpec){SDL_AUDIO_S16, 2, 44100}, NULL),
        .bus = bus,
        .volume = 1.0f,
    };
    if (v.stream && devices[bus]) SDL_BindAudioStream(devices[bus], v.stream);
    SDL_LockMutex(pool_mutex);
    *voice_id = add_to_pool(&v, &voices);
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

int SFX_del_voice(size_t voice_id) {
    voice* v = NULL;
    int err = lock_voice(voice_id, "delete", &v);
    if (err) return err;
    SDL_DestroyAudioStream(v->stream);
    remove_from_pool(voice_id, &voices);
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

int SFX_play_voice(size_t voice_id, const audio_wav* wav) {
    voice* v = NULL;
    int err = lock_voice(voice_id, "play", &v);
    if (err) return err;
    if (v->stream && devices[v->bus]) {
        SDL_ClearAudioStream(v->stream);
        SDL_SetAudioStreamFormat(v->stream, &wav->spec, NULL);
        SDL_PutAudioStreamData(v->stream, wav->buffer, (int)wav->len);
    }
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

int SFX_stop_voice(size_t voice_id) {
    voice* v = NULL;
    int err = lock_voice(voice_id, "stop", &v);
    if (err) return err;
    SDL_ClearAudioStream(v->stream);
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

int SFX_is_voice_playing(size_t voice_id, bool* playing) {
    voice* v = NULL;
    int err = lock_voice(voice_id, "check", &v);
    if (err) return err;
    *playing = SDL_GetAudioStreamQueued(v->stream) > 0;
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

int SFX_set_voice_volume(size_t voice_id, float vol) {
    voice* v = NULL;
    int err = lock_voice(voice_id, "set volume of", &v);
    if (err) return err;
    v->volume = vol > 0.0f ? vol : 0.0f;
    SDL_SetAudioStreamGain(v->stream, v->volume);
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

int SFX_get_voice_volume(size_t voice_id, float* vol) {
    voice* v = NULL;
    int err = lock_voice(voice_id, "get volume of", &v);
    if (err) return err;
    *vol = v->volume;
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

void SFX_pause_bus(sfx_bus bus) {
    if (devices[bus]) SDL_PauseAudioDevice(devices[bus]);
}

void SFX_resume_bus(sfx_bus bus) {
    if (devices[bus]) SDL_ResumeAudioDevice(devices[bus]);
}

bool SFX_is_bus_paused(sfx_bus bus) {
    return devices[bus] && SDL_AudioDevicePaused(devices[bus]);
}

void SFX_set_volume(float vol) {
    volume = vol > 0.0f ? vol : 0.0f;
    for (int b = 0; b < SFX_BUSES_TOTAL; b++)
        if (devices[b]) SDL_SetAudioDeviceGain(devices[b], volume);
}

float SFX_get_volume(void) {
    return volume;
}



void SFX_start(void) {
    init_pool_with(&voices, sizeof(voice), SFX_STARTING_POOL_CAP);
    pool_mutex = SDL_CreateMutex();
    log_creation(SDL_LOG_CATEGORY_AUDIO, pool_mutex, "sfx pool mutex");

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Couldn't init audio, SFX will be silent: %s",
                     SDL_GetError());
        return;
    }
    audio_started = true;
    const char* names[SFX_BUSES_TOTAL] = {"gameplay", "gui"};
    for (int b = 0; b < SFX_BUSES_TOTAL; b++) {
        devices[b] =
            SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
        if (log_creation(SDL_LOG_CATEGORY_AUDIO,
                         devices[b] ? &devices[b] : NULL,
                         "sfx bus %s",
                         names[b]))
            SDL_SetAudioDeviceGain(devices[b], volume);
    }
}

void SFX_stop(void) {
    for (size_t i = 0; i < voices.cap; i++)
        if (get_voice(i)) SDL_DestroyAudioStream(get_voice(i)->stream);
    deinit_pool(&voices);
    for (int b = 0; b < SFX_BUSES_TOTAL; b++) {
        if (devices[b]) SDL_CloseAudioDevice(devices[b]);
        devices[b] = 0;
    }
    SDL_DestroyMutex(pool_mutex);
    pool_mutex = NULL;
    if (audio_started) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audio_started = false;
}



SDL_AudioStream* SFX_get_voice_stream(size_t voice_id) {
    SDL_LockMutex(pool_mutex);
    voice* v = voice_id < voices.cap ? get_voice(voice_id) : NULL;
    SDL_AudioStream* stream = v ? v->stream : NULL;
    SDL_UnlockMutex(pool_mutex);
    return stream;
}

SDL_AudioDeviceID SFX_get_bus_device(sfx_bus bus) {
    return devices[bus];
}
