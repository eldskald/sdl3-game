#include "engine/chunks.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "paths.h"
#include "props/vines.h"
#include "services/csv.h"
#include "services/json.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>



typedef struct {
    Uint8 x;
    Uint8 y;
    Uint8 w;
    Uint8 h;
} platforms_rect;

typedef struct {
    tile_type type;
    Uint8 x;
    Uint8 y;
    Uint8 w;
    Uint8 h;
} tile_type_rect;

// What a vine coming down into a helper tile at some `x` offset finds: nothing,
// a full tile tall vine that keeps going down or a half tile tall vine end.
typedef enum {
    helper_none,
    helper_down,
    helper_end,
} helper_vertical;

// Which side the vine coming down at `bend_x` turns to, at the tile's middle.
typedef enum {
    helper_no_bend,
    helper_bend_left,
    helper_bend_right,
} helper_bend;

// Shape of a tile from the helper tileset on the vines layer. `vertical` is
// indexed by `x` offset, `horizontal` is for tiles a vine crosses sideways.
// Crossed tiles have both, so a vine crossing sideways doesn't interfere with
// the ones going down through it.
typedef struct {
    helper_vertical vertical[SPRITESHEET_CELL_X];
    helper_bend bend;
    Uint8 bend_x;
    bool horizontal;
} helper_tile;



// Indexed by tile ID on `editor/helper-tiles.png`, which starts at the
// helper tileset's `firstgid` on the chunk's JSON.
static const helper_tile helper_tiles[CHUNKS_HELPER_TILES_TOTAL] = {
    [0] = {.vertical = {[2] = helper_down, [5] = helper_down}},
    [1] = {.vertical = {[2] = helper_down}},
    [2] = {.vertical = {[3] = helper_down}},
    [3] = {.vertical = {[4] = helper_down}},
    [4] = {.vertical = {[5] = helper_down}},
    [5] = {.bend = helper_bend_right, .bend_x = 5},
    [6] = {.bend = helper_bend_left, .bend_x = 5},
    [7] = {.horizontal = true},
    [8] = {.vertical = {[2] = helper_end, [5] = helper_down}},
    [9] = {.vertical = {[2] = helper_end}},
    [10] = {.vertical = {[3] = helper_end}},
    [11] = {.vertical = {[4] = helper_end}},
    [12] = {.vertical = {[5] = helper_end}},
    [13] = {.bend = helper_bend_right, .bend_x = 4},
    [14] = {.bend = helper_bend_left, .bend_x = 4},
    [15] = {.vertical = {[2] = helper_down, [5] = helper_down},
            .horizontal = true},
    [16] = {.vertical = {[2] = helper_down, [5] = helper_end}},
    [17] = {.vertical = {[2] = helper_down}, .horizontal = true},
    [18] = {.vertical = {[3] = helper_down}, .horizontal = true},
    [19] = {.vertical = {[4] = helper_down}, .horizontal = true},
    [20] = {.vertical = {[5] = helper_down}, .horizontal = true},
    [21] = {.bend = helper_bend_right, .bend_x = 3},
    [22] = {.bend = helper_bend_left, .bend_x = 3},
    [24] = {.vertical = {[2] = helper_end, [5] = helper_end}},
    [29] = {.bend = helper_bend_right, .bend_x = 2},
    [30] = {.bend = helper_bend_left, .bend_x = 2},
};
static const helper_tile empty_helper_tile = (helper_tile){0};

static chunk chunks[CHUNKS_TOTAL] = {0};
static dynarr platforms = (dynarr){0};
static dynarr types_map = (dynarr){0};



