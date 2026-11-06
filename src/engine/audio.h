#pragma once
#include <SDL3/SDL.h>

// A WAV fully loaded to PCM, played straight from `buffer`.
//
// ```
// struct audio_wav {
//     SDL_AudioSpec spec;
//     Uint8* buffer;
//     Uint32 len;
// };
// ```
typedef struct {
    SDL_AudioSpec spec;
    Uint8* buffer;
    Uint32 len;
} audio_wav;

// An OGG Vorbis kept compressed in memory and decoded in chunks as it plays.
// `vorbis` decodes from `data`, which is its own copy of the file when `owned`,
// so what it was created from can be freed right after, otherwise it's memory
// that outlives it, like an embedded array. `spec` is always `SDL_AUDIO_F32`,
// at the file's channels and frequency. `frames` is its length in sample
// frames, one sample per channel.
//
// A decoder is a single position on the file, so each `audio_ogg` is meant to
// be played by one stream at a time, and decoded from one thread at a time.
//
// ```
// struct audio_ogg {
//     SDL_AudioSpec spec;
//     const Uint8* data;
//     size_t size;
//     bool owned;
//     struct stb_vorbis* vorbis;
//     Uint32 frames;
// };
// ```
typedef struct {
    SDL_AudioSpec spec;
    const Uint8* data;
    size_t size;
    bool owned;
    struct stb_vorbis* vorbis;
    Uint32 frames;
} audio_ogg;



// Creates a WAV from `stream`, closing it when `closeio` is set, even on
// errors. `name` is for logging. Returns `NULL` on errors.
audio_wav*
AUDIO_create_wav(const char* name, SDL_IOStream* stream, bool closeio);

// Destroys a WAV from `AUDIO_create_wav()`. Accepts `NULL`.
void AUDIO_destroy_wav(audio_wav* wav);

// Creates an OGG Vorbis from `stream`, closing it when `closeio` is set, even
// on errors. Its decoder starts at its beginning. `name` is for logging.
// Returns `NULL` on errors.
audio_ogg*
AUDIO_create_ogg(const char* name, SDL_IOStream* stream, bool closeio);

// Creates an OGG Vorbis decoding straight from the `size` bytes at `data`,
// without copying them, so they must outlive it, like a `static const` array
// they're embedded on. Its decoder starts at its beginning. `name` is for
// logging. Returns `NULL` on errors.
audio_ogg*
AUDIO_create_ogg_from_mem(const char* name, const void* data, size_t size);

// Destroys an OGG Vorbis from `AUDIO_create_ogg...()`, freeing its data only
// if it's its own copy. Accepts `NULL`.
void AUDIO_destroy_ogg(audio_ogg* ogg);

// Decodes up to `frames` sample frames of `ogg` from its decoder's position
// into `out`, interleaved, which must fit `frames * ogg->spec.channels`
// floats, moving the position past them. Returns how many frames were
// decoded, fewer than `frames` only when it reaches the end, 0 once it's
// there.
int AUDIO_decode_ogg(audio_ogg* ogg, float* out, int frames);

// Moves the decoder of `ogg` to sample frame `frame`, 0 being its beginning,
// for rewinding and looping. Returns false on errors.
bool AUDIO_seek_ogg(audio_ogg* ogg, Uint32 frame);
