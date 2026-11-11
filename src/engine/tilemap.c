#include "engine/tilemap.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/audio.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/particles.h"
#include "engine/renderer.h"
#include "engine/sfx.h"
#include "paths.h"
#include "services/csv.h"
#include "services/json.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>



#define TILE_SFX_MAX 5
#define TILE_SFX_VOICES_TOTAL 4
#define SIN75 0.9659258
#define COS75 0.2588190



typedef struct {
    struct {
        Uint8 x, y;
    } coords[256];
} autotile_table;

typedef struct {
    particle_type particle;
    float speed;
    float spread;
    float speed_jitter;
    Uint32 count;
} step_particles;

typedef struct {
    particle_type particle;
    float speed;
    float spread;
    float speed_jitter;
    float count_per_pixel;
} land_particles;

// A sound embedded on the executable that plays when stepping or landing on
// `type`, a `tile_type` or a `fluid_type` depending on the table it's on. A
// `NULL` `mem` with a `0` `size` is for no sound, so `type` plays nothing.
typedef struct {
    int type;
    const Uint8* mem;
    size_t size;
} tile_sfx_file;

// The sounds that can play when stepping or landing on a tile type, one of them
// picked at random each time, at `volume`.
typedef struct {
    audio_wav* wavs[TILE_SFX_MAX];
    Uint8 count;
    float volume;
} tile_sfx;



static const char dirt_json[] = {
#embed TILEMAP_DIRT_AUTOTILE_PATH suffix(, '\0')
};
static const char stone_json[] = {
#embed TILEMAP_STONE_AUTOTILE_PATH suffix(, '\0')
};
static const char step_on_particles_csv[] = {
#embed TILEMAP_STEP_ON_PARTICLES_CSV_PATH suffix(, '\0')
};
static const char land_on_particles_csv[] = {
#embed TILEMAP_LAND_ON_PARTICLES_CSV_PATH suffix(, '\0')
};
static const char audio_csv[] = {
#embed TILEMAP_AUDIO_CSV_PATH suffix(, '\0')
};
static const char fluids_audio_csv[] = {
#embed FLUIDS_AUDIO_CSV_PATH suffix(, '\0')
};

static const Uint8 dirt_step_1_wav[] = {
#embed TILEMAP_DIRT_STEP_1_SFX_PATH
};
static const Uint8 dirt_step_2_wav[] = {
#embed TILEMAP_DIRT_STEP_2_SFX_PATH
};
static const Uint8 dirt_step_3_wav[] = {
#embed TILEMAP_DIRT_STEP_3_SFX_PATH
};
static const Uint8 dirt_step_4_wav[] = {
#embed TILEMAP_DIRT_STEP_4_SFX_PATH
};
static const Uint8 dirt_step_5_wav[] = {
#embed TILEMAP_DIRT_STEP_5_SFX_PATH
};

static const Uint8 stone_step_1_wav[] = {
#embed TILEMAP_STONE_STEP_1_SFX_PATH
};
static const Uint8 stone_step_2_wav[] = {
#embed TILEMAP_STONE_STEP_2_SFX_PATH
};
static const Uint8 stone_step_3_wav[] = {
#embed TILEMAP_STONE_STEP_3_SFX_PATH
};
static const Uint8 stone_step_4_wav[] = {
#embed TILEMAP_STONE_STEP_4_SFX_PATH
};
static const Uint8 stone_step_5_wav[] = {
#embed TILEMAP_STONE_STEP_5_SFX_PATH
};

static const Uint8 wood_step_1_wav[] = {
#embed TILEMAP_WOOD_STEP_1_SFX_PATH
};
static const Uint8 wood_step_2_wav[] = {
#embed TILEMAP_WOOD_STEP_2_SFX_PATH
};
static const Uint8 wood_step_3_wav[] = {
#embed TILEMAP_WOOD_STEP_3_SFX_PATH
};
static const Uint8 wood_step_4_wav[] = {
#embed TILEMAP_WOOD_STEP_4_SFX_PATH
};
static const Uint8 wood_step_5_wav[] = {
#embed TILEMAP_WOOD_STEP_5_SFX_PATH
};

