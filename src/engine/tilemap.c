#include <SDL3/SDL.h>
#include <defs.h>
#include <engine/tilemap.h>

#ifdef TEST
#include <stubs.h>
#endif

static const Uint8 DIRT_AUTOTILE[256][2] = {
    {1, 0},  {1, 0},  {2, 4},  {2, 4}, // 0
    {1, 0},  {1, 0},  {2, 4},  {2, 4},
    {3, 4},  {3, 4},  {1, 4},  {1, 2}, // 8
    {3, 4},  {3, 4},  {1, 4},  {1, 2},
    {2, 3},  {2, 3},  {0, 4},  {0, 4}, // 16
    {2, 3},  {2, 3},  {0, 2},  {0, 3},
    {3, 10}, {3, 10}, {1, 7},  {0, 6}, // 24
    {3, 10}, {3, 10}, {1, 6},  {2, 2},
    {1, 0},  {1, 0},  {2, 4},  {2, 4}, // 32
    {1, 0},  {1, 0},  {2, 4},  {2, 4},
    {3, 4},  {3, 4},  {1, 4},  {1, 2}, // 40
    {3, 4},  {3, 4},  {1, 4},  {1, 2},
    {2, 3},  {2, 3},  {0, 4},  {0, 4}, // 48
    {2, 3},  {2, 3},  {0, 3},  {0, 3},
    {3, 10}, {3, 10}, {1, 7},  {0, 6}, // 56
    {3, 10}, {3, 10}, {1, 6},  {2, 2},
    {3, 3},  {3, 3},  {3, 9},  {3, 9}, // 64
    {3, 3},  {3, 3},  {3, 9},  {3, 9},
    {1, 3},  {1, 3},  {0, 7},  {3, 5}, // 72
    {1, 3},  {1, 3},  {0, 7},  {3, 5},
    {0, 3},  {0, 3},  {1, 8},  {1, 8}, // 80
    {0, 3},  {0, 3},  {2, 5},  {2, 5},
    {0, 8},  {0, 8},  {3, 0},  {0, 11}, // 88
    {0, 8},  {0, 8},  {1, 11}, {2, 8},
    {3, 3},  {3, 3},  {3, 9},  {3, 9}, // 96
    {3, 3},  {3, 3},  {3, 9},  {3, 9},
    {1, 1},  {1, 1},  {3, 6},  {3, 2}, // 104
    {1, 1},  {1, 1},  {2, 9},  {3, 2},
    {0, 3},  {0, 3},  {1, 8},  {1, 8}, // 112
    {0, 3},  {0, 3},  {2, 5},  {2, 5},
    {0, 5},  {0, 5},  {2, 11}, {3, 8}, // 120
    {0, 5},  {0, 5},  {2, 9},  {2, 8},
    {1, 0},  {1, 0},  {2, 4},  {2, 4}, // 128
    {1, 0},  {1, 0},  {2, 4},  {2, 4},
    {3, 4},  {3, 4},  {1, 4},  {1, 2}, // 136
    {3, 4},  {3, 4},  {1, 4},  {1, 2},
    {2, 3},  {2, 3},  {0, 4},  {0, 4}, // 144
    {2, 3},  {2, 3},  {0, 2},  {0, 2},
    {3, 10}, {3, 10}, {1, 7},  {0, 6}, // 152
    {3, 10}, {3, 10}, {1, 6},  {2, 2},
    {1, 0},  {1, 0},  {2, 4},  {2, 4}, // 160
    {1, 0},  {1, 0},  {2, 4},  {2, 4},
    {3, 4},  {3, 4},  {1, 4},  {1, 2}, // 168
    {3, 4},  {3, 4},  {1, 4},  {1, 2},
    {2, 3},  {2, 3},  {0, 4},  {0, 4}, // 176
    {2, 3},  {2, 3},  {0, 2},  {0, 2},
    {3, 10}, {3, 10}, {1, 7},  {0, 6}, // 184
    {3, 10}, {3, 10}, {1, 6},  {2, 2},
    {3, 3},  {3, 3},  {3, 9},  {3, 9}, // 192
    {3, 3},  {3, 3},  {3, 9},  {3, 9},
    {1, 3},  {1, 3},  {0, 7},  {3, 5}, // 200
    {1, 3},  {1, 3},  {0, 7},  {3, 5},
    {0, 1},  {0, 1},  {2, 6},  {2, 6}, // 208
    {0, 1},  {0, 1},  {2, 1},  {2, 1},
    {1, 5},  {1, 5},  {3, 11}, {2, 10}, // 216
    {1, 5},  {1, 5},  {2, 7},  {1, 9},
    {3, 3},  {3, 3},  {3, 9},  {3, 9}, // 224
    {3, 3},  {3, 3},  {3, 9},  {3, 9},
    {1, 1},  {1, 1},  {3, 6},  {3, 2}, // 232
    {1, 1},  {1, 1},  {3, 6},  {3, 2},
    {0, 1},  {0, 1},  {2, 6},  {2, 6}, // 240
    {0, 1},  {0, 1},  {2, 1},  {2, 1},
    {3, 1},  {3, 1},  {3, 7},  {0, 10}, // 248
    {3, 1},  {3, 1},  {1, 10}, {2, 0},
};

