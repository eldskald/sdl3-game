#pragma once
#include "data-structs/hashmap.h"
#include "defs.h"
#include <SDL3/SDL.h>

// Material types, for interactions, properties, etc.
//
// ```
// enum { TILEMAP_TILE_TYPES };
// ```
typedef enum { TILEMAP_TILE_TYPES } tile_type;

// Tile information. The `coords_x` and `coords_y` mean this tile's sprite
// coords in the spritesheet in tiles, so if they're `0 0` it means the first
// tile, `0 1` means the tile to its right and so on. The `type` is its
// material.
//
// ```
// struct tile {
//     tile_type type;
//     bool platform;
//     Uint8 coords_x;
//     Uint8 coords_y;
// };
// ```
typedef struct {
    tile_type type;
    bool platform;
    Uint8 coords_x;
    Uint8 coords_y;
} tile;

// Loads the autotiles' first tile coords from `TILEMAP_AUTOTILES_CSV_PATH`.
// Call it when starting the game, before setting any tiles.
void TILEMAP_start(void);

// Clears the tilemap and frees memory allocated at `TILEMAP_start()`. Call it
// when quitting the game.
void TILEMAP_stop(void);

// Sets `(*data)[w][h]` at coords `x`, `y` on the tilemap. Returns `0` when
// successful, `1` when `x,y,w,h` reach a tile outside the tilemap's range.
// Keep in mind the tilemap is a 2D array of tiles.
int TILEMAP_set_at(Uint16 x, Uint16 y, Uint16 w, Uint16 h, tile (*data)[w][h]);

// Gets the tile data at coords `x`, `y` on the tilemap. Returns `0` when
// successful, `1` when `x,y` is out of range. Keep in mind the tilemap is a 2D
// array of tiles.
int TILEMAP_get_at(Uint16 x, Uint16 y, tile* data);

// Queues the tiles on screen on the renderer, platform tiles at
// `TILEMAP_PLATFORMS_Z_INDEX` and every other tile at `TILEMAP_Z_INDEX`. Call
// on renderer update.
void TILEMAP_draw(void);

// Clears the tilemap. Need to call this when quitting the game.
void TILEMAP_clear(void);



// Get a `string-->tile_type` map, used to translate the tile material names
// written on CSV files into their corresponding `fluid_type`. Needed mostly for
// reactions.
hashmap TILEMAP_get_string_tile_types_map(void);