static const Uint8 metal_step_1_wav[] = {
#embed TILEMAP_METAL_STEP_1_SFX_PATH
};
static const Uint8 metal_step_2_wav[] = {
#embed TILEMAP_METAL_STEP_2_SFX_PATH
};
static const Uint8 metal_step_3_wav[] = {
#embed TILEMAP_METAL_STEP_3_SFX_PATH
};
static const Uint8 metal_step_4_wav[] = {
#embed TILEMAP_METAL_STEP_4_SFX_PATH
};
static const Uint8 metal_step_5_wav[] = {
#embed TILEMAP_METAL_STEP_5_SFX_PATH
};

static const Uint8 dirt_land_1_wav[] = {
#embed TILEMAP_DIRT_LAND_1_SFX_PATH
};
static const Uint8 dirt_land_2_wav[] = {
#embed TILEMAP_DIRT_LAND_2_SFX_PATH
};

static const Uint8 stone_land_1_wav[] = {
#embed TILEMAP_STONE_LAND_1_SFX_PATH
};
static const Uint8 stone_land_2_wav[] = {
#embed TILEMAP_STONE_LAND_2_SFX_PATH
};
static const Uint8 stone_land_3_wav[] = {
#embed TILEMAP_STONE_LAND_3_SFX_PATH
};

static const Uint8 wood_land_1_wav[] = {
#embed TILEMAP_WOOD_LAND_1_SFX_PATH
};
static const Uint8 wood_land_2_wav[] = {
#embed TILEMAP_WOOD_LAND_2_SFX_PATH
};

static const Uint8 metal_land_1_wav[] = {
#embed TILEMAP_METAL_LAND_1_SFX_PATH
};
static const Uint8 metal_land_2_wav[] = {
#embed TILEMAP_METAL_LAND_2_SFX_PATH
};
static const Uint8 metal_land_3_wav[] = {
#embed TILEMAP_METAL_LAND_3_SFX_PATH
};

static const Uint8 water_step_1_wav[] = {
#embed TILEMAP_WATER_STEP_1_SFX_PATH
};
static const Uint8 water_step_2_wav[] = {
#embed TILEMAP_WATER_STEP_2_SFX_PATH
};
static const Uint8 water_step_3_wav[] = {
#embed TILEMAP_WATER_STEP_3_SFX_PATH
};
static const Uint8 water_step_4_wav[] = {
#embed TILEMAP_WATER_STEP_4_SFX_PATH
};
static const Uint8 water_step_5_wav[] = {
#embed TILEMAP_WATER_STEP_5_SFX_PATH
};

static const Uint8 sludge_step_1_wav[] = {
#embed TILEMAP_SLUDGE_STEP_1_SFX_PATH
};
static const Uint8 sludge_step_2_wav[] = {
#embed TILEMAP_SLUDGE_STEP_2_SFX_PATH
};

static const Uint8 water_land_1_wav[] = {
#embed TILEMAP_WATER_LAND_1_SFX_PATH
};
static const Uint8 water_land_2_wav[] = {
#embed TILEMAP_WATER_LAND_2_SFX_PATH
};
static const Uint8 water_land_3_wav[] = {
#embed TILEMAP_WATER_LAND_3_SFX_PATH
};

static const Uint8 sludge_land_1_wav[] = {
#embed TILEMAP_SLUDGE_LAND_1_SFX_PATH
};
static const Uint8 sludge_land_2_wav[] = {
#embed TILEMAP_SLUDGE_LAND_2_SFX_PATH
};

