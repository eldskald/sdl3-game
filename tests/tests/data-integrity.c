#include "tests/data-integrity.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>

#define MAX_ERRORS 16

// Type a CSV value must have to be read by its loader.
typedef enum {
    val_double,
    val_int,
    val_uint8,
    val_sint8,
    val_fluid,
    val_tile,
} val_type;

// Row of a key/value CSV, a key followed by `min` up to `max` values.
typedef struct {
    const char* key;
    val_type type;
    size_t min;
    size_t max;
} key_spec;

// Column of a CSV with a header row, each row having a value under it.
typedef struct {
    const char* name;
    val_type type;
} column_spec;

// Checks of a single file, passed to a single `expect()`, along with messages
// of what failed to be printed after it.
typedef struct {
    bool checks[MAX_EXPECTATIONS];
    size_t len;
    char errors[MAX_ERRORS][FAIL_MESSAGE_MAX];
    size_t errors_len;
} report;

static hashmap fluid_names = (hashmap){0};
static hashmap tile_names = (hashmap){0};

static void check(report* r, bool ok, const char* fmt, ...) {
    if (r->len < MAX_EXPECTATIONS) r->checks[r->len++] = ok;
    if (ok || r->errors_len >= MAX_ERRORS) return;
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(r->errors[r->errors_len++], FAIL_MESSAGE_MAX, fmt, args);
    va_end(args);
}

static void finish(const char* path, report* r) {
    char desc[FAIL_MESSAGE_MAX] = "";
    sprintf(desc, "Checking %s...", path);
    expect(desc, r->len, r->checks);
    for (size_t i = 0; i < r->errors_len; i++)
        printf(RED "    %s" RESET "\n", r->errors[i]);
}

// Hashmap values can't tell missing keys apart from types of value 0, like
// `fluid_water`, so this looks for the key itself.
static bool has_key(hashmap* map, const char* key) {
    for (node* i = map->indexes.head; i; i = i->next)
        if (SDL_strcmp(map->keys[(size_t)i->val], key) == 0) return true;
    return false;
}

static bool is_val(const char* s, val_type type) {
    if (type == val_fluid) return has_key(&fluid_names, s);
    if (type == val_tile) return has_key(&tile_names, s);

    // `SDL_strtod()` and `SDL_strtol()` skip leading spaces, which we don't
    // want, and the whole string must be the number.
    size_t len = SDL_strlen(s);
    if (len == 0 || s[0] == ' ' || s[0] == '\t') return false;
    char* end = NULL;
    if (type == val_double) {
        SDL_strtod(s, &end);
        return end == s + len;
    }
    long n = SDL_strtol(s, &end, 10);
    if (end != s + len) return false;
    if (type == val_uint8) return n >= 0 && n <= 255;
    if (type == val_sint8) return n >= -128 && n <= 127;
    return true;
}

static const char* val_type_name(val_type type) {
    switch (type) {
    case val_double:
        return "a number";
    case val_int:
        return "an integer";
    case val_uint8:
        return "an integer from 0 to 255";
    case val_sint8:
        return "an integer from -128 to 127";
    case val_fluid:
        return "a fluid type";
    case val_tile:
        return "a tile type";
    }
    return "";
}

static dynarr* get_row(dynarr* parsed, const char* key) {
    for (size_t i = 0; i < parsed->len; i++) {
        dynarr* row = parsed->at[i];
        if (SDL_strcmp(row->at[0], key) == 0) return row;
    }
    return NULL;
}

