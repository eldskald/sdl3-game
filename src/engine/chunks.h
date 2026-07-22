#pragma once
#include "data-structs/dynarr.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"

// Data about a pool of `type` fluid contained in the rect `(x,y,w,h)` in tile
// coordinates in the chunk.
//
// ```
// struct fluid_pool {
//     Uint16 x;
//     Uint16 y;
//     Uint16 w;
//     Uint16 h;
//     fluid_type type;
// }
// ```
typedef struct {
    Uint16 x;
    Uint16 y;
    Uint16 w;
    Uint16 h;
    fluid_type type;
} fluid_pool;

// A chunk is just a block of tiles of size `(w, h)` contained on map, which
// should be interpreted as a tile `tile(*)[w][h]`. All chunks are embedded from
// `data/chunks` and kept in memory in a static array.
//
// ```
// struct chunk {
//     int w;
//     int h;
//     tile*** map; // cast it to tile(*)[w][h]
//     dynarr fluids; // fluid_pool* dynarr
// }
// ```
typedef struct {
    int w;
    int h;
    tile*** map;
    dynarr fluids;
} chunk;

// Load all chunk files and allocates memory for them. Call at start.
void CHUNKS_start(void);

// Frees all the memory allocated at start. Call when quitting the game.
void CHUNKS_stop(void);

// Gets the chunk at index `id` from the array and puts it on data. Returns `0`
// when successful and `1` if `id` is outside the array range.
int CHUNKS_get(size_t id, chunk* data);

// Loads the chunk in the map at tilemap coordinates `x` and `y`. Take into
// account the chunk's `h` and `w` values, so the passed `x` and `y` values must
// be such that the loaded chunk fits in the tilemap. Returns `0` when
// successful, `1` when `id` is outside the array range and `2` when the chunk
// can't fit the tilemap at that position.
int CHUNKS_load_at(size_t id, Uint16 x, Uint16 y);
