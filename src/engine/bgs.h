#pragma once
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include <SDL3/SDL.h>

// Buses background sounds play on, each its own logical audio device on the
// default output, like the SFX buses, with its own volume.
//
// ```
// enum bgs_bus {
//     bgs_bus_music,
//     bgs_bus_ambience,
// };
// ```
typedef enum {
    bgs_bus_music,
    bgs_bus_ambience,
} bgs_bus;
#define BGS_BUSES_TOTAL 2

// Every background sound of the game. Each is an OGG Vorbis embedded on
// `src/engine/bgs.c`, on a bus set there, looping between the `loop_start` and
// `loop_end` sample frames of its row on `BGS_TRACKS_CSV_PATH`, so an intro can
// play once before the part that loops. `bgs_track_none` is for no track, and
// every function here does nothing with it.
//
// ```
// enum bgs_track { bgs_track_none, BGS_TRACKS };
// ```
typedef enum { bgs_track_none, BGS_TRACKS } bgs_track;

// Each track decodes into its own stream, so tracks can play at the same
// time, for crossfades and such. Nothing is decoded on SDL's audio thread.
// `BGS_update()` keeps each playing track's stream `BGS_BUFFER_MS` ahead of
// what's been heard, decoding at most `BGS_MAX_FILL_MS` of it per track per
// call, so a starting track fills its buffer over a few ticks and a stall
// shorter than the buffer isn't heard. Volumes and stopping take effect right
// away, on what's already buffered. Everything here is thread safe.



// Plays `track` from its start, cutting it and any fade if it was already
// playing. It starts being heard on the next `BGS_update()`.
void BGS_play_track(bgs_track track);

// Stops `track`, cutting what's buffered and any fade.
void BGS_stop_track(bgs_track track);

// Whether `track` is playing.
bool BGS_is_track_playing(bgs_track track);

// Every track that's playing, a `bgs_track` dynarr. Deinit it when done.
dynarr BGS_get_playing_tracks(void);

// Sets the volume of `track`, `1.0f` being its own volume and `0.0f` silence,
// multiplied by its bus's volume. Negative values are taken as `0.0f`. Cancels
// any fade. It's kept across plays and starts at `1.0f`.
void BGS_set_track_volume(bgs_track track, float volume);

// The volume of `track`, mid fade included, see `BGS_set_track_volume()`.
float BGS_get_track_volume(bgs_track track);

// Fades the volume of `track` from what it is to `volume` over `seconds`, on
// `BGS_update()`, stopping it once it gets there when `stop_at_end` is set,
// for fading out. A fade of `0.0` seconds gets there on the next update.
// Negative volumes are taken as `0.0f`.
void BGS_fade_track(bgs_track track,
                    float volume,
                    double seconds,
                    bool stop_at_end);

// Pauses every track on `bus`, keeping where they were. Does nothing without
// an audio device.
void BGS_pause_bus(bgs_bus bus);

// Resumes every track on `bus` from where it was paused.
void BGS_resume_bus(bgs_bus bus);

// Whether `bus` is paused. Always false without an audio device.
bool BGS_is_bus_paused(bgs_bus bus);

// Sets the volume of every track on `bus`, `1.0f` being their own volume and
// `0.0f` silence. Negative values are taken as `0.0f`.
void BGS_set_bus_volume(bgs_bus bus, float volume);

// The volume of `bus`, see `BGS_set_bus_volume()`.
float BGS_get_bus_volume(bgs_bus bus);

// Get a `string-->bgs_track` map, used to translate the track names written
// on CSV files into their corresponding `bgs_track`.
hashmap BGS_get_string_tracks_map(void);



// Opens a logical audio device for each bus and a decoder for each track,
// with its loop points from `BGS_TRACKS_CSV_PATH`. If audio can't be started,
// it logs why and the game runs without background sounds, with tracks never
// playing. Call on start of the game.
void BGS_start(void);

// Tops up every playing track's buffer and advances fades. Call every tick of
// the fixed thread.
void BGS_update(void);

// Destroys every track's decoder, closes the buses and quits audio. Call when
// quitting the game.
void BGS_stop(void);



// Meant for tests to check what SDL and the CSV got. The game shouldn't need
// them.

// The stream of `track`, or `NULL` if there's none.
SDL_AudioStream* BGS_get_track_stream(bgs_track track);

// The logical device of `bus`, `0` without one.
SDL_AudioDeviceID BGS_get_bus_device(bgs_bus bus);

// Fills `loop_start` and `loop_end` with the frames `track` loops between.
void BGS_get_track_loop(bgs_track track, Uint32* loop_start, Uint32* loop_end);
