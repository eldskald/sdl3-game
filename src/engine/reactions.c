#include "engine/reactions.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/audio.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/sfx.h"
#include "engine/tilemap.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>

#define REACTION_SFX_MAX 5
#define REACTION_SFX_VOICES_TOTAL 4



typedef struct {
    fluid_type fluid_a;
    fluid_type fluid_b;
    tile_type tile;
    Uint8 tile_coords_x;
    Uint8 tile_coords_y;
    // One of them picked at random each time it happens, at `sfx_volume`, from
    // `REACTIONS_AUDIO_CSV_PATH`.
    audio_wav* sfx[REACTION_SFX_MAX];
    Uint8 sfx_count;
    float sfx_volume;
} fluid_fluid_into_tile;

// A sound of the fluid fluid into tile reaction between `fluid_a` and
// `fluid_b`, in any order, into `tile`.
typedef struct {
    fluid_type fluid_a;
    fluid_type fluid_b;
    tile_type tile;
    const Uint8* mem;
    size_t size;
} ffit_sfx_file;

static const char ffit_csv[] = {
#embed REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH suffix(, '\0')
};

static const char audio_csv[] = {
#embed REACTIONS_AUDIO_CSV_PATH suffix(, '\0')
};

static const Uint8 water_lava_stone_1_wav[] = {
#embed REACTIONS_WATER_LAVA_STONE_1_SFX_PATH
};

static const ffit_sfx_file ffit_sfx_files[] = {
    {fluid_water,
     fluid_lava,
     tile_stone,
     water_lava_stone_1_wav,
     sizeof(water_lava_stone_1_wav)},
};

static fluid_fluid_into_tile* ffit_data = NULL;
static size_t ffit_count = 0;

// Shared by every reaction. Voices are created on the first sound instead of on
// `REACTIONS_start()`, so reactions don't need `SFX_start()` before them. Only
// the fluids thread plays them, so they aren't locked.
static size_t sfx_voices[REACTION_SFX_VOICES_TOTAL] = {0};
static bool sfx_voices_created = false;
static Uint8 sfx_next_voice = 0;
static Uint64 sfx_last_ns = 0;
static bool sfx_played = false;

// Seconds between any two reaction sounds, and pixels off the camera at which
// they go silent, from `REACTIONS_AUDIO_CSV_PATH`.
static double sfx_cooldown = 0.0;
static double sfx_falloff_range = 0.0;



static void load_fluid_fluid_into_tile_reactions(const char* csv) {
    dynarr parsed = CSV_parse(csv);
    ffit_count = count_csv_entries(&parsed);
    ffit_data = SDL_calloc(ffit_count, sizeof(fluid_fluid_into_tile));
    fluid_fluid_into_tile* r = ffit_data;
    parsing_field fields[] = {
        {"fluid A", parsing_fluid, &r->fluid_a},
        {"fluid B", parsing_fluid, &r->fluid_b},
        {"resulting tile", parsing_tile, &r->tile},
        {"tile spritesheet x", parsing_uint8, &r->tile_coords_x},
        {"tile spritesheet y", parsing_uint8, &r->tile_coords_y},
    };
    read_csv_table(&parsed, fields, SDL_arraysize(fields), sizeof(*r));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Fluid fluid into tile reactions data loaded successfully.");
}

static bool is_same_ffit(fluid_fluid_into_tile r,
                         fluid_type a,
                         fluid_type b,
                         tile_type tile) {
    return r.tile == tile && ((r.fluid_a == a && r.fluid_b == b) ||
                              (r.fluid_a == b && r.fluid_b == a));
}

// Its first section has the cooldown and falloff range shared by every
// reaction, and its second the table of each fluid fluid into tile reaction's
// volume, matched to the reactions by their fluids and resulting tile.
static void load_audio_data(const char* csv) {
    parsing_field keys[] = {
        {"sfx_cooldown", parsing_double, &sfx_cooldown},
        {"falloff_range", parsing_double, &sfx_falloff_range},
    };
    dynarr parsed = CSV_parse_section(csv, 0);
    read_csv_keys(&parsed, keys, SDL_arraysize(keys));
    CSV_free(&parsed);

    typedef struct {
        fluid_type fluid_a;
        fluid_type fluid_b;
        tile_type tile;
        float volume;
    } ffit_audio;
    parsed = CSV_parse_section(csv, 1);
    ffit_audio* rows =
        SDL_calloc(count_csv_entries(&parsed), sizeof(ffit_audio));
    parsing_field fields[] = {
        {"fluid A", parsing_fluid, &rows->fluid_a},
        {"fluid B", parsing_fluid, &rows->fluid_b},
        {"resulting tile", parsing_tile, &rows->tile},
        {"vol", parsing_float, &rows->volume},
    };
    size_t n =
        read_csv_table(&parsed, fields, SDL_arraysize(fields), sizeof(*rows));
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < ffit_count; j++)
            if (is_same_ffit(ffit_data[j],
                             rows[i].fluid_a,
                             rows[i].fluid_b,
                             rows[i].tile))
                ffit_data[j].sfx_volume = rows[i].volume;
    SDL_free(rows);
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Reactions audio data loaded successfully.");
}

