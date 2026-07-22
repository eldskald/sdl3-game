#include "engine/fluids.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/game-manager.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include <SDL3/SDL.h>

#ifdef TEST
#include "stubs.h"
#endif



typedef struct {
    double gravity;
    double flow;
    Uint8 flow_range;
    Uint8 col_r;
    Uint8 col_g;
    Uint8 col_b;
    Uint8 col_a;
} fluid_data;

typedef struct {
    double volume[FLUIDS_TOTAL];
} fluids;

typedef struct {
    Uint32 x;
    Uint32 y;
    Uint32 w;
    Uint32 h;
    fluid_type type;
    double volume;
    bool clear;
} mod;



static fluids grid[TILEMAP_SIZE_X * FLUIDS_SPLIT_X]
                  [TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y] = {0};

static fluid_data data[FLUIDS_TOTAL] = {0};

static dynarr mods = (dynarr){0};



static void load_fluid_data(void) {
#ifndef TEST
    char csv[] = {
#embed FLUIDS_CSV_PATH suffix(, '\0')
    };
#else
    char csv[] = {
#embed "tests/mocks/fluids.csv" suffix(, '\0')
    };
#endif

    dynarr parsed = CSV_parse(csv);

    // Get hashmap of property indexes
    hashmap props = (hashmap){0};
    int props_count = ((dynarr*)parsed.at[0])->len - 1;
    init_hashmap(&props);
    for (size_t i = 1; i <= props_count; i++)
        set_on_hashmap((void*)i, (char*)((dynarr*)parsed.at[0])->at[i], &props);

    // Get hashmap of fluid names
    hashmap names = (hashmap){0};
    init_hashmap(&names);
    set_on_hashmap((void*)fluid_water, "water", &names);
    set_on_hashmap((void*)fluid_lava, "lava", &names);

    // Sets fluids data
    for (size_t i = 1; i <= 2; i++) {
        dynarr* row = (dynarr*)parsed.at[i];
        char* name = (char*)row->at[0];
        data[(size_t)get_from_hashmap(name, &names)] = (fluid_data){
            .gravity = SDL_strtod(
                (char*)row->at[(size_t)get_from_hashmap("gravity", &props)],
                NULL),
            .flow = SDL_strtod(
                (char*)row->at[(size_t)get_from_hashmap("flow", &props)], NULL),
            .flow_range = SDL_strtol(
                (char*)row->at[(size_t)get_from_hashmap("flow_range", &props)],
                NULL,
                10),
            .col_r = SDL_strtol(
                (char*)row->at[(size_t)get_from_hashmap("col_r", &props)],
                NULL,
                10),
            .col_g = SDL_strtol(
                (char*)row->at[(size_t)get_from_hashmap("col_g", &props)],
                NULL,
                10),
            .col_b = SDL_strtol(
                (char*)row->at[(size_t)get_from_hashmap("col_b", &props)],
                NULL,
                10),
            .col_a = SDL_strtol(
                (char*)row->at[(size_t)get_from_hashmap("col_a", &props)],
                NULL,
                10),
        };
    }

    clear_hashmap(&names);
    clear_hashmap(&props);
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Fluid data loaded successfully.");
}

void FLUIDS_start(void) {
    load_fluid_data();
    init_dynarr(&mods);
}

void FLUIDS_stop(void) {
    clear_dynarr(&mods);
    SDL_memset(grid, 0, sizeof(grid));
}



static Uint16 fluid_grid_to_tilemap_x(Uint32 x) {
    return x / FLUIDS_SPLIT_X;
}

static Uint16 fluid_grid_to_tilemap_y(Uint32 y) {
    return y / FLUIDS_SPLIT_Y;
}

static bool is_solid(Uint32 x, Uint32 y) {
    Uint16 tile_x = fluid_grid_to_tilemap_x(x);
    Uint16 tile_y = fluid_grid_to_tilemap_y(y);
    tile data = (tile){0};
    TILEMAP_get_at(tile_x, tile_y, &data);
    return data.mat;
}

#define TOP [1][0]
#define BOT [1][2]
#define LEFT [0][1]
#define RIGHT [2][1]
#define TLEFT [0][0]
#define TRIGHT [2][0]
#define BLEFT [0][2]
#define BRIGHT [2][2]
#define SELF [1][1]

