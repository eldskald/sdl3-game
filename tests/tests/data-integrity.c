#include "tests/data-integrity.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
// The real files are checked, not the mocks on `tests/paths.h`.
#include "../../src/paths.h"
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
    val_bool,
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
    // Past `MAX_EXPECTATIONS`, failures go on the last check so they still fail
    // the file.
    if (r->len < MAX_EXPECTATIONS)
        r->checks[r->len++] = ok;
    else if (!ok)
        r->checks[MAX_EXPECTATIONS - 1] = false;
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

static bool is_val(const char* s, val_type type) {
    if (type == val_fluid) return get_from_hashmap(s, &fluid_names);
    if (type == val_tile) return get_from_hashmap(s, &tile_names);

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
    if (type == val_bool) return n == 0 || n == 1;
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
    case val_bool:
        return "0 or 1";
    case val_fluid:
        return "a fluid type";
    case val_tile:
        return "a tile type";
    }
    return "";
}

static dynarr* get_row(dynarr* parsed, const char* key) {
    for (size_t i = 0; i < parsed->len; i++) {
        dynarr* row = CSV_row(i, parsed);
        if (SDL_strcmp(CSV_cell(0, row), key) == 0) return row;
    }
    return NULL;
}

// Checks a CSV of rows starting with a key followed by its values. Each key on
// `specs` must be there exactly once, with no other keys. Single cell rows
// that aren't keys are section headers and are skipped, while ones that are
// keys have no values, which is fine for lists that can be empty.
static void
check_keys_csv(report* r, dynarr* parsed, const key_spec* specs, size_t n) {
    size_t counts[MAX_EXPECTATIONS] = {0};
    for (size_t i = 0; i < parsed->len; i++) {
        dynarr* row = CSV_row(i, parsed);
        char* key = CSV_cell(0, row);

        size_t s = 0;
        while (s < n && SDL_strcmp(specs[s].key, key) != 0)
            s++;
        if (row->len < 2 && s == n) continue;
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
                  is_val(CSV_cell(j, row), specs[s].type),
                  "Row %lu: \"%s\" value \"%s\" should be %s",
                  i + 1,
                  key,
                  CSV_cell(j, row),
                  val_type_name(specs[s].type));
    }
    for (size_t s = 0; s < n; s++)
        check(r,
              counts[s] == 1,
              "Key \"%s\" found %lu times, expected once",
              specs[s].key,
              counts[s]);
}

// Index of the row naming the columns of a CSV, the first one that isn't a
// single cell section header, or `-1` if missing.
static int get_header(dynarr* parsed) {
    for (size_t i = 0; i < parsed->len; i++)
        if (CSV_row(i, parsed)->len >= 2) return i;
    return -1;
}

// Checks a CSV whose first row is a header naming its columns, and each row
// after it has a value under every column. Every column on `specs` must be on
// the header exactly once, with no other columns. Single cell rows are section
// headers and are skipped, like their loaders do.
static void check_columns_csv(report* r,
                              dynarr* parsed,
                              const column_spec* specs,
                              size_t n) {
    int h = get_header(parsed);
    check(r, h >= 0, "Missing the header row");
    if (h < 0) return;

    dynarr* header = CSV_row(h, parsed);
    size_t indexes[MAX_EXPECTATIONS] = {0};
    size_t counts[MAX_EXPECTATIONS] = {0};
    for (size_t c = 0; c < header->len; c++) {
        size_t s = 0;
        while (s < n && SDL_strcmp(specs[s].name, CSV_cell(c, header)) != 0)
            s++;
        check(r, s < n, "Unknown column \"%s\"", CSV_cell(c, header));
        indexes[c] = s;
        if (s < n) counts[s]++;
    }
    for (size_t s = 0; s < n; s++)
        check(r,
              counts[s] == 1,
              "Column \"%s\" found %lu times, expected once",
              specs[s].name,
              counts[s]);

    for (size_t i = h + 1; i < parsed->len; i++) {
        dynarr* row = CSV_row(i, parsed);
        if (row->len < 2) continue;
        check(r,
              row->len == header->len,
              "Row %lu has %lu values, expected %lu",
              i + 1,
              row->len,
              header->len);
        for (size_t c = 0; c < row->len && c < header->len; c++) {
            if (indexes[c] == n) continue;
            check(r,
                  is_val(CSV_cell(c, row), specs[indexes[c]].type),
                  "Row %lu: \"%s\" value \"%s\" should be %s",
                  i + 1,
                  specs[indexes[c]].name,
                  CSV_cell(c, row),
                  val_type_name(specs[indexes[c]].type));
        }
    }
}