// Checks a CSV of rows starting with a key followed by its values. Each key on
// `specs` must be there exactly once, with no other keys. With
// `allow_headers`, single cell rows are section headers and are skipped, like
// their loaders do.
static void check_keys_csv(report* r,
                           dynarr* parsed,
                           const key_spec* specs,
                           size_t n,
                           bool allow_headers) {
    size_t counts[MAX_EXPECTATIONS] = {0};
    for (size_t i = 0; i < parsed->len; i++) {
        dynarr* row = parsed->at[i];
        char* key = row->at[0];
        if (row->len == 1 && allow_headers) continue;

        size_t s = 0;
        while (s < n && SDL_strcmp(specs[s].key, key) != 0)
            s++;
        check(r, s < n, "Row %lu: unknown key \"%s\"", i + 1, key);
        if (s == n) continue;
        counts[s]++;

        size_t vals = row->len - 1;
        check(r,
              vals >= specs[s].min && vals <= specs[s].max,
              "Row %lu: \"%s\" has %lu values, expected %lu to %lu",
              i + 1,
              key,
              vals,
              specs[s].min,
              specs[s].max);
        for (size_t j = 1; j < row->len; j++)
            check(r,
                  is_val(row->at[j], specs[s].type),
                  "Row %lu: \"%s\" value \"%s\" should be %s",
                  i + 1,
                  key,
                  (char*)row->at[j],
                  val_type_name(specs[s].type));
    }
    for (size_t s = 0; s < n; s++)
        check(r,
              counts[s] == 1,
              "Key \"%s\" found %lu times, expected once",
              specs[s].key,
              counts[s]);
}

// Checks a CSV whose first row is a header naming its columns, and each row
// after it has a value under every column. Every column on `specs` must be on
// the header exactly once, with no other columns.
static void check_columns_csv(report* r,
                              dynarr* parsed,
                              const column_spec* specs,
                              size_t n) {
    check(r, parsed->len > 0, "Missing the header row");
    if (parsed->len == 0) return;

    dynarr* header = parsed->at[0];
    size_t indexes[MAX_EXPECTATIONS] = {0};
    size_t counts[MAX_EXPECTATIONS] = {0};
    for (size_t c = 0; c < header->len; c++) {
        size_t s = 0;
        while (s < n && SDL_strcmp(specs[s].name, header->at[c]) != 0)
            s++;
        check(r, s < n, "Unknown column \"%s\"", (char*)header->at[c]);
        indexes[c] = s;
        if (s < n) counts[s]++;
    }
    for (size_t s = 0; s < n; s++)
        check(r,
              counts[s] == 1,
              "Column \"%s\" found %lu times, expected once",
              specs[s].name,
              counts[s]);

    for (size_t i = 1; i < parsed->len; i++) {
        dynarr* row = parsed->at[i];
        check(r,
              row->len == header->len,
              "Row %lu has %lu values, expected %lu",
              i + 1,
              row->len,
              header->len);
        for (size_t c = 0; c < row->len && c < header->len; c++) {
            if (indexes[c] == n) continue;
            check(r,
                  is_val(row->at[c], specs[indexes[c]].type),
                  "Row %lu: \"%s\" value \"%s\" should be %s",
                  i + 1,
                  specs[indexes[c]].name,
                  (char*)row->at[c],
                  val_type_name(specs[indexes[c]].type));
        }
    }
}

// Index of column `name` on the header, or `-1` if missing.
static int get_column(dynarr* parsed, const char* name) {
    dynarr* header = parsed->at[0];
    for (size_t c = 0; c < header->len; c++)
        if (SDL_strcmp(header->at[c], name) == 0) return c;
    return -1;
}



static void check_bloom(void) {
    char csv[] = {
#embed RENDERER_BLOOM_CSV_PATH suffix(, '\0')
    };
    const key_spec specs[] = {
        {"threshold", val_double, 1, 1},
        {"soft_threshold", val_double, 1, 1},
        {"intensities", val_double, 1, RENDERER_BLOOM_MAX_ITERATIONS},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs), false);
    CSV_free(&parsed);
    finish(RENDERER_BLOOM_CSV_PATH, &r);
}

static void check_ripples(void) {
    char csv[] = {
#embed FLUIDS_RIPPLES_CSV_PATH suffix(, '\0')
    };
    const key_spec specs[] = {
        {"body_max_speed", val_double, 1, 1},
        {"new_ripple_pix_size", val_int, 1, 1},
        {"strength", val_double, 1, 1},
        {"damping", val_double, 1, 1},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs), false);
    CSV_free(&parsed);
    finish(FLUIDS_RIPPLES_CSV_PATH, &r);
}

