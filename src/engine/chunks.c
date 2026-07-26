#include "engine/chunks.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "services/json.h"
#include <SDL3/SDL.h>

static chunk chunks[CHUNKS_TOTAL] = {0};

static material get_material_from_coords(Uint8 x) {
    if (x >= SPRITESHEET_DIRT_MIN_X && x <= SPRITESHEET_DIRT_MAX_X)
        return tile_dirt;
    if (x >= SPRITESHEET_WOOD_MIN_X && x <= SPRITESHEET_WOOD_MAX_X)
        return tile_wood;
    if (x >= SPRITESHEET_STONE_MIN_X && x <= SPRITESHEET_STONE_MAX_X)
        return tile_stone;
    if (x >= SPRITESHEET_METAL_MIN_X && x <= SPRITESHEET_METAL_MAX_X)
        return tile_metal;
    return 0;
}

static chunk load_from_json(const char* json) {
    jsondata* data = JSON_parse(json);
    int w = (int)((jsondata*)get_from_hashmap("width", &data->obj))->num;
    int h = (int)((jsondata*)get_from_hashmap("height", &data->obj))->num;
    tile(*map)[w][h] = SDL_calloc(w * h, sizeof(tile));
    dynarr fluids = (dynarr){0};
    init_dynarr(&fluids);

    dynarr layers = ((jsondata*)get_from_hashmap("layers", &data->obj))->arr;

    // Parse each layer
    for (int idx = 0; idx < layers.len; idx++) {
        hashmap layer = ((jsondata*)layers.at[idx])->obj;
        char* name = ((jsondata*)get_from_hashmap("name", &layer))->str;
        char* type = ((jsondata*)get_from_hashmap("type", &layer))->str;

        // If layer is main tilemap
        if (SDL_strcmp(name, "Main") == 0 &&
            SDL_strcmp(type, "tilelayer") == 0) {
            dynarr tiles = ((jsondata*)get_from_hashmap("data", &layer))->arr;
            for (int i = 0; i < w; i++) {
                for (int j = 0; j < h; j++) {
                    int curr = (int)((jsondata*)tiles.at[i + w * j])->num - 1;
                    if (curr == -1) continue;
                    (*map)[i][j].coords_x =
                        curr % (SPRITESHEET_X / SPRITESHEET_CELL_X);
                    (*map)[i][j].coords_y =
                        curr / (SPRITESHEET_X / SPRITESHEET_CELL_X);
                    (*map)[i][j].mat =
                        get_material_from_coords((*map)[i][j].coords_x);
                }
            }
        }

        // If layer is a water layer
        if (SDL_strcmp(name, "Water") == 0 &&
            SDL_strcmp(type, "objectgroup") == 0) {
            dynarr objects =
                ((jsondata*)get_from_hashmap("objects", &layer))->arr;
            for (int i = 0; i < objects.len; i++) {
                hashmap obj = ((jsondata*)objects.at[i])->obj;
                double x = ((jsondata*)get_from_hashmap("x", &obj))->num;
                double y = ((jsondata*)get_from_hashmap("y", &obj))->num;
                double w = ((jsondata*)get_from_hashmap("width", &obj))->num;
                double h = ((jsondata*)get_from_hashmap("height", &obj))->num;
                fluid_pool* pool = SDL_malloc(sizeof(fluid_pool));
                *pool = (fluid_pool){.x = x / FLUIDS_CELL_W,
                                     .y = y / FLUIDS_CELL_H,
                                     .w = w / FLUIDS_CELL_W,
                                     .h = h / FLUIDS_CELL_H,
                                     .type = fluid_water};
                push_to_dynarr((void*)pool, &fluids);
            }
        }

        // If layer is a lava layer
        if (SDL_strcmp(name, "Lava") == 0 &&
            SDL_strcmp(type, "objectgroup") == 0) {
            dynarr objects =
                ((jsondata*)get_from_hashmap("objects", &layer))->arr;
            for (int i = 0; i < objects.len; i++) {
                hashmap obj = ((jsondata*)objects.at[i])->obj;
                double x = ((jsondata*)get_from_hashmap("x", &obj))->num;
                double y = ((jsondata*)get_from_hashmap("y", &obj))->num;
                double w = ((jsondata*)get_from_hashmap("width", &obj))->num;
                double h = ((jsondata*)get_from_hashmap("height", &obj))->num;
                fluid_pool* pool = SDL_malloc(sizeof(fluid_pool));
                *pool = (fluid_pool){.x = x / FLUIDS_CELL_W,
                                     .y = y / FLUIDS_CELL_H,
                                     .w = w / FLUIDS_CELL_W,
                                     .h = h / FLUIDS_CELL_H,
                                     .type = fluid_lava};
                push_to_dynarr((void*)pool, &fluids);
            }
        }

    }

    JSON_free(data);
    return (chunk){.w = w, .h = h, .map = (tile***)map, .fluids = fluids};
}

void CHUNKS_start(void) {
#ifndef TEST
    char chunk001[] = {
#embed CHUNK_001_PATH suffix(, '\0')
    };
    chunks[0] = load_from_json(chunk001);

    char chunk002[] = {
#embed CHUNK_002_PATH suffix(, '\0')
    };
    chunks[1] = load_from_json(chunk002);

    char chunk003[] = {
#embed CHUNK_003_PATH suffix(, '\0')
    };
    chunks[2] = load_from_json(chunk003);

    char chunk004[] = {
#embed CHUNK_004_PATH suffix(, '\0')
    };
    chunks[3] = load_from_json(chunk004);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Chunks loaded successfully.");
#else
    char chunk001[] = {
#embed "tests/mocks/001.json" suffix(, '\0')
    };
    chunks[0] = load_from_json(chunk001);
#endif
}

void CHUNKS_stop(void) {
    for (int i = 0; i < CHUNKS_TOTAL; i++) {
        SDL_free(chunks[i].map);
        for (int k = 0; k < chunks[i].fluids.len; k++)
            SDL_free(chunks[i].fluids.at[k]);
        clear_dynarr(&chunks[i].fluids);
    }
}

int CHUNKS_get(size_t id, chunk* data) {
    if (id < 0 || id >= CHUNKS_TOTAL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to access chunk id=%lu but this is outside the "
                    "chunk range",
                    id);
        return 1;
    }
    *data = chunks[id];
    return 0;
}

int CHUNKS_load_at(size_t id, Uint16 x, Uint16 y) {
    if (id < 0 || id >= CHUNKS_TOTAL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to access chunk id=%lu but this is outside the "
                    "chunk range",
                    id);
        return 1;
    }
    if (x + chunks[id].w >= TILEMAP_SIZE_X ||
        y + chunks[id].h >= TILEMAP_SIZE_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to load chunk id=%lu at (%d,%d) but it would loud "
                    "outside the tilemap's tile range",
                    id,
                    x,
                    y);
        return 2;
    }
    chunk c = chunks[id];
    TILEMAP_set_at(0, 0, c.w, c.h, (tile(*)[c.w][c.h])c.map);
    for (size_t i = 0; i < c.fluids.len; i++) {
        fluid_pool* p = c.fluids.at[i];
        FLUIDS_set_at(p->x, p->y, p->w, p->h, p->type, 1.0f);
    }
    return 0;
}
