#pragma once
#include "engine/audio.h"
#include <SDL3/SDL.h>

// Buses SFX play on, each its own logical audio device on the default output,
// so they can be paused or have their volume changed apart from each other.
// Every bus has the global SFX volume, see `SFX_set_volume()`.
//
// ```
// enum sfx_bus {
//     sfx_bus_gameplay,
//     sfx_bus_gui,
// };
// ```
typedef enum {
    sfx_bus_gameplay,
    sfx_bus_gui,
} sfx_bus;
#define SFX_BUSES_TOTAL 2

// A voice plays one `audio_wav` at a time on its bus, so sounds that should
// overlap need a voice each. Voices are on a pool, so their `voice_id` always
// points to them while they exist, and they're kept after their sound ends,
// ready to play again. Everything here is thread safe.



// Creates a voice on `bus`. Fills `voice_id` with its id, returning `0`.
int SFX_new_voice(sfx_bus bus, size_t* voice_id);

// Destroys the voice, cutting what it's playing. Returns `0` when successful,
// `1` if `voice_id` points to an empty slot or `3` if it's outside the pool's
// range.
int SFX_del_voice(size_t voice_id);

// Plays `wav` on the voice from its start, cutting whatever it was playing.
// `wav` must outlive the sound. Without an audio device, nothing is played.
// Returns `0` when successful, `1` if `voice_id` points to an empty slot or `3`
// if it's outside the pool's range.
int SFX_play_voice(size_t voice_id, const audio_wav* wav);

// Stops what the voice is playing. Returns `0` when successful, `1` if
// `voice_id` points to an empty slot or `3` if it's outside the pool's range.
int SFX_stop_voice(size_t voice_id);

// Fills `playing` with whether the voice is still playing. Returns `0` when
// successful, `1` if `voice_id` points to an empty slot or `3` if it's outside
// the pool's range.
int SFX_is_voice_playing(size_t voice_id, bool* playing);

// Sets the volume of the voice, `1.0f` being its sounds' own volume and `0.0f`
// silence, multiplied by the global SFX volume. Negative values are taken as
// `0.0f`. It's kept across plays and starts at `1.0f`. Returns `0` when
// successful, `1` if `voice_id` points to an empty slot or `3` if it's outside
// the pool's range.
int SFX_set_voice_volume(size_t voice_id, float volume);

// Fills `volume` with the voice's volume, see `SFX_set_voice_volume()`.
// Returns `0` when successful, `1` if `voice_id` points to an empty slot or `3`
// if it's outside the pool's range.
int SFX_get_voice_volume(size_t voice_id, float* volume);

// Pauses every voice on `bus`, keeping where they were. Voices played while
// it's paused start once it's resumed. Does nothing without an audio device.
void SFX_pause_bus(sfx_bus bus);

// Resumes every voice on `bus` from where it was paused.
void SFX_resume_bus(sfx_bus bus);

// Whether `bus` is paused. Always false without an audio device.
bool SFX_is_bus_paused(sfx_bus bus);

// Sets the volume of every SFX, `1.0f` being their own volume and `0.0f`
// silence. Negative values are taken as `0.0f`.
void SFX_set_volume(float volume);

// The volume of every SFX, see `SFX_set_volume()`.
float SFX_get_volume(void);



// Opens a logical audio device for each bus. If audio can't be started, it
// logs why and the game runs silent, with voices still working but not
// playing anything. Call on start of the game.
void SFX_start(void);

// Destroys every voice, closes the buses and quits audio. Call when quitting
// the game.
void SFX_stop(void);



// Meant for tests to check what SDL got. The game shouldn't need them.

// The stream of the voice, or `NULL` if there's none.
SDL_AudioStream* SFX_get_voice_stream(size_t voice_id);

// The logical device of `bus`, `0` without one.
SDL_AudioDeviceID SFX_get_bus_device(sfx_bus bus);
