#pragma once
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include <SDL3/SDL.h>

// Every background sound of the game. Each is an OGG Vorbis embedded on
// `src/engine/bgs.c`, on an `audio_bus` set there, looping between the
// `loop_start` and `loop_end` sample frames of its row on
// `BGS_TRACKS_CSV_PATH`, so an intro can play once before the part that loops.
// Its `stop_when_silent` there sets whether its volume reaching `0.0f` stops
// it, so it plays from its start next time, or it keeps playing silently,
// keeping its place. `bgs_track_none` is for no track, and every function here
// does nothing with it.
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
// multiplied by its bus's volume. Negative values are taken as `0.0f`, which
// stops it if it's set to stop when silent. Cancels any fade. It's kept across
// plays and starts at `1.0f`.
void BGS_set_track_volume(bgs_track track, float volume);

// The volume of `track`, mid fade included, see `BGS_set_track_volume()`.
float BGS_get_track_volume(bgs_track track);

// Fades the volume of `track` from what it is to `volume` on `BGS_update()`,
// changing it by `rate` per second, like `BGS_set_track_volume()` once it gets
// there. Calling it again mid fade retargets it from where it is, so it can be
// called every tick to smooth out a volume that jumps around. A rate of `0.0f`
// or less gets there on the next update. Negative volumes are taken as `0.0f`.
void BGS_fade_track(bgs_track track, float volume, float rate);

// Whether `track` stops once its volume reaches `0.0f`, from its row on
// `BGS_TRACKS_CSV_PATH`.
bool BGS_get_track_stops_when_silent(bgs_track track);

// Get a `string-->bgs_track` map, used to translate the track names written
// on CSV files into their corresponding `bgs_track`.
hashmap BGS_get_string_tracks_map(void);



// Opens a decoder and a stream on its bus for each track, with its loop points
// and whether it stops when silent from `BGS_TRACKS_CSV_PATH`. Tracks whose bus
// has no device, see `AUDIO_start()`, never play. Call on start of the game,
// after `AUDIO_start()`.
void BGS_start(void);

// Tops up every playing track's buffer and advances fades. Call every tick of
// the fixed thread.
void BGS_update(void);

// Destroys every track's stream and decoder. Call when quitting the game,
// before `AUDIO_stop()`.
void BGS_stop(void);



// Meant for tests to check what SDL and the CSV got. The game shouldn't need
// them.

// The stream of `track`, or `NULL` if there's none.
SDL_AudioStream* BGS_get_track_stream(bgs_track track);

// Fills `loop_start` and `loop_end` with the frames `track` loops between.
void BGS_get_track_loop(bgs_track track, Uint32* loop_start, Uint32* loop_end);