static void load_platforms_data(const char* csv) {
    dynarr parsed = CSV_parse(csv);
    size_t n = PARSING_count_entries(&parsed);
    init_dynarr_with(&platforms, sizeof(platforms_rect), n);
    platforms_rect* r = grow_dynarr(n, &platforms);
    parsing_field fields[] = {
        {"x", parsing_uint8, &r->x},
        {"y", parsing_uint8, &r->y},
        {"w", parsing_uint8, &r->w},
        {"h", parsing_uint8, &r->h},
    };
    PARSING_read_table(&parsed, fields, SDL_arraysize(fields), sizeof(*r));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Platforms data loaded successfully.");
}

static void load_types_map_data(const char* csv) {
    dynarr parsed = CSV_parse(csv);
    size_t n = PARSING_count_entries(&parsed);
    init_dynarr_with(&types_map, sizeof(tile_type_rect), n);
    tile_type_rect* r = grow_dynarr(n, &types_map);
    parsing_field fields[] = {
        {"type", parsing_tile, &r->type},
        {"x", parsing_uint8, &r->x},
        {"y", parsing_uint8, &r->y},
        {"w", parsing_uint8, &r->w},
        {"h", parsing_uint8, &r->h},
    };
    PARSING_read_table(&parsed, fields, SDL_arraysize(fields), sizeof(*r));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Tile types map data loaded successfully.");
}

static tile_type get_type_from_coords(Uint8 x, Uint8 y) {
    for (size_t i = 0; i < types_map.len; i++) {
        tile_type_rect* r = dynarr_at(i, &types_map);
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h)
            return r->type;
    }
    return 0;
}

static bool get_platform_from_coords(Uint8 x, Uint8 y) {
    for (size_t i = 0; i < platforms.len; i++) {
        platforms_rect* r = dynarr_at(i, &platforms);
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h)
            return true;
    }
    return false;
}

// The vines layer's tile data, with its size and the helper tileset's firstgid.
typedef struct {
    dynarr tiles;
    int w, h;
    int firstgid;
} vines_layer;

// Tiles outside the layer or not from the helper tileset are empty.
static const helper_tile* get_helper_tile(vines_layer* l, int x, int y) {
    if (x < 0 || y < 0 || x >= l->w || y >= l->h) return &empty_helper_tile;
    int id = (int)JSON_at(x + l->w * y, &l->tiles)->num - l->firstgid;
    if (id < 0 || id >= CHUNKS_HELPER_TILES_TOTAL) return &empty_helper_tile;
    return &helper_tiles[id];
}

// Whether a vine coming down at offset `c` continues into this tile.
static bool takes_vine_from_top(const helper_tile* t, Uint8 c) {
    return t->vertical[c] != helper_none ||
           (t->bend != helper_no_bend && t->bend_x == c);
}

// Follows the vine coming down from the top of tile `(x,y)` at offset `c`.
// Full tiles, bends and horizontal tiles are 2 segments each, vine ends are 1.
// Bends lead sideways, turning at the tile's middle, until a bend going up,
// then up until the ceiling where its second pin is. Without a bend going up,
// the vine just ends where the sideways tiles end.
static vine_spawn trace_vine(vines_layer* l, int x, int y, Uint8 c) {
    vine_spawn s = (vine_spawn){.a_x = x * SPRITESHEET_CELL_X + c,
                                .a_y = y * SPRITESHEET_CELL_Y,
                                .b_x = x * SPRITESHEET_CELL_X + c};
    while (true) {
        const helper_tile* t = get_helper_tile(l, x, y);
        if (t->vertical[c] == helper_end) {
            s.segments += 1;
            s.b_y = y * SPRITESHEET_CELL_Y + SPRITESHEET_CELL_Y / 2;
            s.bend_y = s.b_y;
            return s;
        }
        if (t->vertical[c] == helper_down) {
            s.segments += 2;
            if (!takes_vine_from_top(get_helper_tile(l, x, y + 1), c)) {
                s.b_y = (y + 1) * SPRITESHEET_CELL_Y;
                s.bend_y = s.b_y;
                return s;
            }
            y++;
            continue;
        }

        // Bending sideways
        s.segments += 2;
        s.bend_y = y * SPRITESHEET_CELL_Y + SPRITESHEET_CELL_Y / 2;
        int dir = t->bend == helper_bend_left ? -1 : 1;
        helper_bend up_bend = dir == 1 ? helper_bend_left : helper_bend_right;
        x += dir;
        t = get_helper_tile(l, x, y);
        while (t->horizontal) {
            s.segments += 2;
            x += dir;
            t = get_helper_tile(l, x, y);
        }
        if (t->bend != up_bend) {
            // Ends at the side of the last sideways tile
            s.b_x = (dir == 1 ? x : x + 1) * SPRITESHEET_CELL_X;
            s.b_y = s.bend_y;
            return s;
        }

        // Bending up
        s.segments += 2;
        c = t->bend_x;
        while (get_helper_tile(l, x, y - 1)->vertical[c] == helper_down) {
            s.segments += 2;
            y--;
        }
        s.double_pinned = true;
        s.b_x = x * SPRITESHEET_CELL_X + c;
        s.b_y = y * SPRITESHEET_CELL_Y;
        return s;
    }
}