static void get_neighbors(Uint32 x, Uint32 y, fluid_cell (*nbs)[3][4]) {
    for (long i = (long)x - 1; i <= (long)x + 1; i++) {
        for (long j = (long)y - 1; j <= (long)y + 2; j++) {
            if (i < 0 || j < 0 || i >= TILEMAP_SIZE_X * FLUIDS_SPLIT_X ||
                j >= TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y) {
                (*nbs)[i - x + 1][j - y + 1] = (fluid_cell){.solid = true};
                continue;
            }
            fluid_cell cell = (fluid_cell){0};
            cell.solid = is_solid(i, j);
            for (Uint8 type = 0; type < FLUIDS_TOTAL; type++)
                cell.volume[type] = grid[i][j].volume[type];
            (*nbs)[i - x + 1][j - y + 1] = cell;
        }
    }
    // Blocking off diagonals if both up/down left/right neighbors are blocked
    if ((*nbs)TOP.solid && (*nbs)LEFT.solid) (*nbs) TLEFT.solid = true;
    if ((*nbs)TOP.solid && (*nbs)RIGHT.solid) (*nbs) TRIGHT.solid = true;
    if ((*nbs)BOT.solid && (*nbs)RIGHT.solid) (*nbs) BRIGHT.solid = true;
    if ((*nbs)BOT.solid && (*nbs)LEFT.solid) (*nbs) BLEFT.solid = true;
}

static void flow_step(double vol,
                      fluid_type type,
                      int steps,
                      int dir,
                      Uint32 x,
                      Uint32 y,
                      fluids (*new)[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X]
                                   [FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y]) {
    (*new)[x][y].volume[type] += vol;
    fluid_cell nbs[3][4] = {0};
    get_neighbors(x, y, &nbs);

    bool b = false;
    bool t = false;
    if (!nbs[1 + dir][2].solid &&
        nbs[1 + dir][2].volume[type] < 1.0f - EPSILON &&
        nbs[1 + dir][2].volume[type] > EPSILON && !nbs[1 + dir][1].solid &&
        (nbs[1 + dir][3].solid ||
         nbs[1 + dir][3].volume[type] > 1.0f - EPSILON) &&
        (nbs[1 + dir][1].volume[type] < EPSILON ||
         nbs[1 + dir][1].volume[type] > 1.0f - EPSILON))
        b = true;
    else if (!nbs[1 + dir][1].solid && nbs[1 + dir][1].volume[type] < vol &&
             (nbs[1 + dir][2].solid ||
              nbs[1 + dir][2].volume[type] > 1.0f - EPSILON))
        t = true;

    if (steps <= 1) return;
    if (b) {
        double qty = SDL_min(vol * (double)(steps - 1) / steps,
                             1.0f - nbs[1 + dir][2].volume[type]);
        (*new)[x][y].volume[type] -= qty;
        flow_step(qty, type, steps - 1, dir, x + dir, y + 1, new);
    } else if (t) {
        double avg =
            (nbs[1][1].volume[type] + nbs[1 + dir][1].volume[type]) / 2.0f;
        double qty = SDL_min(vol * (double)(steps - 1) / steps,
                             1.0f - nbs[1 + dir][1].volume[type]);
        qty = SDL_min(qty, vol - avg);
        (*new)[x][y].volume[type] -= qty;
        flow_step(qty, type, steps - 1, dir, x + dir, y, new);
    }
}

