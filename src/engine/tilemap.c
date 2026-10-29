#include "engine/tilemap.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/camera.h"
#include "services/json.h"
#include <SDL3/SDL.h>

#ifdef TEST
#include "stubs.h"
#endif



typedef struct {
    struct {
        Uint8 x, y;
    } coords[256];
} autotile_table;



// Neighbor offsets, in the order of their bits on the autotiles' lookup tables:
//
// ```
//  1  2  4
//  8     16
//  32 64 128
// ```
static const int NEIGHBORS[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

static tile (*map)[TILEMAP_SIZE_X][TILEMAP_SIZE_Y] = NULL;
static dynarr autotiles = (dynarr){0};

// Index on `autotiles` plus one of the autotile each spritesheet tile belongs
// to, `0` meaning it's not part of any autotile.
static Uint8 autotile_ids[SPRITESHEET_X / SPRITESHEET_CELL_X]
                         [SPRITESHEET_Y / SPRITESHEET_CELL_Y] = {0};



static int get_firstgid(dynarr* tilesets, const char* name) {
    size_t name_len = SDL_strlen(name);
    for (size_t i = 0; i < tilesets->len; i++) {
        hashmap tileset = ((jsondata*)tilesets->at[i])->obj;
        char* source = ((jsondata*)get_from_hashmap("source", &tileset))->str;
        size_t source_len = SDL_strlen(source);
        if (source_len >= name_len &&
            SDL_strcmp(source + source_len - name_len, name) == 0)
            return (int)((jsondata*)get_from_hashmap("firstgid", &tileset))
                ->num;
    }
    return 0;
}

static dynarr get_layer_data(dynarr* layers, const char* prefix) {
    size_t prefix_len = SDL_strlen(prefix);
    for (size_t i = 0; i < layers->len; i++) {
        hashmap layer = ((jsondata*)layers->at[i])->obj;
        char* name = ((jsondata*)get_from_hashmap("name", &layer))->str;
        if (SDL_strncmp(name, prefix, prefix_len) == 0)
            return ((jsondata*)get_from_hashmap("data", &layer))->arr;
    }
    return (dynarr){0};
}

// Loads a Tiled automapping rule map into a new lookup table. Each output tile
// is the center of a 3x3 block on the input layer, where each neighbor is
// either occupied, empty or 0 for either way.
static void load_autotile(const char* json) {
    jsondata* data = JSON_parse(json);
    int w = (int)((jsondata*)get_from_hashmap("width", &data->obj))->num;
    dynarr tilesets =
        ((jsondata*)get_from_hashmap("tilesets", &data->obj))->arr;
    int rules_gid = get_firstgid(&tilesets, "automap-tiles.tsx");
    int sheet_gid = get_firstgid(&tilesets, "spritesheet.tsx");
    dynarr layers = ((jsondata*)get_from_hashmap("layers", &data->obj))->arr;
    dynarr input = get_layer_data(&layers, "input_");
    dynarr output = get_layer_data(&layers, "output_");

    autotile_table* table = SDL_calloc(1, sizeof(autotile_table));
    Uint8 id = autotiles.len + 1;
    for (size_t i = 0; i < output.len; i++) {
        // Skips empty cells and automapping tiles on the output layer
        int out = (int)((jsondata*)output.at[i])->num;
        if (out < sheet_gid) continue;

        Uint8 coords_x =
            (out - sheet_gid) % (SPRITESHEET_X / SPRITESHEET_CELL_X);
        Uint8 coords_y =
            (out - sheet_gid) / (SPRITESHEET_X / SPRITESHEET_CELL_X);
        int x = i % w;
        int y = i / w;
        Uint8 occupied = 0;
        Uint8 either = 0;
        for (int k = 0; k < 8; k++) {
            int u = x + NEIGHBORS[k][0];
            int v = y + NEIGHBORS[k][1];
            int in = (int)((jsondata*)input.at[u + w * v])->num;
            if (in == 0)
                either |= 1 << k;
            else if (in - rules_gid == TILEMAP_AUTOMAP_NON_EMPTY_ID)
                occupied |= 1 << k;
        }
        for (int val = 0; val < 256; val++) {
            if ((val & ~either) != occupied) continue;
            table->coords[val].x = coords_x;
            table->coords[val].y = coords_y;
        }
        autotile_ids[coords_x][coords_y] = id;
    }
    push_to_dynarr((void*)table, &autotiles);

    JSON_free(data);
}

// Returns the index on `autotiles` of the autotile `t` belongs to, or `-1` if
// `t` isn't part of any autotile.
static int get_autotile(tile t) {
    if (!t.type || t.coords_x >= SPRITESHEET_X / SPRITESHEET_CELL_X ||
        t.coords_y >= SPRITESHEET_Y / SPRITESHEET_CELL_Y)
        return -1;
    return autotile_ids[t.coords_x][t.coords_y] - 1;
}

// These args must be int because they can be -1
static bool offset(int autotile, int x, int y, int u, int v) {
    if (x + u < 0 || x + u >= TILEMAP_SIZE_X || y + v < 0 ||
        y + v >= TILEMAP_SIZE_Y)
        return true;
    return get_autotile((*map)[x + u][y + v]) == autotile;
}

static Uint8 check_adjacent_autotile(int autotile, Uint16 x, Uint16 y) {
    Uint8 val = 0;
    for (int k = 0; k < 8; k++)
        if (offset(autotile, x, y, NEIGHBORS[k][0], NEIGHBORS[k][1]))
            val |= 1 << k;
    return val;
}

static int update_autotile(Uint16 x, Uint16 y, bool suppress_warnings) {
    if (x < 0 || y < 0 || x >= TILEMAP_SIZE_X || y >= TILEMAP_SIZE_Y) {
        if (!suppress_warnings)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Tried to update autotile at %d, %d, which is outside "
                        "map range",
                        x,
                        y);
        return 1;
    }

    int autotile = get_autotile((*map)[x][y]);
    if (autotile == -1) return 2;

    autotile_table* table = autotiles.at[autotile];
    Uint8 val = check_adjacent_autotile(autotile, x, y);
    (*map)[x][y].coords_x = table->coords[val].x;
    (*map)[x][y].coords_y = table->coords[val].y;
    return 0;
}