static const tile_sfx_file step_sfx_files[] = {
    {tile_dirt, dirt_step_1_wav, sizeof(dirt_step_1_wav)},
    {tile_dirt, dirt_step_2_wav, sizeof(dirt_step_2_wav)},
    {tile_dirt, dirt_step_3_wav, sizeof(dirt_step_3_wav)},
    {tile_dirt, dirt_step_4_wav, sizeof(dirt_step_4_wav)},
    {tile_dirt, dirt_step_5_wav, sizeof(dirt_step_5_wav)},
    {tile_stone, stone_step_1_wav, sizeof(stone_step_1_wav)},
    {tile_stone, stone_step_2_wav, sizeof(stone_step_2_wav)},
    {tile_stone, stone_step_3_wav, sizeof(stone_step_3_wav)},
    {tile_stone, stone_step_4_wav, sizeof(stone_step_4_wav)},
    {tile_stone, stone_step_5_wav, sizeof(stone_step_5_wav)},
    {tile_wood, wood_step_1_wav, sizeof(wood_step_1_wav)},
    {tile_wood, wood_step_2_wav, sizeof(wood_step_2_wav)},
    {tile_wood, wood_step_3_wav, sizeof(wood_step_3_wav)},
    {tile_wood, wood_step_4_wav, sizeof(wood_step_4_wav)},
    {tile_wood, wood_step_5_wav, sizeof(wood_step_5_wav)},
    {tile_metal, metal_step_1_wav, sizeof(metal_step_1_wav)},
    {tile_metal, metal_step_2_wav, sizeof(metal_step_2_wav)},
    {tile_metal, metal_step_3_wav, sizeof(metal_step_3_wav)},
    {tile_metal, metal_step_4_wav, sizeof(metal_step_4_wav)},
    {tile_metal, metal_step_5_wav, sizeof(metal_step_5_wav)},
};
static const tile_sfx_file land_sfx_files[] = {
    {tile_dirt, dirt_land_1_wav, sizeof(dirt_land_1_wav)},
    {tile_dirt, dirt_land_2_wav, sizeof(dirt_land_2_wav)},
    {tile_stone, stone_land_1_wav, sizeof(stone_land_1_wav)},
    {tile_stone, stone_land_2_wav, sizeof(stone_land_2_wav)},
    {tile_stone, stone_land_3_wav, sizeof(stone_land_3_wav)},
    {tile_wood, wood_land_1_wav, sizeof(wood_land_1_wav)},
    {tile_wood, wood_land_2_wav, sizeof(wood_land_2_wav)},
    {tile_metal, metal_land_1_wav, sizeof(metal_land_1_wav)},
    {tile_metal, metal_land_2_wav, sizeof(metal_land_2_wav)},
    {tile_metal, metal_land_3_wav, sizeof(metal_land_3_wav)},
};

static const tile_sfx_file fluids_step_sfx_files[] = {
    {fluid_water, water_step_1_wav, sizeof(water_step_1_wav)},
    {fluid_water, water_step_2_wav, sizeof(water_step_2_wav)},
    {fluid_water, water_step_3_wav, sizeof(water_step_3_wav)},
    {fluid_water, water_step_4_wav, sizeof(water_step_4_wav)},
    {fluid_water, water_step_5_wav, sizeof(water_step_5_wav)},
    {fluid_acid, water_step_1_wav, sizeof(water_step_1_wav)},
    {fluid_acid, water_step_2_wav, sizeof(water_step_2_wav)},
    {fluid_acid, water_step_3_wav, sizeof(water_step_3_wav)},
    {fluid_acid, water_step_4_wav, sizeof(water_step_4_wav)},
    {fluid_acid, water_step_5_wav, sizeof(water_step_5_wav)},
    {fluid_sludge, sludge_step_1_wav, sizeof(sludge_step_1_wav)},
    {fluid_sludge, sludge_step_2_wav, sizeof(sludge_step_2_wav)},
};
static const tile_sfx_file fluids_land_sfx_files[] = {
    {fluid_water, water_land_1_wav, sizeof(water_land_1_wav)},
    {fluid_water, water_land_2_wav, sizeof(water_land_2_wav)},
    {fluid_water, water_land_3_wav, sizeof(water_land_3_wav)},
    {fluid_acid, water_land_1_wav, sizeof(water_land_1_wav)},
    {fluid_acid, water_land_2_wav, sizeof(water_land_2_wav)},
    {fluid_acid, water_land_3_wav, sizeof(water_land_3_wav)},
    {fluid_sludge, sludge_land_1_wav, sizeof(sludge_land_1_wav)},
    {fluid_sludge, sludge_land_2_wav, sizeof(sludge_land_2_wav)},
};

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

static Uint8 autotile_ids[SPRITESHEET_X / SPRITESHEET_CELL_X]
                         [SPRITESHEET_Y / SPRITESHEET_CELL_Y] = {0};