void FLUIDS_step(void) {
    double dt = GAME_MANAGER_get_current_dt();

    fluids new[FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X]
              [FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y] = {0};
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
            new[x][y] = grid[x][y];
        }
    }

    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
            if (is_solid(x, y)) continue;
            fluid_cell nbs[3][4] = {0};
            get_neighbors(x, y, &nbs);
            for (Uint8 type = 0; type < FLUIDS_TOTAL; type++) {
                double vol = grid[x][y].volume[type];
                if (vol < EPSILON) continue;

                // Check and de-pressurizes
                if (vol > 1.0f + EPSILON) {
                    // Checks free neighbors without fluids/solids
                    int free_x[8] = {0};
                    int free_y[8] = {0};
                    size_t len = 0;
                    if (!nbs TLEFT.solid && nbs TLEFT.volume[type] < EPSILON) {
                        free_x[len] = -1;
                        free_y[len] = -1;
                        len++;
                    }
                    if (!nbs TOP.solid && nbs TOP.volume[type] < EPSILON) {
                        free_x[len] = 0;
                        free_y[len] = -1;
                        len++;
                    }
                    if (!nbs TRIGHT.solid &&
                        nbs TRIGHT.volume[type] < EPSILON) {
                        free_x[len] = 1;
                        free_y[len] = -1;
                        len++;
                    }
                    if (!nbs LEFT.solid && nbs LEFT.volume[type] < EPSILON) {
                        free_x[len] = -1;
                        free_y[len] = 0;
                        len++;
                    }
                    if (!nbs RIGHT.solid && nbs RIGHT.volume[type] < EPSILON) {
                        free_x[len] = 1;
                        free_y[len] = 0;
                        len++;
                    }
                    if (!nbs BLEFT.solid && nbs BLEFT.volume[type] < EPSILON) {
                        free_x[len] = -1;
                        free_y[len] = 1;
                        len++;
                    }
                    if (!nbs BOT.solid && nbs BOT.volume[type] < EPSILON) {
                        free_x[len] = 0;
                        free_y[len] = 1;
                        len++;
                    }
                    if (!nbs BRIGHT.solid &&
                        nbs BRIGHT.volume[type] < EPSILON) {
                        free_x[len] = 1;
                        free_y[len] = 1;
                        len++;
                    }
                    // Spits out excess fluid in equal quantity in all free
                    // neighbors
                    if (len > 0) {
                        double qty = (vol - 1.0f) / len;
                        for (int i = 0; i < len; i++) {
                            new[x + free_x[i]][y + free_y[i]].volume[type] +=
                                qty;
                        }
                        new[x][y].volume[type] = 1.0f;
                        continue;
                    }
                    // If there are no free neighbors, send excess fluid up
                    // if up is not solid
                    else if (!nbs TOP.solid) {
                        double qty = vol - 1.0f;
                        new[x][y - 1].volume[type] += qty;
                        new[x][y].volume[type] = 1.0f;
                        continue;
                    }
                }

                // Check and simulate falling
                if (nbs BOT.volume[type] < EPSILON && !nbs BOT.solid &&
                    (vol > 1.0f - EPSILON || nbs TOP.volume[type] < EPSILON)) {
                    double qty = SDL_min(vol, dt * data[type].gravity);
                    new[x][y].volume[type] -= qty;
                    new[x][y + 1].volume[type] += qty;
                    continue;
                } else if (nbs BOT.volume[type] > EPSILON && !nbs BOT.solid &&
                           nbs BOT.volume[type] < 1.0f - EPSILON) {
                    double qty = SDL_min(vol, dt * data[type].gravity);
                    qty = SDL_min(qty, 1.0f - nbs BOT.volume[type]);
                    new[x][y].volume[type] -= qty;
                    new[x][y + 1].volume[type] += qty;
                    continue;
                }

                // If the current cell is not full and being fed from above and
                // the its bottom is free, it can't feed it and it should start
                // flowing left and right. However, we don't want this to
                // happen, we want it to wait for the cell above to fill it so
                // it can start dropping down, so we have to stop the checks
                // here. We want the same thing to happen when the cell under is
                // full but the current one is not and currently being fed from
                // above, it should wait for the bottom to feed other cells and
                // then current should feed it.
                if ((!nbs BOT.solid && nbs BOT.volume[type] <= EPSILON &&
                     vol < 1.0f - EPSILON && nbs TOP.volume[type] >= EPSILON) ||
                    (!nbs BOT.solid && nbs BOT.volume[type] >= 1.0f - EPSILON &&
                     vol < 1.0f - EPSILON && nbs TOP.volume[type] >= EPSILON)) {
                    continue;
                }

                // Check and simulate flowing. First, we check which
                // diagonals are free, because it means we flow all the way
                // and we won't flow sideways. If one of the diagonals isn't
                // available, we check its top. If it's available, we flow
                // to it only until volumes equalize.
                bool left = false;
                bool bleft = false;
                bool right = false;
                bool bright = false;
                Uint8 flows = 0;
                if (!nbs BLEFT.solid &&
                    nbs BLEFT.volume[type] < 1.0f - EPSILON &&
                    !nbs LEFT.solid &&
                    (nbs LEFT.volume[type] < EPSILON ||
                     nbs LEFT.volume[type] > 1.0f - EPSILON)) {
                    bleft = true;
                    flows++;
                } else if (!nbs LEFT.solid && nbs LEFT.volume[type] < vol) {
                    left = true;
                    flows++;
                }
                if (!nbs BRIGHT.solid &&
                    nbs BRIGHT.volume[type] < 1.0f - EPSILON &&
                    !nbs RIGHT.solid &&
                    (nbs RIGHT.volume[type] < EPSILON ||
                     nbs RIGHT.volume[type] > 1.0f - EPSILON)) {
                    bright = true;
                    flows++;
                } else if (!nbs RIGHT.solid && nbs RIGHT.volume[type] < vol) {
                    right = true;
                    flows++;
                }
                if (flows == 0) continue;
                if (flows == 2) {
                    double qty = SDL_min(dt * data[type].flow, vol) / 2.0f;
                    if (bleft) {
                        double lqty =
                            SDL_min(qty, 1.0f - nbs BLEFT.volume[type]);
                        new[x][y].volume[type] -= lqty;
                        flow_step(lqty,
                                  type,
                                  data[type].flow_range,
                                  -1,
                                  x - 1,
                                  y + 1,
                                  &new);
                    } else {
                        double avg = (vol + nbs LEFT.volume[type]) / 2.0f;
                        double lqty = SDL_min(qty, vol - avg);
                        new[x][y].volume[type] -= lqty;
                        flow_step(lqty,
                                  type,
                                  data[type].flow_range,
                                  -1,
                                  x - 1,
                                  y,
                                  &new);
                    }
                    if (bright) {
                        double rqty =
                            SDL_min(qty, 1.0f - nbs BRIGHT.volume[type]);
                        new[x][y].volume[type] -= rqty;
                        flow_step(rqty,
                                  type,
                                  data[type].flow_range,
                                  1,
                                  x + 1,
                                  y + 1,
                                  &new);
                    } else {
                        double avg = (vol + nbs RIGHT.volume[type]) / 2.0f;
                        double rqty = SDL_min(qty, vol - avg);
                        new[x][y].volume[type] -= rqty;
                        flow_step(rqty,
                                  type,
                                  data[type].flow_range,
                                  1,
                                  x + 1,
                                  y,
                                  &new);
                    }
                }
                if (flows == 1) {
                    double qty = SDL_min(dt * data[type].flow, vol);
                    if (bleft) {
                        double lqty =
                            SDL_min(qty, 1.0f - nbs BLEFT.volume[type]);
                        new[x][y].volume[type] -= lqty;
                        flow_step(lqty,
                                  type,
                                  data[type].flow_range,
                                  -1,
                                  x - 1,
                                  y + 1,
                                  &new);
                    } else if (left) {
                        double avg = (vol + nbs LEFT.volume[type]) / 2.0f;
                        double lqty = SDL_min(qty, vol - avg);
                        new[x][y].volume[type] -= lqty;
                        flow_step(lqty,
                                  type,
                                  data[type].flow_range,
                                  -1,
                                  x - 1,
                                  y,
                                  &new);
                    } else if (bright) {
                        double rqty =
                            SDL_min(qty, 1.0f - nbs BRIGHT.volume[type]);
                        new[x][y].volume[type] -= rqty;
                        flow_step(rqty,
                                  type,
                                  data[type].flow_range,
                                  1,
                                  x + 1,
                                  y + 1,
                                  &new);
                    } else if (right) {
                        double avg = (vol + nbs RIGHT.volume[type]) / 2.0f;
                        double rqty = SDL_min(qty, vol - avg);
                        new[x][y].volume[type] -= rqty;
                        flow_step(rqty,
                                  type,
                                  data[type].flow_range,
                                  1,
                                  x + 1,
                                  y,
                                  &new);
                    }
                }
            }
        }
    }

    for (long x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (long y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
            grid[x][y] = new[x][y];
        }
    }

    while (mods.len > 0) {
        mod* m = (mod*)mods.at[0];
        for (Uint32 x = m->x; x < m->x + m->w; x++) {
            for (Uint32 y = m->y; y < m->y + m->h; y++) {
                if (m->clear) {
                    for (fluid_type type = 0; type < FLUIDS_TOTAL; type++) {
                        grid[x][y].volume[type] = 0.0f;
                    }
                } else {
                    grid[x][y].volume[m->type] += m->volume;
                }
            }
        }
        SDL_free(m);
        remove_at_from_dynarr(0, &mods);
    }
}