// Index of column `name` on the header, or `-1` if missing.
static int get_column(dynarr* parsed, const char* name) {
    int h = get_header(parsed);
    if (h < 0) return -1;
    dynarr* header = CSV_row(h, parsed);
    for (size_t c = 0; c < header->len; c++)
        if (SDL_strcmp(CSV_cell(c, header), name) == 0) return c;
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
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs));
    CSV_free(&parsed);
    finish(RENDERER_BLOOM_CSV_PATH, &r);
}

static void check_ripples(void) {
    char csv[] = {
#embed FLUIDS_RIPPLES_CSV_PATH suffix(, '\0')
    };
    const key_spec specs[] = {
        {"body_max_speed", val_double, 1, 1},
        {"prop_max_speed", val_double, 1, 1},
        {"prop_min_speed", val_double, 1, 1},
        {"new_body_ripple_pix_size", val_int, 1, 1},
        {"new_prop_ripple_pix_size", val_int, 1, 1},
        {"body_ripple_strength", val_double, 1, 1},
        {"prop_ripple_strength", val_double, 1, 1},
        {"damping", val_double, 1, 1},
        {"refraction", val_double, 1, 1},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs));
    CSV_free(&parsed);
    finish(FLUIDS_RIPPLES_CSV_PATH, &r);
}

static void check_props(void) {
    char csv[] = {
#embed PHYSICS_PROPS_CSV_PATH suffix(, '\0')
    };
    const key_spec specs[] = {
        {"prop_body_collision_pad", val_double, 1, 1},
        {"prop_global_damping", val_double, 1, 1},
        {"vine_color", val_uint8, 4, 4},
        {"vine_gravity", val_double, 1, 1},
        {"vine_fluid_gravity", val_double, 1, 1},
        {"vine_destroyer_fluids", val_fluid, 0, FLUIDS_TOTAL},
        {"vine_cut_duration", val_double, 1, 1},
        {"vine_fade_time", val_double, 1, 1},
    };
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs));
    CSV_free(&parsed);
    finish(PHYSICS_PROPS_CSV_PATH, &r);
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
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs));
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
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs));
    CSV_free(&parsed);
    finish(PLAYER_BASE_CSV_PATH, &r);
}

// Checks the pins of the player's prop `prop`, mapped by frame on its
// `<prop>_anim_x` and `<prop>_anim_y` rows with a value on each of `pin_keys`
// for each frame. Frames are mapped by spritesheet cell, so they must be cells
// of the spritesheet, and every keyframe of the animations must be mapped,
// otherwise the prop falls back to the pins of whichever frame was mapped
// before. Mapped frames no animation uses are fine, they're drawn on the
// spritesheet and ready to be used.
static void check_pins(report* r,
                       dynarr* parsed,
                       const char* prop,
                       const char** pin_keys,
                       size_t n,
                       const char** anims,
                       char (*keys)[3][KEY_MAX_SIZE],
                       size_t anims_n) {
    char x_key[KEY_MAX_SIZE] = "";
    char y_key[KEY_MAX_SIZE] = "";
    sprintf(x_key, "%s_anim_x", prop);
    sprintf(y_key, "%s_anim_y", prop);
    dynarr* anim_x = get_row(parsed, x_key);
    dynarr* anim_y = get_row(parsed, y_key);
    if (!anim_x || !anim_y) return;

    for (size_t k = 0; k < n; k++) {
        dynarr* row = get_row(parsed, pin_keys[k]);
        if (!row) continue;
        check(r,
              row->len == anim_x->len,
              "\"%s\" has %lu values, expected %lu like \"%s\"",
              pin_keys[k],
              row->len - 1,
              anim_x->len - 1,
              x_key);
    }

    bool is_mapped[SPRITESHEET_X / SPRITESHEET_CELL_X]
                  [SPRITESHEET_Y / SPRITESHEET_CELL_Y] = {0};
    for (size_t i = 1; i < anim_x->len && i < anim_y->len; i++) {
        long x = SDL_strtol(CSV_cell(i, anim_x), NULL, 10);
        long y = SDL_strtol(CSV_cell(i, anim_y), NULL, 10);
        bool inside = x < SPRITESHEET_X / SPRITESHEET_CELL_X &&
                      y < SPRITESHEET_Y / SPRITESHEET_CELL_Y;
        check(r,
              inside,
              "%s frame %lu at %ld,%ld is outside the spritesheet",
              prop,
              i,
              x,
              y);
        if (inside) is_mapped[x][y] = true;
    }
    for (size_t a = 0; a < anims_n; a++) {
        dynarr* x = get_row(parsed, keys[a][0]);
        dynarr* y = get_row(parsed, keys[a][1]);
        for (size_t i = 1; x && y && i < x->len && i < y->len; i++) {
            long cx = SDL_strtol(CSV_cell(i, x), NULL, 10);
            long cy = SDL_strtol(CSV_cell(i, y), NULL, 10);
            if (cx >= SPRITESHEET_X / SPRITESHEET_CELL_X ||
                cy >= SPRITESHEET_Y / SPRITESHEET_CELL_Y)
                continue;
            check(r,
                  is_mapped[cx][cy],
                  "\"%s\" keyframe %lu at %ld,%ld has no %s pins",
                  anims[a],
                  i,
                  cx,
                  cy,
                  prop);
        }
    }
}