// A vine starts at every tile a vine comes down into from a tile above that
// doesn't have one going down. Double pinned vines start at both ends, so they
// are only kept from the end that comes first, top to bottom, left to right.
static void load_vines(vines_layer* l, dynarr* vines) {
    for (int y = 0; y < l->h; y++) {
        for (int x = 0; x < l->w; x++) {
            const helper_tile* t = get_helper_tile(l, x, y);
            const helper_tile* above = get_helper_tile(l, x, y - 1);
            for (Uint8 c = 0; c < SPRITESHEET_CELL_X; c++) {
                if (!takes_vine_from_top(t, c)) continue;
                if (above->vertical[c] == helper_down) continue;
                vine_spawn s = trace_vine(l, x, y, c);
                if (s.double_pinned &&
                    (s.b_y < s.a_y || (s.b_y == s.a_y && s.b_x < s.a_x)))
                    continue;
                push_to_dynarr(&s, vines);
            }
        }
    }
}

static int get_helper_firstgid(jsondata* data) {
    dynarr tilesets = JSON_get("tilesets", data)->arr;
    for (size_t i = 0; i < tilesets.len; i++) {
        jsondata* ts = JSON_at(i, &tilesets);
        // Embedded tilesets have no source
        jsondata* source = JSON_get("source", ts);
        if (source && SDL_strstr(source->str, CHUNKS_HELPER_TILESET_NAME))
            return (int)JSON_get("firstgid", ts)->num;
    }
    return 0;
}