void FLUIDS_set_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type, double volume) {
    mod* m = SDL_malloc(sizeof(mod));
    *m = (mod){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .type = type,
        .volume = volume,
        .clear = false,
    };
    push_to_dynarr((void*)m, &mods);
}

void FLUIDS_clear_at(Uint32 x, Uint32 y, Uint32 w, Uint32 h) {
    mod* m = SDL_malloc(sizeof(mod));
    *m = (mod){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .clear = true,
    };
    push_to_dynarr((void*)m, &mods);
}

void FLUIDS_get_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_cell (*cells)[w][h]) {
    for (long i = x; i < x + w; i++) {
        for (long j = y; j < y + h; j++) {
            if (i >= TILEMAP_SIZE_X * FLUIDS_SPLIT_X ||
                j >= TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y) {
                (*cells)[i - x][j - y] = (fluid_cell){.solid = true};
                continue;
            }
            fluid_cell cell = (fluid_cell){0};
            cell.solid = is_solid(i, j);
            for (Uint8 type = 0; type < FLUIDS_TOTAL; type++)
                cell.volume[type] = grid[i][j].volume[type];
            (*cells)[i - x][j - y] = cell;
        }
    }
}



// Draw function. We could draw cells with math based on SPRITESHEET_CELL_X/Y
// and FLUIDS_SPLIT_X/Y but instead, we will draw them iterating on the bits on
// a Uint16, this way we can draw them with 0's and 1's, each bit representing a
// pixel on a 4x4 grid (which is the current size of each cell) which also means
// if we ever change sizes, we need to redraw and resize the variable.
void FLUIDS_draw(SDL_Renderer* renderer) {
    int cell_w = SPRITESHEET_CELL_X / FLUIDS_SPLIT_X;
    int cell_h = SPRITESHEET_CELL_Y / FLUIDS_SPLIT_Y;
    for (fluid_type type = 0; type < FLUIDS_TOTAL; type++) {
#ifndef TEST
        SDL_SetRenderDrawColor(renderer,
                               data[type].col_r,
                               data[type].col_g,
                               data[type].col_b,
                               data[type].col_a);
#endif
        for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
            for (Uint32 y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
                double vol = grid[x][y].volume[type];
                if (vol < EPSILON) continue;

                fluid_cell nbs[3][4] = {0};
                get_neighbors(x, y, &nbs);
                bool t = nbs TOP.solid ? true : nbs TOP.volume[type] > EPSILON;
                bool b = nbs BOT.solid ? true : nbs BOT.volume[type] > EPSILON;
                bool l =
                    nbs LEFT.solid ? true : nbs LEFT.volume[type] > EPSILON;
                bool r =
                    nbs RIGHT.solid ? true : nbs RIGHT.volume[type] > EPSILON;

                double vt = nbs TOP.solid ? 1.0f : nbs TOP.volume[type];
                double vb = nbs BOT.solid ? 1.0f : nbs BOT.volume[type];
                double vl = nbs LEFT.solid ? 1.0f : nbs LEFT.volume[type];
                double vr = nbs RIGHT.solid ? 1.0f : nbs RIGHT.volume[type];
                Uint16 draw = 0;

                // Cells with no neighbors first
                if (!t && !b && !l && !r) {
                    if (vol <= 0.3f)
                        draw = 0b0000010000000000;
                    else if (vol <= 0.6f)
                        draw = 0b0000011001100000;
                    else
                        draw = 0b0110111111110110;
                }

                // Cells with one neighbor to the top
                if (t && !b && !l && !r) {
                    if (vol <= 0.3f)
                        draw = 0b0110000000000000;
                    else if (vol <= 0.6f)
                        draw = 0b1111011000000000;
                    else
                        draw = 0b1111111101100000;
                }

                // Cells with one neighbor to the bottom
                if (!t && b && !l && !r) {
                    if (vol <= 0.3f)
                        draw = 0b0000000000000110;
                    else if (vol <= 0.6f)
                        draw = 0b0000000001101111;
                    else
                        draw = 0b0000011011111111;
                }

                // Cells with one neighbor to the left
                if (!t && !b && l && !r) {
                    if (vol <= 0.3f)
                        draw = 0b0000100010000000;
                    else if (vol <= 0.6f)
                        draw = 0b1000110011001000;
                    else
                        draw = 0b1100111011101100;
                }

                // Cells with one neighbor to the right
                if (!t && !b && !l && r) {
                    if (vol <= 0.3f)
                        draw = 0b0000000100010000;
                    else if (vol <= 0.6f)
                        draw = 0b0001001100110001;
                    else
                        draw = 0b0011011101110011;
                }

                // Cells with two neighbors top and bottom
                if (t && b && !l && !r) {
                    if (vol <= 0.25f)
                        draw = 0b0100010001000100;
                    else if (vol <= 0.5f)
                        draw = 0b0110011001100110;
                    else if (vol <= 0.75f)
                        draw = 0b1110111011101110;
                    else
                        draw = 0b1111111111111111;
                }

                // Cells with two neighbors left and right
                if (!t && !b && l && r) {
                    if (vol <= 0.3f)
                        draw = 0b0000100110010000;
                    else if (vol <= 0.6f)
                        draw = 0b1001111111111001;
                    else
                        draw = 0b1111111111111111;
                }

                // Cells with two neighbors bottom and left
                if (!t && b && l && !r) {
                    if (vl <= 0.5f || vb <= 0.5f)
                        draw = 0b0000000000001000;
                    else
                        draw = 0b0000000011001100;
                }

                // Cells with two neighbors bottom and right
                if (!t && b && !l && r) {
                    if (vr <= 0.5f || vb <= 0.5f)
                        draw = 0b0000000000000001;
                    else
                        draw = 0b0000000000110011;
                }

                // Cells with two neighbors top and left
                if (t && !b && l && !r) {
                    if (vl <= 0.5f || vt <= 0.5f)
                        draw = 0b1000000000000000;
                    else
                        draw = 0b1100110000000000;
                }

                // Cells with two neighbors top and right
                if (t && !b && !l && r) {
                    if (vr <= 0.5f || vt <= 0.5)
                        draw = 0b0001000000000000;
                    else
                        draw = 0b0011001100000000;
                }

                // Cells with three neighbors, all but top
                if (!t && b && l && r) {
                    if (vol <= 0.25f)
                        draw = 0b0000000000001111;
                    else if (vol <= 0.5f)
                        draw = 0b0000000011111111;
                    else if (vol <= 0.75)
                        draw = 0b0000111111111111;
                    else
                        draw = 0b1111111111111111;
                }

                // Cells with three neighbors, all but bottom
                if (t && !b && l && r) {
                    if (vol <= 0.25f)
                        draw = 0b1111000000000000;
                    else if (vol <= 0.5f)
                        draw = 0b1111111100000000;
                    else if (vol <= 0.75f)
                        draw = 0b1111111111110000;
                    else
                        draw = 0b1111111111111111;
                }

                // Cells with three neighbors, all but left
                if (t && b && !l && r) {
                    if (vol <= 0.25f)
                        draw = 0b0001000100010001;
                    else if (vol <= 0.5f)
                        draw = 0b0011001100110011;
                    else if (vol <= 0.75f)
                        draw = 0b0111011101110111;
                    else
                        draw = 0b1111111111111111;
                }

                // Cells with three neighbors, all but right
                if (t && b && l && !r) {
                    if (vol <= 0.25f)
                        draw = 0b1000000000000000;
                    else if (vol <= 0.5f)
                        draw = 0b1100110011001100;
                    else if (vol <= 0.75f)
                        draw = 0b1110111011101110;
                    else
                        draw = 0b1111111111111111;
                }

                // Cells with neighbors in all directions
                if (t && b && l && r) {
                    draw = 0b1111111111111111;
                }

                // Loop though the bits of `draw` and draw each pixel
                int bit = 15;
                while (draw > 0) {
                    if (draw & 1)
#ifndef TEST
                        SDL_RenderPoint(renderer,
                                        x * cell_w + (bit & 3),
                                        y * cell_h + (bit >> 2));

#else
                        stubbed_SDL_RenderPoint(x * cell_w + (bit & 3),
                                                y * cell_h + (bit >> 2));
#endif
                    draw >>= 1;
                    bit--;
                }
            }
        }
    }
}