static step_particles step_on_particles[TILEMAP_TILE_TYPES_TOTAL] = {0};
static land_particles land_on_particles[TILEMAP_TILE_TYPES_TOTAL] = {0};
static tile_sfx step_sfx[TILEMAP_TILE_TYPES_TOTAL] = {0};
static tile_sfx land_sfx[TILEMAP_TILE_TYPES_TOTAL] = {0};

static tile_sfx fluids_step_sfx[FLUIDS_TOTAL] = {0};
static tile_sfx fluids_land_sfx[FLUIDS_TOTAL] = {0};

static size_t sfx_voices[TILE_SFX_VOICES_TOTAL] = {0};
static bool sfx_voices_created = false;
static Uint8 sfx_next_voice = 0;
static Uint64 sfx_last_ns = 0;
static bool sfx_played = false;

static double sfx_cooldown = 0.0;
static double sfx_falloff_range = 0.0;



static int get_firstgid(dynarr* tilesets, const char* name) {
    size_t name_len = SDL_strlen(name);
    for (size_t i = 0; i < tilesets->len; i++) {
        jsondata* tileset = JSON_at(i, tilesets);
        char* source = JSON_get("source", tileset)->str;
        size_t source_len = SDL_strlen(source);
        if (source_len >= name_len &&
            SDL_strcmp(source + source_len - name_len, name) == 0)
            return (int)JSON_get("firstgid", tileset)->num;
    }
    return 0;
}

static dynarr get_layer_data(dynarr* layers, const char* prefix) {
    size_t prefix_len = SDL_strlen(prefix);
    for (size_t i = 0; i < layers->len; i++) {
        jsondata* layer = JSON_at(i, layers);
        char* name = JSON_get("name", layer)->str;
        if (SDL_strncmp(name, prefix, prefix_len) == 0)
            return JSON_get("data", layer)->arr;
    }
    return (dynarr){0};
}

