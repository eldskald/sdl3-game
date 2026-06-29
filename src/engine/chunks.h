#pragma once
#include <engine/tilemap.h>

// A chunk is just a block of tiles of size `(w, h)` contained on map, which
// should be interpreted as a tile `tile(*)[w][h]`. All chunks are embedded from
// `data/chunks` and kept in memory in a static array.
//
// ```
// struct chunk {
//     int w;
//     int h;
//     tile*** map; // cast it to tile(*)[w][h]
// }
// ```
typedef struct {
    int w;
    int h;
    tile*** map;
} chunk;

// Load all chunk files and allocates memory for them. Call at start.
void CHUNKS_start(void);

// Frees all the memory allocated at start. Call when quitting the game.
void CHUNKS_stop(void);

// Gets the chunk at index `id` from the array and puts it on data. Returns `0`
// when successful and `1` if `id` is outside the array range.
int CHUNKS_get(size_t id, chunk* data);
