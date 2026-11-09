#pragma once
#include "engine/audio.h"
#include <SDL3/SDL.h>

// A voice plays one `audio_wav` at a time on its bus, see `audio_bus` on
// `engine/audio.h`, so sounds that should overlap need a voice each. Voices are
// on a pool, so their `voice_id` always points to them while they exist, and
// they're kept after their sound ends, ready to play again. Everything here is
// thread safe.



// Creates a voice on `bus`. Fills `voice_id` with its id, returning `0`, or
// returns `2` without creating it before `SFX_start()` or after `SFX_stop()`.
// Without the bus's device, see `AUDIO_start()`, it never plays anything.
int SFX_new_voice(audio_bus bus, size_t* voice_id);

// Destroys the voice, cutting what it's playing. Returns `0` when successful,
// `1` if `voice_id` points to an empty slot or `3` if it's outside the pool's
// range.
int SFX_del_voice(size_t voice_id);

// Plays `wav` on the voice from its start, cutting whatever it was playing.
// `wav` must outlive the sound. Without an audio device, nothing is played.
// Returns `0` when successful, `1` if `voice_id` points to an empty slot or `3`
// if it's outside the pool's range.
int SFX_play_voice(size_t voice_id, const audio_wav* wav);

// Plays `wav` on the voice like `SFX_play_voice()`, but as heard from the
// global position `(x,y)`, in pixels. Inside the camera rect it's played at
// `max_volume`, and outside it its volume goes down linearly with its distance
// to the rect, reaching silence at `falloff` pixels away. Sets the
// voice's volume to that before playing, see `SFX_set_voice_volume()`, and
// when it's silent, it doesn't play it, keeping whatever was playing. Returns
// the same as `SFX_play_voice()`.
int SFX_play_voice_2d(size_t voice_id,
                      const audio_wav* wav,
                      double x,
                      double y,
                      float max_volume,
                      double falloff);

// Stops what the voice is playing. Returns `0` when successful, `1` if
// `voice_id` points to an empty slot or `3` if it's outside the pool's range.
int SFX_stop_voice(size_t voice_id);

// Fills `playing` with whether the voice is still playing. Returns `0` when
// successful, `1` if `voice_id` points to an empty slot or `3` if it's outside
// the pool's range.
int SFX_is_voice_playing(size_t voice_id, bool* playing);

// Sets the volume of the voice, `1.0f` being its sounds' own volume and `0.0f`
// silence, multiplied by its bus's volume. Negative values are taken as
// `0.0f`. It's kept across plays and starts at `1.0f`. Returns `0` when
// successful, `1` if `voice_id` points to an empty slot or `3` if it's outside
// the pool's range.
int SFX_set_voice_volume(size_t voice_id, float volume);

// Fills `volume` with the voice's volume, see `SFX_set_voice_volume()`.
// Returns `0` when successful, `1` if `voice_id` points to an empty slot or `3`
// if it's outside the pool's range.
int SFX_get_voice_volume(size_t voice_id, float* volume);

// Sets up the voices' pool. Call on start of the game, after `AUDIO_start()`.
void SFX_start(void);

// Destroys every voice. Call when quitting the game, before `AUDIO_stop()`.
void SFX_stop(void);



// Meant for tests to check what SDL got. The game shouldn't need it.

// The stream of the voice, or `NULL` if there's none.
SDL_AudioStream* SFX_get_voice_stream(size_t voice_id);