// Loads a Tiled automapping rule map into a new lookup table. Each output tile
// is the center of a 3x3 block on the input layer, where each neighbor is
// either occupied, empty or 0 for either way.
static void load_autotile(const char* json) {
    jsondata* data = JSON_parse(json);
    int w = (int)JSON_get("width", data)->num;
    dynarr tilesets = JSON_get("tilesets", data)->arr;
    int rules_gid = get_firstgid(&tilesets, "automap-tiles.tsx");
    int sheet_gid = get_firstgid(&tilesets, "spritesheet.tsx");
    dynarr layers = JSON_get("layers", data)->arr;
    dynarr input = get_layer_data(&layers, "input_");
    dynarr output = get_layer_data(&layers, "output_");

    Uint8 id = autotiles.len + 1;
    autotile_table* table = push_to_dynarr(NULL, &autotiles);
    for (size_t i = 0; i < output.len; i++) {
        // Skips empty cells and automapping tiles on the output layer
        int out = (int)JSON_at(i, &output)->num;
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
            int in = (int)JSON_at(u + w * v, &input)->num;
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

    autotile_table* table = dynarr_at(autotile, &autotiles);
    Uint8 val = check_adjacent_autotile(autotile, x, y);
    (*map)[x][y].coords_x = table->coords[val].x;
    (*map)[x][y].coords_y = table->coords[val].y;
    return 0;
}

static void load_step_on_particles(const char* csv) {
    struct {
        tile_type type;
        step_particles data;
    } rows[TILEMAP_TILE_TYPES_TOTAL] = {0};
    step_particles* d = &rows[0].data;
    parsing_field fields[] = {
        {"tile", parsing_tile, &rows[0].type},
        {"particle", parsing_particle, &d->particle},
        {"speed", parsing_float, &d->speed},
        {"spread", parsing_float, &d->spread},
        {"speed_jitter", parsing_float, &d->speed_jitter},
        {"count", parsing_uint32, &d->count},
    };
    dynarr parsed = CSV_parse(csv);
    size_t n =
        read_csv_table(&parsed, fields, SDL_arraysize(fields), sizeof(rows[0]));
    for (size_t i = 0; i < n; i++)
        step_on_particles[rows[i].type] = rows[i].data;
    CSV_free(&parsed);
}

static void load_land_on_particles(const char* csv) {
    struct {
        tile_type type;
        land_particles data;
    } rows[TILEMAP_TILE_TYPES_TOTAL] = {0};
    land_particles* d = &rows[0].data;
    parsing_field fields[] = {
        {"tile", parsing_tile, &rows[0].type},
        {"particle", parsing_particle, &d->particle},
        {"speed", parsing_float, &d->speed},
        {"spread", parsing_float, &d->spread},
        {"speed_jitter", parsing_float, &d->speed_jitter},
        {"count_per_pixel", parsing_float, &d->count_per_pixel},
    };
    dynarr parsed = CSV_parse(csv);
    size_t n =
        read_csv_table(&parsed, fields, SDL_arraysize(fields), sizeof(rows[0]));
    for (size_t i = 0; i < n; i++)
        land_on_particles[rows[i].type] = rows[i].data;
    CSV_free(&parsed);
}

// The first section of `TILEMAP_AUDIO_CSV_PATH` has the sounds' cooldown and
// falloff range, and its second the table of each tile type's volumes. The
// fluids' volumes are on their columns of `FLUIDS_AUDIO_CSV_PATH`'s table.
static void load_sfx_volumes(void) {
    parsing_field keys[] = {
        {"sfx_cooldown", parsing_double, &sfx_cooldown},
        {"falloff_range", parsing_double, &sfx_falloff_range},
    };
    dynarr parsed = CSV_parse_section(audio_csv, 0);
    read_csv_keys(&parsed, keys, SDL_arraysize(keys));
    CSV_free(&parsed);

    struct {
        tile_type type;
        float step_vol, land_vol;
    } tiles[TILEMAP_TILE_TYPES_TOTAL] = {0};
    parsing_field tile_fields[] = {
        {"tile", parsing_tile, &tiles[0].type},
        {"step_vol", parsing_float, &tiles[0].step_vol},
        {"land_vol", parsing_float, &tiles[0].land_vol},
    };
    parsed = CSV_parse_section(audio_csv, 1);
    size_t n = read_csv_table(
        &parsed, tile_fields, SDL_arraysize(tile_fields), sizeof(tiles[0]));
    CSV_free(&parsed);
    for (size_t i = 0; i < n; i++) {
        step_sfx[tiles[i].type].volume = tiles[i].step_vol;
        land_sfx[tiles[i].type].volume = tiles[i].land_vol;
    }

    struct {
        fluid_type type;
        float step_vol, land_vol;
    } fluids[FLUIDS_TOTAL] = {0};
    parsing_field fluid_fields[] = {
        {"fluid", parsing_fluid, &fluids[0].type},
        {"step_vol", parsing_float, &fluids[0].step_vol},
        {"land_vol", parsing_float, &fluids[0].land_vol},
    };
    parsed = CSV_parse_section(fluids_audio_csv, 1);
    n = read_csv_table(
        &parsed, fluid_fields, SDL_arraysize(fluid_fields), sizeof(fluids[0]));
    CSV_free(&parsed);
    for (size_t i = 0; i < n; i++) {
        fluids_step_sfx[fluids[i].type].volume = fluids[i].step_vol;
        fluids_land_sfx[fluids[i].type].volume = fluids[i].land_vol;
    }
}

// Loads `f` into `set`, unless it's full or `f` has no sound.
static void add_sfx(tile_sfx* set, tile_sfx_file f, const char* name) {
    if (!f.mem) return;
    if (set->count >= TILE_SFX_MAX) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "More than %d %s SFX, ignoring the rest",
                    TILE_SFX_MAX,
                    name);
        return;
    }
    audio_wav* wav =
        AUDIO_create_wav(name, SDL_IOFromConstMem(f.mem, f.size), true);
    if (wav) set->wavs[set->count++] = wav;
}

// Loads the `n` `files` into `sets`, indexed by their types.
static void load_sfx(const tile_sfx_file* files,
                     size_t n,
                     tile_sfx* sets,
                     const char* name) {
    for (size_t i = 0; i < n; i++)
        add_sfx(&sets[files[i].type], files[i], name);
}

// Frees the `n` `sets`.
static void unload_sfx(tile_sfx* sets, size_t n) {
    for (size_t t = 0; t < n; t++) {
        for (int i = 0; i < sets[t].count; i++)
            AUDIO_destroy_wav(sets[t].wavs[i]);
        sets[t] = (tile_sfx){0};
    }
}

