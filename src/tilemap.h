#pragma once
#include <SDL3/SDL.h>

// Material types, for interactions, properties, etc.
//
// ```
// enum {tile_null, tile_dirt, tile_stone, tile_wood, tile_metal};
// ```
typedef enum {
    tile_null,
    tile_dirt,
    tile_stone,
    tile_wood,
    tile_metal,
} material;

// Tile information. The `coords_x` and `coords_y` mean this tile's sprite
// coords in the spritesheet in tiles, so if they're `0 0` it means the first
// tile, `0 1` means the tile to its right and so on. The `mat` is its material.
//
// ```
// struct tile {
//     material mat;
//     Uint8 coords_x;
//     Uint8 coords_y;
// };
// ```
typedef struct {
    material mat;
    Uint8 coords_x;
    Uint8 coords_y;
} tile;

// Sets `(*data)[w][h]` at coords `x`, `y` on the tilemap. Returns `0` when
// successful, `1` when `x,y,w,h` reach a tile outside the tilemap's range.
// Keep in mind the tilemap is a 2D array of tiles.
int TILEMAP_set_at(Uint16 x, Uint16 y, Uint16 w, Uint16 h, tile (*data)[w][h]);

// Gets the tile data at coords `x`, `y` on the tilemap. Returns `0` when
// successful, `1` when `x,y` is out of range. Keep in mind the tilemap is a 2D
// array of tiles.
int TILEMAP_get_at(Uint16 x, Uint16 y, tile* data);

// Draws the tilemap. Call on renderer update.
void TILEMAP_draw(SDL_Renderer* renderer, SDL_Texture* spritesheet);

// Clears the tilemap. Need to call this when quitting the game.
void TILEMAP_clear(void);