static void load_sfx(void) {
    for (size_t i = 0; i < SDL_arraysize(ffit_sfx_files); i++) {
        ffit_sfx_file f = ffit_sfx_files[i];
        for (size_t j = 0; j < ffit_count; j++) {
            fluid_fluid_into_tile* r = &ffit_data[j];
            if (!is_same_ffit(*r, f.fluid_a, f.fluid_b, f.tile)) continue;
            if (r->sfx_count >= REACTION_SFX_MAX) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Fluid fluid into tile reaction %lu has more than "
                            "%d SFX, ignoring the rest",
                            j,
                            REACTION_SFX_MAX);
                continue;
            }
            audio_wav* wav = AUDIO_create_wav(
                "reaction", SDL_IOFromConstMem(f.mem, f.size), true);
            if (wav) r->sfx[r->sfx_count++] = wav;
        }
    }
}

static void unload_sfx(void) {
    for (size_t i = 0; i < ffit_count; i++)
        for (Uint8 j = 0; j < ffit_data[i].sfx_count; j++)
            AUDIO_destroy_wav(ffit_data[i].sfx[j]);
    if (sfx_voices_created)
        for (int i = 0; i < REACTION_SFX_VOICES_TOTAL; i++)
            SFX_del_voice(sfx_voices[i]);
    sfx_voices_created = false;
    sfx_next_voice = 0;
    sfx_played = false;
}



void REACTIONS_start(void) {
    load_fluid_fluid_into_tile_reactions(ffit_csv);
    load_audio_data(audio_csv);
    load_sfx();
}

void REACTIONS_stop(void) {
    unload_sfx();
    SDL_free(ffit_data);
    ffit_data = NULL;
    ffit_count = 0;
}



static bool has_fluid(fluid_cell cell, fluid_type type) {
    return !cell.solid &&
           cell.volume[type] >
               REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD - EPSILON;
}

static bool are_touching(fluid_cell a, fluid_cell b, fluid_fluid_into_tile r) {
    return (has_fluid(a, r.fluid_a) && has_fluid(b, r.fluid_b)) ||
           (has_fluid(a, r.fluid_b) && has_fluid(b, r.fluid_a));
}

static bool
react_inside_tile(fluid_cell (*cells)[FLUIDS_SPLIT_X][FLUIDS_SPLIT_Y],
                  fluid_fluid_into_tile r) {
    for (Uint32 i = 0; i < FLUIDS_SPLIT_X; i++) {
        for (Uint32 j = 0; j < FLUIDS_SPLIT_Y; j++) {
            fluid_cell cell = (*cells)[i][j];
            if (are_touching(cell, cell, r)) return true;
            if (i + 1 < FLUIDS_SPLIT_X &&
                are_touching(cell, (*cells)[i + 1][j], r))
                return true;
            if (j + 1 < FLUIDS_SPLIT_Y &&
                are_touching(cell, (*cells)[i][j + 1], r))
                return true;
        }
    }
    return false;
}

// Checks the tile's top row of cells against the cells right above them.
static bool react_with_top(fluid_cell (*cells)[FLUIDS_SPLIT_X][FLUIDS_SPLIT_Y],
                           fluid_cell (*above)[FLUIDS_SPLIT_X][1],
                           fluid_fluid_into_tile r) {
    for (Uint32 i = 0; i < FLUIDS_SPLIT_X; i++)
        if (are_touching((*cells)[i][0], (*above)[i][0], r)) return true;
    return false;
}

static void clear_all_fluids(Uint32 x, Uint32 y, Uint32 w, Uint32 h) {
    for (Uint8 type = 0; type < FLUIDS_TOTAL; type++)
        FLUIDS_clear_at(x, y, w, h, type);
}