static void delete_sfx_voices(void) {
    if (sfx_voices_created)
        for (int i = 0; i < TILE_SFX_VOICES_TOTAL; i++)
            SFX_del_voice(sfx_voices[i]);
    sfx_voices_created = false;
    sfx_next_voice = 0;
    sfx_played = false;
}

// Row of the tiles right under the bottom of `rect`.
static int get_row_under(SDL_FRect rect) {
    return (int)SDL_floorf((rect.y + rect.h + 0.5f) / SPRITESHEET_CELL_Y);
}

// Type of the tile right under the bottom of `rect`, checking under its center
// first and then under its left and right edges. `0` if there's none.
static tile_type get_tile_under(SDL_FRect rect) {
    if (!map) return 0;
    int y = get_row_under(rect);
    if (y < 0 || y >= TILEMAP_SIZE_Y) return 0;
    float xs[] = {rect.x + rect.w / 2, rect.x, rect.x + rect.w - 0.01f};
    for (size_t i = 0; i < SDL_arraysize(xs); i++) {
        int x = (int)SDL_floorf(xs[i] / SPRITESHEET_CELL_X);
        if (x < 0 || x >= TILEMAP_SIZE_X) continue;
        if ((*map)[x][y].type) return (*map)[x][y].type;
    }
    return 0;
}

// Whether there's more than `EPSILON` of fluids on the fluid cells right on top
// of the tiles under `rect`, along its width. If there is, fills `type` with
// the fluid with the most volume on them.
static bool get_fluid_on_tiles_under(SDL_FRect rect, fluid_type* type) {
    int y = get_row_under(rect) * FLUIDS_SPLIT_Y - 1;
    int from = (int)SDL_floorf(rect.x / FLUIDS_CELL_W);
    int to = (int)SDL_floorf((rect.x + rect.w - 0.01f) / FLUIDS_CELL_W);
    if (y < 0 || from < 0 || to < from) return false;
    Uint32 w = to - from + 1;
    fluid_cell cells[w][1];
    FLUIDS_get_at(from, y, w, 1, &cells);
    double volumes[FLUIDS_TOTAL] = {0};
    double total = 0.0;
    for (Uint32 i = 0; i < w; i++) {
        for (int t = 0; t < FLUIDS_TOTAL; t++) {
            volumes[t] += cells[i][0].volume[t];
            total += cells[i][0].volume[t];
        }
    }
    if (total <= EPSILON) return false;
    *type = 0;
    for (int t = 1; t < FLUIDS_TOTAL; t++)
        if (volumes[t] > volumes[*type]) *type = t;
    return true;
}

// The sounds of the fluid with the most volume on top of the tiles under `rect`
// on `fluid_sets`, or `tile_set` if there's none or it has no sounds.
static const tile_sfx*
pick_sfx(SDL_FRect rect, const tile_sfx* fluid_sets, const tile_sfx* tile_set) {
    fluid_type fluid;
    if (get_fluid_on_tiles_under(rect, &fluid) && fluid_sets[fluid].count)
        return &fluid_sets[fluid];
    return tile_set;
}

// Plays one of the sounds of `set` at random on the next voice at its volume,
// as heard from `(x,y)`, unless it's within `sfx_cooldown` of the last one
// played.
static void play_sfx(const tile_sfx* set, double x, double y) {
    if (!set->count) return;
    Uint64 now = SDL_GetTicksNS();
    if (sfx_played &&
        now - sfx_last_ns < (Uint64)(sfx_cooldown * SDL_NS_PER_SECOND))
        return;
    if (!sfx_voices_created) {
        // Without SFX started, there's nothing to play them on
        for (int i = 0; i < TILE_SFX_VOICES_TOTAL; i++)
            if (SFX_new_voice(audio_bus_gameplay_sounds, &sfx_voices[i]))
                return;
        sfx_voices_created = true;
    }
    audio_wav* wav = set->wavs[SDL_rand(set->count)];
    SFX_play_voice_2d(
        sfx_voices[sfx_next_voice], wav, x, y, set->volume, sfx_falloff_range);
    sfx_next_voice = (sfx_next_voice + 1) % TILE_SFX_VOICES_TOTAL;
    sfx_last_ns = now;
    sfx_played = true;
}