static void check_camera(void) {
    char csv[] = {
#embed CAMERA_CSV_PATH suffix(, '\0')
    };
    const key_spec specs[] = {
        {"speed", val_double, 1, 1},
        {"smoothing_range", val_double, 1, 1},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs), false);
    CSV_free(&parsed);
    finish(CAMERA_CSV_PATH, &r);
}

static void check_player_core(void) {
    char csv[] = {
#embed PLAYER_BASE_CSV_PATH suffix(, '\0')
    };
    const key_spec specs[] = {
        {"max_speed", val_double, 1, 1},
        {"acceleration", val_double, 1, 1},
        {"friction", val_double, 1, 1},
        {"gravity", val_double, 1, 1},
        {"max_fall_speed", val_double, 1, 1},
        {"jump_speed", val_double, 1, 1},
        {"jump_cut", val_double, 1, 1},
        {"coyote_jump_time", val_double, 1, 1},
        {"jump_buffer_time", val_double, 1, 1},
        {"swimming_max_speed", val_double, 1, 1},
        {"swimming_max_fall_speed", val_double, 1, 1},
        {"swimming_acceleration", val_double, 1, 1},
        {"swimming_friction", val_double, 1, 1},
        {"swimming_gravity", val_double, 1, 1},
        {"swimming_swim_speed", val_double, 1, 1},
        {"width", val_uint8, 1, 1},
        {"height", val_uint8, 1, 1},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs), true);
    CSV_free(&parsed);
    finish(PLAYER_BASE_CSV_PATH, &r);
}

static void check_player_presentation(void) {
    char csv[] = {
#embed PLAYER_SPRITE_CSV_PATH suffix(, '\0')
    };
    const char* anims[] = {"idle_anim",
                           "moving_anim",
                           "airborne_anim",
                           "crouch_idle_anim",
                           "crouch_moving_anim",
                           "swimming_anim"};
    char keys[SDL_arraysize(anims)][3][KEY_MAX_SIZE] = {0};
    key_spec specs[2 + 3 * SDL_arraysize(anims)] = {
        {"coords", val_uint8, 4, 4},
        {"offset", val_sint8, 2, 2},
    };
    for (size_t a = 0; a < SDL_arraysize(anims); a++) {
        sprintf(keys[a][0], "%s_x", anims[a]);
        sprintf(keys[a][1], "%s_y", anims[a]);
        sprintf(keys[a][2], "%s_dur", anims[a]);
        specs[2 + 3 * a] =
            (key_spec){keys[a][0], val_uint8, 1, SPRITES_ANIM_MAX_KEYFRAMES};
        specs[3 + 3 * a] =
            (key_spec){keys[a][1], val_uint8, 1, SPRITES_ANIM_MAX_KEYFRAMES};
        specs[4 + 3 * a] =
            (key_spec){keys[a][2], val_double, 1, SPRITES_ANIM_MAX_KEYFRAMES};
    }
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs), false);

    // Each keyframe of an animation needs its x, y and duration.
    for (size_t a = 0; a < SDL_arraysize(anims); a++) {
        dynarr* x = get_row(&parsed, keys[a][0]);
        dynarr* y = get_row(&parsed, keys[a][1]);
        dynarr* dur = get_row(&parsed, keys[a][2]);
        if (!x || !y || !dur) continue;
        check(&r,
              x->len == y->len && y->len == dur->len,
              "\"%s\" has %lu x, %lu y and %lu durations, expected the same",
              anims[a],
              x->len - 1,
              y->len - 1,
              dur->len - 1);
    }
    CSV_free(&parsed);
    finish(PLAYER_SPRITE_CSV_PATH, &r);
}

