#include <SDL3/SDL.h>
#include <data-structs/dynarr.h>
#include <data-structs/hashmap.h>
#include <defs.h>
#include <engine/chunks.h>
#include <engine/tilemap.h>
#include <services/json.h>

static chunk* chunks = NULL;

static material get_material_from_coords(Uint8 x) {
    if (x >= SPRITESHEET_DIRT_MIN_X && x <= SPRITESHEET_DIRT_MAX_X)
        return tile_dirt;
    if (x >= SPRITESHEET_WOOD_MIN_X && x <= SPRITESHEET_WOOD_MAX_X)
        return tile_wood;
    if (x >= SPRITESHEET_STONE_MIN_X && x <= SPRITESHEET_STONE_MAX_X)
        return tile_stone;
    if (x >= SPRITESHEET_METAL_MIN_X && x <= SPRITESHEET_METAL_MAX_X)
        return tile_metal;
    return tile_null;
}

static chunk load_from_json(const char* json) {
    jsondata* data = JSON_parse(json);
    int w = (int)((jsondata*)get_from_hashmap("width", &data->obj))->num;
    int h = (int)((jsondata*)get_from_hashmap("height", &data->obj))->num;
    tile(*map)[w][h] = SDL_calloc(w * h, sizeof(tile));
    dynarr layers = ((jsondata*)get_from_hashmap("layers", &data->obj))->arr;
    dynarr tiles =
        ((jsondata*)get_from_hashmap("data", &((jsondata*)layers.at[0])->obj))
            ->arr;
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            int curr = (int)((jsondata*)tiles.at[i + w * j])->num - 1;
            if (curr == -1) continue;
            (*map)[i][j].coords_x = curr % (SPRITESHEET_X / SPRITESHEET_CELL_X);
            (*map)[i][j].coords_y = curr / (SPRITESHEET_X / SPRITESHEET_CELL_X);
            (*map)[i][j].mat = get_material_from_coords((*map)[i][j].coords_x);
        }
    }
    JSON_free(data);
    return (chunk){.w = w, .h = h, .map = (tile***)map};
}

void CHUNKS_start(void) {
    chunks = SDL_calloc(CHUNK_TOTAL, sizeof(chunk));

#ifndef TEST
    Uint8 mem001[] = {
#embed CHUNK_001_PATH
    };
    SDL_IOStream* stream = SDL_IOFromConstMem(mem001, sizeof(mem001));
    char json[sizeof(mem001) + 1] = "";
    SDL_ReadIO(stream, json, sizeof(mem001));
    SDL_CloseIO(stream);
    chunks[0] = load_from_json(json);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Chunks loaded successfully.");
#else
    Uint8 mem001[] = {
#embed "tests/mocks/001.json"
    };
    SDL_IOStream* stream = SDL_IOFromConstMem(mem001, sizeof(mem001));
    char json[sizeof(mem001) + 1] = "";
    SDL_ReadIO(stream, json, sizeof(mem001));
    SDL_CloseIO(stream);
    chunks[0] = load_from_json(json);
#endif
}

void CHUNKS_stop(void) {
    for (int i = 0; i < CHUNK_TOTAL; i++) {
        SDL_free(chunks[i].map);
    }
    SDL_free(chunks);
}

int CHUNKS_get(size_t id, chunk* data) {
    if (id < 0 || id >= CHUNK_TOTAL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to access chunk id=%lu but this is outside the "
                    "chunk range",
                    id);
        return 1;
    }
    *data = chunks[id];
    return 0;
}
