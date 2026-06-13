#pragma once

// Main settings
#define WINDOW_TITLE "Game"
#define WINDOW_SIZE_X 320
#define WINDOW_SIZE_Y 180

#define MAIN_T_TARGET_FPS 60
#define FIXED_T_TARGET_FPS 60

#define STARTING_WINDOW_SIZE_X 1280
#define STARTING_WINDOW_SIZE_Y 720
#define MIN_WINDOW_SIZE_X 640
#define MIN_WINDOW_SIZE_Y 360



// Renderer
#define RES_PATH "res/"
#define RES_TEXTURES_PATH RES_PATH "textures/"

#define SPRITESHEET_CELL_X 8
#define SPRITESHEET_CELL_Y 8

#define COL_BLACK 0, 0, 0, 255
#define COL_WHITE 255, 255, 255, 255



// Sprites
#define STARTING_SPRITES_DYNARR_CAP 1024
#define Z_INDEX_TOTAL 256 // z_index is Sint8, varies from -127 to 127



// Tilemaps
#define TILEMAP_SIZE_X 1024
#define TILEMAP_SIZE_Y 1024
