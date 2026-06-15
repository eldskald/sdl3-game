#include <SDL3/SDL.h>
#include <chunks.h>
#include <data-structs/dynarr.h>
#include <data-structs/hashmap.h>
#include <defs.h>
#include <services/json.h>
#include <tilemap.h>

static chunk* chunks = NULL;

static material get_material_from_coords(Uint8 x) {
    if (x >= SPRITESHEET_DIRT_MIN_X && x <= SPRITESHEET_DIRT_MAX_X) return dirt;
    if (x >= SPRITESHEET_WOOD_MIN_X && x <= SPRITESHEET_WOOD_MAX_X) return wood;
    if (x >= SPRITESHEET_STONE_MIN_X && x <= SPRITESHEET_STONE_MAX_X)
        return stone;
    if (x >= SPRITESHEET_METAL_MIN_X && x <= SPRITESHEET_METAL_MAX_X)
        return metal;
    return null;
}

static chunk load_from_json(const char* json) {
    jsondata* data = JSON_parse(json);
    int w = (int)((jsondata*)get_from_hashmap("width", &data->obj))->num;
    int h = (int)((jsondata*)get_from_hashmap("height", &data->obj))->num;
    tile*** map = SDL_calloc(w * h, sizeof(tile));
    dynarr layers = ((jsondata*)get_from_hashmap("layers", &data->obj))->arr;
    dynarr tiles =
        ((jsondata*)get_from_hashmap("data", &((jsondata*)layers.at[0])->obj))
            ->arr;
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            int curr = (int)((jsondata*)tiles.at[i + w * j])->num;
            (*map)[i][j].coords_x = curr % (SPRITESHEET_X / SPRITESHEET_CELL_X);
            (*map)[i][j].coords_y = curr / (SPRITESHEET_X / SPRITESHEET_CELL_X);
            (*map)[i][j].mat = get_material_from_coords((*map)[i][j].coords_x);
        }
    }
    JSON_free(data);
    return (chunk){.w = w, .h = h, .map = map};
}

void CHUNKS_start(void) {
    chunks = SDL_calloc(CHUNK_TOTAL, sizeof(chunk));

    Uint8 mem001[] = {
#embed CHUNK_001_PATH
    };
    SDL_IOStream* stream = SDL_IOFromConstMem(mem001, sizeof(mem001));
    char json[sizeof(mem001) + 1] = "";
    SDL_ReadIO(stream, json, sizeof(mem001));
    SDL_CloseIO(stream);
    chunks[0] = load_from_json(json);
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