void TILEMAP_start(void) {
#ifndef TEST
    char dirt_json[] = {
#embed TILEMAP_DIRT_AUTOTILE_PATH suffix(, '\0')
    };

    char stone_json[] = {
#embed TILEMAP_STONE_AUTOTILE_PATH suffix(, '\0')
    };
#else
    char dirt_json[] = {
#embed TESTS_TILEMAP_DIRT_AUTOTILE_PATH suffix(, '\0')
    };

    char stone_json[] = {
#embed TESTS_TILEMAP_STONE_AUTOTILE_PATH suffix(, '\0')
    };
#endif

    init_dynarr(&autotiles);
    load_autotile(dirt_json);
    load_autotile(stone_json);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Autotiles data loaded successfully.");
}

void TILEMAP_stop(void) {
    TILEMAP_clear();
    for (size_t i = 0; i < autotiles.len; i++)
        SDL_free(autotiles.at[i]);
    clear_dynarr(&autotiles);
    SDL_memset(autotile_ids, 0, sizeof(autotile_ids));
}

int TILEMAP_set_at(Uint16 x, Uint16 y, Uint16 w, Uint16 h, tile (*data)[w][h]) {
    if (x < 0 || y < 0 || x + w > TILEMAP_SIZE_X || y + h > TILEMAP_SIZE_Y) {
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
            update_autotile(x + i, y + j, true);
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
    if (!map) {
        *data = (tile){0};
        return 0;
    }
    *data = (*map)[x][y];
    return 0;
}

void TILEMAP_clear(void) {
    SDL_free(map);
    map = NULL;
}

void TILEMAP_draw(SDL_Renderer* renderer,
                  SDL_Texture* spritesheet,
                  bool platforms) {
    if (!map) return;

    SDL_FRect cam = CAMERA_get_rect();
    int from_x = cam.x / SPRITESHEET_CELL_X;
    int from_y = cam.y / SPRITESHEET_CELL_Y;
    int to_x =
        SDL_min(SDL_ceil((cam.x + cam.w) / SPRITESHEET_CELL_X), TILEMAP_SIZE_X);
    int to_y =
        SDL_min(SDL_ceil((cam.y + cam.h) / SPRITESHEET_CELL_Y), TILEMAP_SIZE_Y);

    for (int i = from_x; i < to_x; i++) {
        for (int j = from_y; j < to_y; j++) {
            if (!(*map)[i][j].type) continue;
            if ((*map)[i][j].platform != platforms) continue;
            SDL_FRect srcrect = (SDL_FRect){
                .x = SPRITESHEET_CELL_X * (*map)[i][j].coords_x,
                .y = SPRITESHEET_CELL_Y * (*map)[i][j].coords_y,
                .w = SPRITESHEET_CELL_X,
                .h = SPRITESHEET_CELL_Y,
            };
            SDL_FRect dstrect = (SDL_FRect){
                .x = SPRITESHEET_CELL_X * i - cam.x,
                .y = SPRITESHEET_CELL_Y * j - cam.y,
                .w = SPRITESHEET_CELL_X,
                .h = SPRITESHEET_CELL_Y,
            };
#ifndef TEST
            SDL_RenderTexture(renderer, spritesheet, &srcrect, &dstrect);
#else
            stubbed_SDL_RenderTexture(&srcrect, &dstrect, SDL_FLIP_NONE);
#endif
        }
    }
}



hashmap TILEMAP_get_string_tile_types_map(void) {
    hashmap map;
    init_hashmap(&map);
    set_on_hashmap((void*)tile_dirt, "dirt", &map);
    set_on_hashmap((void*)tile_stone, "stone", &map);
    set_on_hashmap((void*)tile_wood, "wood", &map);
    set_on_hashmap((void*)tile_metal, "metal", &map);
    return map;
}