static void check_player_presentation(void) {
    char csv[] = {
#embed PLAYER_PRESENTATION_CSV_PATH suffix(, '\0')
    };
    const char* anims[] = {"idle_anim",
                           "moving_anim",
                           "airborne_anim",
                           "crouch_idle_anim",
                           "crouch_moving_anim",
                           "swimming_anim"};
    char keys[SDL_arraysize(anims)][3][KEY_MAX_SIZE] = {0};
    // Every cell of the spritesheet can be mapped once
    const size_t cells = (SPRITESHEET_X / SPRITESHEET_CELL_X) *
                         (SPRITESHEET_Y / SPRITESHEET_CELL_Y);
    const key_spec base_specs[] = {
        {"coords", val_uint8, 4, 4},
        {"offset", val_sint8, 2, 2},
        {"hair_col", val_uint8, 4, 4},
        {"hair_gravity", val_double, 1, 1},
        {"hair_fluid_gravity", val_double, 1, 1},
        {"hair_damping", val_double, 1, 1},
        {"hair_fluid_damping", val_double, 1, 1},
        {"hair_segments", val_uint8, 1, 1},
        {"hair_segments_length", val_double, 1, 1},
        {"hair_segments_two_ways", val_bool, 1, 1},
        {"hair_anim_x", val_uint8, 1, cells},
        {"hair_anim_y", val_uint8, 1, cells},
        {"hair_pin_x", val_uint8, 1, cells},
        {"hair_pin_y", val_uint8, 1, cells},
        {"cloak_out_col", val_uint8, 4, 4},
        {"cloak_in_col", val_uint8, 4, 4},
        {"cloak_gravity", val_double, 1, 1},
        {"cloak_fluid_gravity", val_double, 1, 1},
        {"cloak_damping", val_double, 1, 1},
        {"cloak_fluid_damping", val_double, 1, 1},
        {"cloak_horiz_segments", val_uint8, 1, 1},
        {"cloak_vert_segments", val_uint8, 1, 1},
        {"cloak_horiz_segment_length", val_double, 1, 1},
        {"cloak_vert_segment_length", val_double, 1, 1},
        {"cloak_horiz_segment_two_ways", val_bool, 1, 1},
        {"cloak_vert_segment_two_ways", val_bool, 1, 1},
        {"cloak_anim_x", val_uint8, 1, cells},
        {"cloak_anim_y", val_uint8, 1, cells},
        {"cloak_pin_l_x", val_uint8, 1, cells},
        {"cloak_pin_l_y", val_uint8, 1, cells},
        {"cloak_pin_r_x", val_uint8, 1, cells},
        {"cloak_pin_r_y", val_uint8, 1, cells},
    };
    const size_t base_n = SDL_arraysize(base_specs);
    key_spec specs[SDL_arraysize(base_specs) + 3 * SDL_arraysize(anims)];
    SDL_memcpy(specs, base_specs, sizeof(base_specs));
    for (size_t a = 0; a < SDL_arraysize(anims); a++) {
        sprintf(keys[a][0], "%s_x", anims[a]);
        sprintf(keys[a][1], "%s_y", anims[a]);
        sprintf(keys[a][2], "%s_dur", anims[a]);
        specs[base_n + 3 * a] =
            (key_spec){keys[a][0], val_uint8, 1, SPRITES_ANIM_MAX_KEYFRAMES};
        specs[base_n + 1 + 3 * a] =
            (key_spec){keys[a][1], val_uint8, 1, SPRITES_ANIM_MAX_KEYFRAMES};
        specs[base_n + 2 + 3 * a] =
            (key_spec){keys[a][2], val_double, 1, SPRITES_ANIM_MAX_KEYFRAMES};
    }
    report r = (report){0};
    dynarr parsed = CSV_parse(csv);
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs));

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

    // The hair's and cloak's points are kept on arrays of fixed size.
    const char* segment_keys[] = {
        "hair_segments", "cloak_horiz_segments", "cloak_vert_segments"};
    const long segment_maxes[] = {PLAYER_HAIR_MAX_SEGMENTS,
                                  PLAYER_CLOAK_MAX_HORIZ_SEGMENTS,
                                  PLAYER_CLOAK_MAX_VERT_SEGMENTS};
    for (size_t k = 0; k < SDL_arraysize(segment_keys); k++) {
        dynarr* row = get_row(&parsed, segment_keys[k]);
        if (!row || row->len < 2) continue;
        long n = SDL_strtol(CSV_cell(1, row), NULL, 10);
        check(&r,
              n >= 1 && n <= segment_maxes[k],
              "\"%s\" is %ld, expected 1 to %ld",
              segment_keys[k],
              n,
              segment_maxes[k]);
    }

    const char* hair_keys[] = {"hair_anim_y", "hair_pin_x", "hair_pin_y"};
    const char* cloak_keys[] = {"cloak_anim_y",
                                "cloak_pin_l_x",
                                "cloak_pin_l_y",
                                "cloak_pin_r_x",
                                "cloak_pin_r_y"};
    check_pins(&r,
               &parsed,
               "hair",
               hair_keys,
               SDL_arraysize(hair_keys),
               anims,
               keys,
               SDL_arraysize(anims));
    check_pins(&r,
               &parsed,
               "cloak",
               cloak_keys,
               SDL_arraysize(cloak_keys),
               anims,
               keys,
               SDL_arraysize(anims));
    CSV_free(&parsed);
    finish(PLAYER_PRESENTATION_CSV_PATH, &r);
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
    check_keys_csv(&r, &parsed, specs, SDL_arraysize(specs));
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
    int h = get_header(&parsed);
    int col = get_column(&parsed, "fluid");
    size_t fluids = 0;
    for (size_t j = h + 1; h >= 0 && j < parsed.len; j++)
        if (CSV_row(j, &parsed)->len >= 2) fluids++;
    for (size_t i = 0; i < fluid_names.len; i++) {
        const char* name = hashmap_key_at(i, &fluid_names);
        size_t count = 0;
        for (size_t j = h + 1; j < parsed.len; j++) {
            dynarr* row = CSV_row(j, &parsed);
            if (row->len < 2) continue;
            if (col < row->len && SDL_strcmp(CSV_cell(col, row), name) == 0)
                count++;
        }
        check(&r,
              count == 1,
              "Fluid \"%s\" found %lu times, expected once",
              name,
              count);
    }
    check(&r,
          fluids == FLUIDS_TOTAL,
          "Has %lu fluids, expected %d",
          fluids,
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
    check_props();
    check_camera();
    check_player_core();
    check_player_presentation();
    check_fluid_particles();
    check_fluids();
    check_fluid_fluid_into_tile();
    check_platforms();
    check_types_map();

    deinit_hashmap(&fluid_names);
    deinit_hashmap(&tile_names);
}