static void check_fluid_particles(void) {
    char csv[] = {
#embed FLUIDS_PARTICLES_CSV_PATH suffix(, '\0')
    };
    const key_spec specs[] = {
        {"gravity", val_double, 1, 1},
        {"size_threshold", val_double, 1, 1},
        {"fan_angle", val_double, 1, 1},
        {"angle_jitter", val_double, 1, 1},
        {"particle_max_speed", val_double, 1, 1},
        {"volume", val_double, 1, 1},
        {"body_max_speed", val_double, 1, 1},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs), true);
    CSV_free(&parsed);
    finish(FLUIDS_PARTICLES_CSV_PATH, &r);
}

static void check_fluids(void) {
    char csv[] = {
#embed FLUIDS_CSV_PATH suffix(, '\0')
    };
    const column_spec specs[] = {
        {"fluid", val_fluid},
        {"density", val_double},
        {"move_down_time", val_double},
        {"move_diag_time", val_double},
        {"move_side_time", val_double},
        {"min_spread", val_double},
        {"particle_vol", val_double},
        {"col_r", val_uint8},
        {"col_g", val_uint8},
        {"col_b", val_uint8},
        {"col_a", val_uint8},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_columns_csv(&r, &parsed, specs, SDL_arraysize(specs));

    // The loader reads exactly one row for each fluid.
    int col = parsed.len > 0 ? get_column(&parsed, "fluid") : -1;
    for (node* i = fluid_names.indexes.head; i && col >= 0; i = i->next) {
        char* name = fluid_names.keys[(size_t)i->val];
        size_t count = 0;
        for (size_t j = 1; j < parsed.len; j++) {
            dynarr* row = parsed.at[j];
            if (col < row->len && SDL_strcmp(row->at[col], name) == 0) count++;
        }
        check(&r,
              count == 1,
              "Fluid \"%s\" found %lu times, expected once",
              name,
              count);
    }
    check(&r,
          parsed.len == FLUIDS_TOTAL + 1,
          "Has %lu fluids, expected %d",
          parsed.len > 0 ? parsed.len - 1 : 0,
          FLUIDS_TOTAL);
    CSV_free(&parsed);
    finish(FLUIDS_CSV_PATH, &r);
}

static void check_fluid_fluid_into_tile(void) {
    char csv[] = {
#embed REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH suffix(, '\0')
    };
    const column_spec specs[] = {
        {"fluid A", val_fluid},
        {"fluid B", val_fluid},
        {"resulting tile", val_tile},
        {"tile spritesheet x", val_uint8},
        {"tile spritesheet y", val_uint8},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_columns_csv(&r, &parsed, specs, SDL_arraysize(specs));
    CSV_free(&parsed);
    finish(REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH, &r);
}

static void check_platforms(void) {
    char csv[] = {
#embed CHUNKS_PLATFORMS_CSV_PATH suffix(, '\0')
    };
    const column_spec specs[] = {
        {"x", val_uint8},
        {"y", val_uint8},
        {"w", val_uint8},
        {"h", val_uint8},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_columns_csv(&r, &parsed, specs, SDL_arraysize(specs));
    CSV_free(&parsed);
    finish(CHUNKS_PLATFORMS_CSV_PATH, &r);
}

static void check_types_map(void) {
    char csv[] = {
#embed CHUNKS_TYPES_MAP_CSV_PATH suffix(, '\0')
    };
    const column_spec specs[] = {
        {"type", val_tile},
        {"x", val_uint8},
        {"y", val_uint8},
        {"w", val_uint8},
        {"h", val_uint8},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_columns_csv(&r, &parsed, specs, SDL_arraysize(specs));
    CSV_free(&parsed);
    finish(CHUNKS_TYPES_MAP_CSV_PATH, &r);
}



void test_data_integrity(void) {
    printf(YELLOW "\nTesting data integrity...\n" RESET);

    fluid_names = FLUIDS_get_string_fluid_types_map();
    tile_names = TILEMAP_get_string_tile_types_map();

    check_bloom();
    check_ripples();
    check_camera();
    check_player_core();
    check_player_presentation();
    check_fluid_particles();
    check_fluids();
    check_fluid_fluid_into_tile();
    check_platforms();
    check_types_map();

    clear_hashmap(&fluid_names);
    clear_hashmap(&tile_names);
}