// Plays one of the sounds of `r` at random on the next voice at its volume, as
// heard from `(x,y)`, unless it's within `sfx_cooldown` of the last one played.
static void play_sfx(const fluid_fluid_into_tile* r, double x, double y) {
    if (!r->sfx_count) return;
    Uint64 now = SDL_GetTicksNS();
    if (sfx_played &&
        now - sfx_last_ns < (Uint64)(sfx_cooldown * SDL_NS_PER_SECOND))
        return;
    if (!sfx_voices_created) {
        // Without SFX started, there's nothing to play them on
        for (int i = 0; i < REACTION_SFX_VOICES_TOTAL; i++)
            if (SFX_new_voice(audio_bus_gameplay_sounds, &sfx_voices[i]))
                return;
        sfx_voices_created = true;
    }
    SFX_play_voice_2d(sfx_voices[sfx_next_voice],
                      r->sfx[SDL_rand(r->sfx_count)],
                      x,
                      y,
                      r->sfx_volume,
                      sfx_falloff_range);
    sfx_next_voice = (sfx_next_voice + 1) % REACTION_SFX_VOICES_TOTAL;
    sfx_last_ns = now;
    sfx_played = true;
}

// Squared distance from `(x,y)` to `cam`, `0` inside it.
static double get_distance_sq(SDL_FRect cam, double x, double y) {
    double dx = SDL_max(SDL_max(cam.x - x, x - (cam.x + cam.w)), 0.0);
    double dy = SDL_max(SDL_max(cam.y - y, y - (cam.y + cam.h)), 0.0);
    return dx * dx + dy * dy;
}

// Of every reaction happening on a pass, only the sound of the one closest to
// the camera is played, from the center of its tile.
void REACTIONS_do_fluid_fluid_into_tile_reactions(void) {
    SDL_FRect cam = CAMERA_get_rect();
    const fluid_fluid_into_tile* closest = NULL;
    double closest_dist = 0.0, closest_x = 0.0, closest_y = 0.0;

    SDL_Rect sim = FLUIDS_get_simulated_rect();
    Uint16 from_x = sim.x / FLUIDS_SPLIT_X;
    Uint16 from_y = sim.y / FLUIDS_SPLIT_Y;
    for (Uint16 x = from_x; x < from_x + SIMULATION_SPACE_X; x++) {
        for (Uint16 y = from_y; y < from_y + SIMULATION_SPACE_Y; y++) {
            tile t = (tile){0};
            TILEMAP_get_at(x, y, &t);
            if (t.type && !t.platform) continue;

            Uint32 cx = x * FLUIDS_SPLIT_X;
            Uint32 cy = y * FLUIDS_SPLIT_Y;
            fluid_cell cells[FLUIDS_SPLIT_X][FLUIDS_SPLIT_Y];
            fluid_cell above[FLUIDS_SPLIT_X][1];
            FLUIDS_get_at(cx, cy, FLUIDS_SPLIT_X, FLUIDS_SPLIT_Y, &cells);
            if (y > 0)
                FLUIDS_get_at(cx, cy - 1, FLUIDS_SPLIT_X, 1, &above);
            else
                for (Uint32 i = 0; i < FLUIDS_SPLIT_X; i++)
                    above[i][0] = (fluid_cell){.solid = true};

            // The first reaction on the list to happen clears the fluids, so
            // the ones after it don't get checked.
            for (size_t i = 0; i < ffit_count; i++) {
                fluid_fluid_into_tile r = ffit_data[i];
                bool inside = react_inside_tile(&cells, r);
                if (!inside && !react_with_top(&cells, &above, r)) continue;

                tile product = (tile){
                    .type = r.tile,
                    .coords_x = r.tile_coords_x,
                    .coords_y = r.tile_coords_y,
                };
                TILEMAP_set_at(x, y, 1, 1, (tile(*)[1][1]) & product);
                clear_all_fluids(cx, cy, FLUIDS_SPLIT_X, FLUIDS_SPLIT_Y);
                if (!inside) clear_all_fluids(cx, cy - 1, FLUIDS_SPLIT_X, 1);

                double px = (x + 0.5) * SPRITESHEET_CELL_X;
                double py = (y + 0.5) * SPRITESHEET_CELL_Y;
                double dist = get_distance_sq(cam, px, py);
                if (!closest || dist < closest_dist) {
                    closest = &ffit_data[i];
                    closest_dist = dist;
                    closest_x = px;
                    closest_y = py;
                }
                break;
            }
        }
    }
    if (closest) play_sfx(closest, closest_x, closest_y);
}