static tile (*map)[TILEMAP_SIZE_X][TILEMAP_SIZE_Y] = NULL;


// These args must be int because they can be -1
static bool offset(int x, int y, int u, int v) {
    if (x + u < 0 || x + u >= TILEMAP_SIZE_X || y + v < 0 ||
        y + v >= TILEMAP_SIZE_Y)
        return true;
    return (*map)[x + u][y + v].mat == tile_dirt;
}

static Uint8 check_adjacent_dirt(Uint16 x, Uint16 y) {
    Uint8 val = 0;
    if (offset(x, y, -1, -1)) val += 1; ///////  1  2  4
    if (offset(x, y, 0, -1)) val += 2;  ///////  8     16
    if (offset(x, y, 1, -1)) val += 4;  ///////  32 64 128
    if (offset(x, y, -1, 0)) val += 8;
    if (offset(x, y, 1, 0)) val += 16;
    if (offset(x, y, -1, 1)) val += 32;
    if (offset(x, y, 0, 1)) val += 64;
    if (offset(x, y, 1, 1)) val += 128;
    return val;
}

static int update_dirt_tile(Uint16 x, Uint16 y, bool suppress_warnings) {
    if (x < 0 || y < 0 || x >= TILEMAP_SIZE_X || y >= TILEMAP_SIZE_Y) {
        if (!suppress_warnings)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Tried to update dirt tile at %d, %d, which is outside "
                        "map range",
                        x,
                        y);
        return 1;
    }
    if ((*map)[x][y].mat != tile_dirt) {
        if (!suppress_warnings)
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "Tried to update dirt tile at %d, %d, which is not dirt",
                x,
                y);
        return 2;
    }

    Uint8 val = check_adjacent_dirt(x, y);
    (*map)[x][y].coords_x = DIRT_AUTOTILE[val][0];
    (*map)[x][y].coords_y = DIRT_AUTOTILE[val][1];
    return 0;
}



int TILEMAP_set_at(Uint16 x, Uint16 y, Uint16 w, Uint16 h, tile (*data)[w][h]) {
    if (x < 0 || y < 0 || x + w >= TILEMAP_SIZE_X || y + h >= TILEMAP_SIZE_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set dataset of size %d, %d at %d, %d which is "
                    "out of range",
                    w,
                    h,
                    x,
                    y);
        return 1;
    }
    if (!map) {
        map = SDL_calloc(TILEMAP_SIZE_X * TILEMAP_SIZE_Y, sizeof(tile));
    }
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            (*map)[x + i][y + j] = (*data)[i][j];
        }
    }
    for (int i = -1; i <= w; i++) {
        for (int j = -1; j <= h; j++) {
            update_dirt_tile(x + i, y + j, true);
        }
    }
    return 0;
}

int TILEMAP_get_at(Uint16 x, Uint16 y, tile* data) {
    if (x < 0 || y < 0 || x >= TILEMAP_SIZE_X || y >= TILEMAP_SIZE_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get tile at %d, %d, which is outside map range",
                    x,
                    y);
        return 1;
    }
    *data = (*map)[x][y];
    return 0;
}

void TILEMAP_clear(void) {
    SDL_free(map);
    map = NULL;
}

void TILEMAP_draw(SDL_Renderer* renderer, SDL_Texture* spritesheet) {
    if (!map) return;

    for (int i = 0; i < TILEMAP_SIZE_X; i++) {
        for (int j = 0; j < TILEMAP_SIZE_Y; j++) {
            if ((*map)[i][j].mat == tile_null) continue;
            SDL_FRect srcrect = (SDL_FRect){
                .x = SPRITESHEET_CELL_X * (*map)[i][j].coords_x,
                .y = SPRITESHEET_CELL_Y * (*map)[i][j].coords_y,
                .w = SPRITESHEET_CELL_X,
                .h = SPRITESHEET_CELL_Y,
            };
            SDL_FRect dstrect = (SDL_FRect){
                .x = SPRITESHEET_CELL_X * i,
                .y = SPRITESHEET_CELL_Y * j,
                .w = SPRITESHEET_CELL_X,
                .h = SPRITESHEET_CELL_Y,
            };
#ifndef TEST
            SDL_RenderTexture(renderer, spritesheet, &srcrect, &dstrect);
#else
            stubbed_SDL_RenderTexture(&srcrect, &dstrect);
#endif
        }
    }
}