static chunk load_from_json(const char* json) {
    jsondata* data = JSON_parse(json);
    int w = (int)JSON_get("width", data)->num;
    int h = (int)JSON_get("height", data)->num;
    tile(*map)[w][h] = SDL_calloc(w * h, sizeof(tile));
    dynarr fluids = (dynarr){0};
    init_dynarr(&fluids, sizeof(fluid_pool));
    dynarr vines = (dynarr){0};
    init_dynarr(&vines, sizeof(vine_spawn));

    dynarr layers = JSON_get("layers", data)->arr;

    // Parse each layer
    for (int idx = 0; idx < layers.len; idx++) {
        jsondata* layer = JSON_at(idx, &layers);
        char* name = JSON_get("name", layer)->str;
        char* type = JSON_get("type", layer)->str;

        // If layer is main tilemap
        if (SDL_strcmp(name, "Main") == 0 &&
            SDL_strcmp(type, "tilelayer") == 0) {
            dynarr tiles = JSON_get("data", layer)->arr;
            for (int i = 0; i < w; i++) {
                for (int j = 0; j < h; j++) {
                    int curr = (int)JSON_at(i + w * j, &tiles)->num - 1;
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

        // If layer is the vines layer
        if (SDL_strcmp(name, "Vines") == 0 &&
            SDL_strcmp(type, "tilelayer") == 0) {
            vines_layer l = (vines_layer){
                .tiles = JSON_get("data", layer)->arr,
                .w = w,
                .h = h,
                .firstgid = get_helper_firstgid(data),
            };
            load_vines(&l, &vines);
        }

        // If layer is a water layer
        if (SDL_strcmp(name, "Water") == 0 &&
            SDL_strcmp(type, "objectgroup") == 0) {
            dynarr objects = JSON_get("objects", layer)->arr;
            for (int i = 0; i < objects.len; i++) {
                jsondata* obj = JSON_at(i, &objects);
                double x = JSON_get("x", obj)->num;
                double y = JSON_get("y", obj)->num;
                double w = JSON_get("width", obj)->num;
                double h = JSON_get("height", obj)->num;
                fluid_pool pool = (fluid_pool){.x = x / FLUIDS_CELL_W,
                                               .y = y / FLUIDS_CELL_H,
                                               .w = w / FLUIDS_CELL_W,
                                               .h = h / FLUIDS_CELL_H,
                                               .type = fluid_water};
                push_to_dynarr(&pool, &fluids);
            }
        }

        // If layer is a lava layer
        if (SDL_strcmp(name, "Lava") == 0 &&
            SDL_strcmp(type, "objectgroup") == 0) {
            dynarr objects = JSON_get("objects", layer)->arr;
            for (int i = 0; i < objects.len; i++) {
                jsondata* obj = JSON_at(i, &objects);
                double x = JSON_get("x", obj)->num;
                double y = JSON_get("y", obj)->num;
                double w = JSON_get("width", obj)->num;
                double h = JSON_get("height", obj)->num;
                fluid_pool pool = (fluid_pool){.x = x / FLUIDS_CELL_W,
                                               .y = y / FLUIDS_CELL_H,
                                               .w = w / FLUIDS_CELL_W,
                                               .h = h / FLUIDS_CELL_H,
                                               .type = fluid_lava};
                push_to_dynarr(&pool, &fluids);
            }
        }
    }

    JSON_free(data);
    return (chunk){
        .w = w, .h = h, .map = (tile***)map, .fluids = fluids, .vines = vines};
}

void CHUNKS_start(void) {
    char platforms_csv[] = {
#embed CHUNKS_PLATFORMS_CSV_PATH suffix(, '\0')
    };

    char types_map_csv[] = {
#embed CHUNKS_TYPES_MAP_CSV_PATH suffix(, '\0')
    };

    load_platforms_data(platforms_csv);
    load_types_map_data(types_map_csv);

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
}

void CHUNKS_stop(void) {
    for (int i = 0; i < CHUNKS_TOTAL; i++) {
        SDL_free(chunks[i].map);
        deinit_dynarr(&chunks[i].fluids);
        deinit_dynarr(&chunks[i].vines);
    }
    deinit_dynarr(&platforms);
    deinit_dynarr(&types_map);
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
        fluid_pool* p = dynarr_at(i, &c.fluids);
        FLUIDS_set_at(p->x + x * FLUIDS_SPLIT_X,
                      p->y + y * FLUIDS_SPLIT_Y,
                      p->w,
                      p->h,
                      p->type,
                      1.0f);
    }

    // Vine pins are in pixel coords relative to the chunk
    double px = x * SPRITESHEET_CELL_X;
    double py = y * SPRITESHEET_CELL_Y;
    for (size_t i = 0; i < c.vines.len; i++) {
        vine_spawn* v = dynarr_at(i, &c.vines);
        SDL_FPoint path[] = {
            {v->a_x + px, v->a_y + py},
            {v->a_x + px, v->bend_y + py},
            {v->b_x + px, v->bend_y + py},
            {v->b_x + px, v->b_y + py},
        };
        VINES_add(path, SDL_arraysize(path), v->segments, v->double_pinned);
    }
    return 0;
}
