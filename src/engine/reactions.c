#include "engine/reactions.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>



typedef struct {
    fluid_type fluid_a;
    fluid_type fluid_b;
    tile_type tile;
    Uint8 tile_coords_x;
    Uint8 tile_coords_y;
} fluid_fluid_into_tile;

static fluid_fluid_into_tile* ffit_data = NULL;
static size_t ffit_count = 0;



static char* read_col(dynarr* row, char* col, hashmap* props) {
    return (char*)row->at[(size_t)get_from_hashmap(col, props)];
}

static void load_fluid_fluid_into_tile_reactions(const char* csv) {
    hashmap fluids_map = FLUIDS_get_string_fluid_types_map();
    hashmap tiles_map = TILEMAP_get_string_tile_types_map();

    dynarr parsed = CSV_parse(csv);

    // Get hashmap of property indexes
    hashmap props = (hashmap){0};
    size_t props_count = ((dynarr*)parsed.at[0])->len;
    init_hashmap(&props);
    for (size_t i = 0; i < props_count; i++)
        set_on_hashmap((void*)i, (char*)((dynarr*)parsed.at[0])->at[i], &props);

    // Set fluid fluid into tile reactions data
    size_t total_reactions = parsed.len - 1;
    ffit_data = SDL_calloc(total_reactions, sizeof(fluid_fluid_into_tile));
    ffit_count = total_reactions;
    for (size_t i = 1; i <= total_reactions; i++) {
        dynarr* row = (dynarr*)parsed.at[i];
        char* fluid_a = read_col(row, "fluid A", &props);
        char* fluid_b = read_col(row, "fluid B", &props);
        char* tile = read_col(row, "resulting tile", &props);
        char* coords_x = read_col(row, "tile spritesheet x", &props);
        char* coords_y = read_col(row, "tile spritesheet y", &props);
        ffit_data[i - 1] = (fluid_fluid_into_tile){
            .fluid_a =
                (fluid_type)(size_t)get_from_hashmap(fluid_a, &fluids_map),
            .fluid_b =
                (fluid_type)(size_t)get_from_hashmap(fluid_b, &fluids_map),
            .tile = (tile_type)(size_t)get_from_hashmap(tile, &tiles_map),
            .tile_coords_x = SDL_strtol(coords_x, NULL, 10),
            .tile_coords_y = SDL_strtol(coords_y, NULL, 10),
        };
    }

    clear_hashmap(&fluids_map);
    clear_hashmap(&tiles_map);
    clear_hashmap(&props);
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Fluid fluid into tile reactions data loaded successfully.");
}



void REACTIONS_start(void) {
#ifndef TEST
    char ffit_csv[] = {
#embed REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH suffix(, '\0')
    };
#else
    char ffit_csv[] = {
#embed TESTS_REACTIONS_FFIT_CSV_PATH suffix(, '\0')
    };
#endif
    load_fluid_fluid_into_tile_reactions(ffit_csv);
}

void REACTIONS_stop(void) {
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

void REACTIONS_do_fluid_fluid_into_tile_reactions(void) {
    SDL_Rect sim = FLUIDS_get_simulated_rect();
    Uint16 from_x = sim.x / FLUIDS_SPLIT_X;
    Uint16 from_y = sim.y / FLUIDS_SPLIT_Y;
    for (Uint16 x = from_x; x < from_x + FLUIDS_SIMULATE_X; x++) {
        for (Uint16 y = from_y; y < from_y + FLUIDS_SIMULATE_Y; y++) {
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
                break;
            }
        }
    }
}
