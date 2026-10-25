#include "engine/chunks.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include "services/json.h"
#include <SDL3/SDL.h>



typedef struct {
    Uint8 x;
    Uint8 y;
    Uint8 w;
    Uint8 h;
} platforms_rect;

typedef struct {
    Uint8 type;
    Uint8 x;
    Uint8 y;
    Uint8 w;
    Uint8 h;
} tile_type_rect;



static chunk chunks[CHUNKS_TOTAL] = {0};
static dynarr platforms = (dynarr){0};
static dynarr types_map = (dynarr){0};



static void* pack_platforms_rect(platforms_rect r) {
    void* p = NULL;
    SDL_memcpy(&p, &r, sizeof(r));
    return p;
}

static platforms_rect unpack_platforms_rect(void* p) {
    platforms_rect r;
    SDL_memcpy(&r, &p, sizeof(r));
    return r;
}

static void* pack_tile_type_rect(tile_type_rect r) {
    void* p = NULL;
    SDL_memcpy(&p, &r, sizeof(r));
    return p;
}

static tile_type_rect unpack_tile_type_rect(void* p) {
    tile_type_rect r;
    SDL_memcpy(&r, &p, sizeof(r));
    return r;
}

static Uint8 read_prop(dynarr* row, char* prop, hashmap* props) {
    return SDL_strtol(
        (char*)row->at[(size_t)get_from_hashmap(prop, props)], NULL, 10);
}

static void load_platforms_data(const char* csv) {
    dynarr parsed = CSV_parse(csv);

    // Get hashmap of property indexes
    hashmap props = (hashmap){0};
    size_t props_count = ((dynarr*)parsed.at[0])->len;
    init_hashmap(&props);
    for (size_t i = 0; i < props_count; i++)
        set_on_hashmap((void*)i, (char*)((dynarr*)parsed.at[0])->at[i], &props);

    // Set platforms data
    init_dynarr(&platforms);
    for (size_t i = 1; i < parsed.len; i++) {
        dynarr* row = (dynarr*)parsed.at[i];
        platforms_rect r = (platforms_rect){
            .x = read_prop(row, "x", &props),
            .y = read_prop(row, "y", &props),
            .w = read_prop(row, "w", &props),
            .h = read_prop(row, "h", &props),
        };
        push_to_dynarr(pack_platforms_rect(r), &platforms);
    }

    clear_hashmap(&props);
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Platforms data loaded successfully.");
}

static void load_types_map_data(const char* csv) {
    hashmap tiles_map = TILEMAP_get_string_tile_types_map();

    dynarr parsed = CSV_parse(csv);

    // Get hashmap of property indexes
    hashmap props = (hashmap){0};
    size_t props_count = ((dynarr*)parsed.at[0])->len;
    init_hashmap(&props);
    for (size_t i = 0; i < props_count; i++)
        set_on_hashmap((void*)i, (char*)((dynarr*)parsed.at[0])->at[i], &props);

    // Set types map data
    init_dynarr(&types_map);
    for (size_t i = 1; i < parsed.len; i++) {
        dynarr* row = (dynarr*)parsed.at[i];
        char* type = (char*)row->at[(size_t)get_from_hashmap("type", &props)];
        tile_type_rect r = (tile_type_rect){
            .type = (tile_type)(size_t)get_from_hashmap(type, &tiles_map),
            .x = read_prop(row, "x", &props),
            .y = read_prop(row, "y", &props),
            .w = read_prop(row, "w", &props),
            .h = read_prop(row, "h", &props),
        };
        push_to_dynarr(pack_tile_type_rect(r), &types_map);
    }

    clear_hashmap(&tiles_map);
    clear_hashmap(&props);
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Tile types map data loaded successfully.");
}

static tile_type get_type_from_coords(Uint8 x, Uint8 y) {
    for (size_t i = 0; i < types_map.len; i++) {
        tile_type_rect r = unpack_tile_type_rect(types_map.at[i]);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return (tile_type)r.type;
    }
    return 0;
}

static bool get_platform_from_coords(Uint8 x, Uint8 y) {
    for (size_t i = 0; i < platforms.len; i++) {
        platforms_rect r = unpack_platforms_rect(platforms.at[i]);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return true;
    }
    return false;
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
                    (*map)[i][j].type = get_type_from_coords(
                        (*map)[i][j].coords_x, (*map)[i][j].coords_y);
                    (*map)[i][j].platform = get_platform_from_coords(
                        (*map)[i][j].coords_x, (*map)[i][j].coords_y);
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
    char platforms_csv[] = {
#embed CHUNKS_PLATFORMS_CSV_PATH suffix(, '\0')
    };

    char types_map_csv[] = {
#embed CHUNKS_TYPES_MAP_CSV_PATH suffix(, '\0')
    };
#else
    char platforms_csv[] = {
#embed TESTS_CHUNKS_PLATFORMS_CSV_PATH suffix(, '\0')
    };

    char types_map_csv[] = {
#embed TESTS_CHUNKS_TYPES_MAP_CSV_PATH suffix(, '\0')
    };
#endif

    load_platforms_data(platforms_csv);
    load_types_map_data(types_map_csv);

#ifndef TEST
    char chunk001[] = {
#embed CHUNKS_001_PATH suffix(, '\0')
    };
    chunks[0] = load_from_json(chunk001);

    char chunk002[] = {
#embed CHUNKS_002_PATH suffix(, '\0')
    };
    chunks[1] = load_from_json(chunk002);

    char chunk003[] = {
#embed CHUNKS_003_PATH suffix(, '\0')
    };
    chunks[2] = load_from_json(chunk003);

    char chunk004[] = {
#embed CHUNKS_004_PATH suffix(, '\0')
    };
    chunks[3] = load_from_json(chunk004);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Chunks loaded successfully.");
#else
    char chunk001[] = {
#embed TESTS_CHUNKS_001_PATH suffix(, '\0')
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
    clear_dynarr(&platforms);
    clear_dynarr(&types_map);
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
    if (x + chunks[id].w > TILEMAP_SIZE_X ||
        y + chunks[id].h > TILEMAP_SIZE_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to load chunk id=%lu at (%d,%d) but it would loud "
                    "outside the tilemap's tile range",
                    id,
                    x,
                    y);
        return 2;
    }
    chunk c = chunks[id];
    TILEMAP_set_at(x, y, c.w, c.h, (tile(*)[c.w][c.h])c.map);

    // Fluid pools are in fluid cell coords relative to the chunk
    for (size_t i = 0; i < c.fluids.len; i++) {
        fluid_pool* p = c.fluids.at[i];
        FLUIDS_set_at(p->x + x * FLUIDS_SPLIT_X,
                      p->y + y * FLUIDS_SPLIT_Y,
                      p->w,
                      p->h,
                      p->type,
                      1.0f);
    }
    return 0;
}
