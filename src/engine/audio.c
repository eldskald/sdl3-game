#include "engine/audio.h"
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
