#include "engine/sfx.h"
#include "data-structs/pool.h"
#include "defs.h"
#include "engine/audio.h"
#include "engine/camera.h"
#include "utils/logger.h"
#include <SDL3/SDL.h>

typedef struct {
    SDL_AudioStream* stream;
    audio_bus bus;
    float volume;
} voice;

static pool voices = (pool){0};
static SDL_Mutex* pool_mutex = NULL;

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



int SFX_new_voice(audio_bus bus, size_t* voice_id) {
    // The pool is only set up on `SFX_start()`
    if (!voices.data_size) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                    "Tried to create a voice before starting SFX");
        return 2;
    }
    voice v = (voice){
        .stream = SDL_CreateAudioStream(
            &(SDL_AudioSpec){SDL_AUDIO_S16, 2, 44100}, NULL),
        .bus = bus,
        .volume = 1.0f,
    };
    SDL_AudioDeviceID device = AUDIO_get_bus_device(bus);
    if (v.stream && device) SDL_BindAudioStream(device, v.stream);
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
    if (v->stream && AUDIO_get_bus_device(v->bus)) {
        SDL_ClearAudioStream(v->stream);
        SDL_SetAudioStreamFormat(v->stream, &wav->spec, NULL);
        SDL_PutAudioStreamData(v->stream, wav->buffer, (int)wav->len);
    }
    SDL_UnlockMutex(pool_mutex);
    return 0;
}

int SFX_play_voice_2d(size_t voice_id,
                      const audio_wav* wav,
                      double x,
                      double y,
                      float max_volume,
                      double falloff) {
    SDL_FRect cam = CAMERA_get_rect();
    double dx = SDL_max(SDL_max(cam.x - x, x - (cam.x + cam.w)), 0.0);
    double dy = SDL_max(SDL_max(cam.y - y, y - (cam.y + cam.h)), 0.0);
    double scale = 1.0 - SDL_sqrt(dx * dx + dy * dy) / falloff;
    float vol = max_volume * (float)SDL_max(scale, 0.0);
    int err = SFX_set_voice_volume(voice_id, vol);
    if (err || vol <= 0.0f) return err;
    return SFX_play_voice(voice_id, wav);
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

void SFX_start(void) {
    init_pool_with(&voices, sizeof(voice), SFX_STARTING_POOL_CAP);
    pool_mutex = SDL_CreateMutex();
    log_creation(SDL_LOG_CATEGORY_AUDIO, pool_mutex, "sfx pool mutex");
}

void SFX_stop(void) {
    for (size_t i = 0; i < voices.cap; i++)
        if (get_voice(i)) SDL_DestroyAudioStream(get_voice(i)->stream);
    deinit_pool(&voices);
    // So `SFX_new_voice()` sees it as not started again
    voices = (pool){0};
    SDL_DestroyMutex(pool_mutex);
    pool_mutex = NULL;
}



SDL_AudioStream* SFX_get_voice_stream(size_t voice_id) {
    SDL_LockMutex(pool_mutex);
    voice* v = voice_id < voices.cap ? get_voice(voice_id) : NULL;
    SDL_AudioStream* stream = v ? v->stream : NULL;
    SDL_UnlockMutex(pool_mutex);
    return stream;
}