void TILEMAP_start(void) {
    init_dynarr(&autotiles, sizeof(autotile_table));
    load_autotile(dirt_json);
    load_autotile(stone_json);
    load_step_on_particles(step_on_particles_csv);
    load_land_on_particles(land_on_particles_csv);
    load_sfx(step_sfx_files, SDL_arraysize(step_sfx_files), step_sfx, "step");
    load_sfx(land_sfx_files, SDL_arraysize(land_sfx_files), land_sfx, "land");
    load_sfx(fluids_step_sfx_files,
             SDL_arraysize(fluids_step_sfx_files),
             fluids_step_sfx,
             "fluids step");
    load_sfx(fluids_land_sfx_files,
             SDL_arraysize(fluids_land_sfx_files),
             fluids_land_sfx,
             "fluids land");
    load_sfx_volumes();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Autotiles data loaded successfully.");
}

void TILEMAP_stop(void) {
    TILEMAP_clear();
    delete_sfx_voices();
    unload_sfx(step_sfx, TILEMAP_TILE_TYPES_TOTAL);
    unload_sfx(land_sfx, TILEMAP_TILE_TYPES_TOTAL);
    unload_sfx(fluids_step_sfx, FLUIDS_TOTAL);
    unload_sfx(fluids_land_sfx, FLUIDS_TOTAL);
    SDL_memset(step_on_particles, 0, sizeof(step_on_particles));
    SDL_memset(land_on_particles, 0, sizeof(land_on_particles));
    deinit_dynarr(&autotiles);
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

void TILEMAP_draw(void) {
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
            tile t = (*map)[i][j];
            if (!t.type) continue;
            RENDERER_draw_from_spritesheet(
                SPRITESHEET_CELL_X * i - cam.x,
                SPRITESHEET_CELL_Y * j - cam.y,
                t.coords_x,
                t.coords_y,
                1,
                1,
                SDL_FLIP_NONE,
                (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f},
                t.platform ? TILEMAP_PLATFORMS_Z_INDEX : TILEMAP_Z_INDEX);
        }
    }
}



void TILEMAP_step_on(SDL_FRect rect, Sint8 dir) {
    tile_type type = get_tile_under(rect);
    if (!type) return;

    play_sfx(pick_sfx(rect, fluids_step_sfx, &step_sfx[type]),
             rect.x + rect.w / 2,
             rect.y + rect.h);

    // Thrown up and behind the body, at 45 degrees
    step_particles p = step_on_particles[type];
    if (p.particle == particle_none || !p.count) return;
    PARTICLES_emit(p.particle,
                   (particles_emit_settings){
                       .shape = particles_emit_point,
                       .pos = {rect.x + rect.w / 2, rect.y + rect.h},
                       .vel = {-dir * p.speed * COS75, -p.speed * SIN75},
                       .speed_jitter = p.speed_jitter,
                       .spread = p.spread,
                       .count = p.count,
                   });
}

void TILEMAP_land_on(SDL_FRect rect) {
    tile_type type = get_tile_under(rect);
    if (!type) return;

    play_sfx(pick_sfx(rect, fluids_land_sfx, &land_sfx[type]),
             rect.x + rect.w / 2,
             rect.y + rect.h);

    // Thrown up from the bottom row of pixels of the body
    land_particles p = land_on_particles[type];
    Uint32 count = (Uint32)SDL_lroundf(p.count_per_pixel * rect.w);
    if (p.particle == particle_none || !count) return;
    PARTICLES_emit(p.particle,
                   (particles_emit_settings){
                       .shape = particles_emit_rect,
                       .rect = {rect.x, rect.y + rect.h - 1, rect.w, 1},
                       .vel = {0, -p.speed},
                       .speed_jitter = p.speed_jitter,
                       .spread = p.spread,
                       .count = count,
                   });
}



hashmap TILEMAP_get_string_tile_types_map(void) {
    hashmap map;
    init_hashmap(&map, sizeof(tile_type));
    set_on_hashmap(&(tile_type){tile_dirt}, "dirt", &map);
    set_on_hashmap(&(tile_type){tile_stone}, "stone", &map);
    set_on_hashmap(&(tile_type){tile_wood}, "wood", &map);
    set_on_hashmap(&(tile_type){tile_metal}, "metal", &map);
    return map;
}
