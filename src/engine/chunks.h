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

// A vine to be created when the chunk is loaded, with `segments` segments laid
// along the path that goes down from `(a_x,a_y)` to `bend_y`, sideways to
// `b_x` and then up to `(b_x,b_y)`, in pixel coordinates in the chunk. Vines
// that don't bend have `b_x == a_x` and `bend_y == b_y`, and ones that don't go
// up after bending have `bend_y == b_y`. It's pinned at `(a_x,a_y)` and, if
// `double_pinned`, also at `(b_x,b_y)`.
//
// ```
// struct vine_spawn {
//     Uint16 a_x, a_y;
//     Uint16 b_x, b_y;
//     Uint16 bend_y;
//     Uint8 segments;
//     bool double_pinned;
// }
// ```
typedef struct {
    Uint16 a_x, a_y;
    Uint16 b_x, b_y;
    Uint16 bend_y;
    Uint8 segments;
    bool double_pinned;
} vine_spawn;

// A chunk is just a block of tiles of size `(w, h)` contained on map, which
// should be interpreted as a tile `tile(*)[w][h]`. All chunks are embedded from
// `data/chunks` and kept in memory in a static array.
//
// ```
// struct chunk {
//     int w;
//     int h;
//     tile*** map; // cast it to tile(*)[w][h]
//     dynarr fluids; // fluid_pool dynarr
//     dynarr vines; // vine_spawn dynarr
// }
// ```
typedef struct {
    int w;
    int h;
    tile*** map;
    dynarr fluids;
    dynarr vines;
} chunk;

// Load all chunk files and allocates memory for them. Call at start.
void CHUNKS_start(void);

// Frees all the memory allocated at start. Call when quitting the game.
void CHUNKS_stop(void);

// Gets the chunk at index `id` from the array and puts it on data. Returns `0`
// when successful and `1` if `id` is outside the array range.
int CHUNKS_get(size_t id, chunk* data);

// Loads the chunk in the map at tilemap coordinates `x` and `y`, along with its
// fluids and vines. Take into account the chunk's `h` and `w` values, so the
// passed `x` and `y` values must be such that the loaded chunk fits in the
// tilemap. Returns `0` when successful, `1` when `id` is outside the array
// range and `2` when the chunk can't fit the tilemap at that position.
int CHUNKS_load_at(size_t id, Uint16 x, Uint16 y);
