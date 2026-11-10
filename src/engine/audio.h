#pragma once
#include <SDL3/SDL.h>

// Buses everything is played on, each its own logical audio device on the
// default output, so they can be paused or have their volume changed apart from
// each other, like for a volume slider each. SFX voices and BGS tracks pick the
// bus they play on, so a sound belongs on the bus whose slider should control
// it, not the one of the system playing it.
//
// ```
// enum audio_bus {
//     audio_bus_gui_sounds,
//     audio_bus_gameplay_sounds,
//     audio_bus_ambience_tracks,
//     audio_bus_music_tracks,
// };
// ```
typedef enum {
    audio_bus_gui_sounds,
    audio_bus_gameplay_sounds,
    audio_bus_ambience_tracks,
    audio_bus_music_tracks,
} audio_bus;
#define AUDIO_BUSES_TOTAL 4

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



// Pauses everything on `bus`, keeping where it was. What's played while it's
// paused starts once it's resumed. Does nothing without an audio device.
void AUDIO_pause_bus(audio_bus bus);

// Resumes everything on `bus` from where it was paused.
void AUDIO_resume_bus(audio_bus bus);

// Whether `bus` is paused. Always false without an audio device.
bool AUDIO_is_bus_paused(audio_bus bus);

// Sets the volume of everything on `bus`, `1.0f` being their own volume and
// `0.0f` silence, scaled by the master volume. Negative values are taken as
// `0.0f`. It's kept across `AUDIO_stop()` and `AUDIO_start()`, and starts at
// `AUDIO_STARTING_VOLUME`.
void AUDIO_set_bus_volume(audio_bus bus, float volume);

// The volume of `bus`, see `AUDIO_set_bus_volume()`.
float AUDIO_get_bus_volume(audio_bus bus);

// Sets the volume every bus's volume is scaled by, `1.0f` leaving them as they
// are and `0.0f` muting everything. Negative values are taken as `0.0f`. It's
// kept across `AUDIO_stop()` and `AUDIO_start()`, and starts at `1.0f`.
void AUDIO_set_master_volume(float volume);

// The master volume, see `AUDIO_set_master_volume()`.
float AUDIO_get_master_volume(void);

// The logical device of `bus`, to bind streams to, `0` without one.
SDL_AudioDeviceID AUDIO_get_bus_device(audio_bus bus);



// Inits audio and opens a logical audio device for each bus. If audio can't be
// started, it logs why and the game runs silent, with buses having no device.
// Call on start of the game, before `SFX_start()` and `BGS_start()`.
void AUDIO_start(void);

// Closes the buses and quits audio. Call when quitting the game, after
// `SFX_stop()` and `BGS_stop()`, since destroying their streams waits for
// their devices to be done with them.
void AUDIO_stop(void);
