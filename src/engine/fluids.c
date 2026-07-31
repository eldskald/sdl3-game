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
    double density;
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
    double volume;
    bool solid;
} neighbor;

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
static fluid_type ordered[FLUIDS_TOTAL] = {0};



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
    for (size_t i = 1; i <= FLUIDS_TOTAL; i++) {
        dynarr* row = (dynarr*)parsed.at[i];
        char* name = (char*)row->at[0];
        data[(size_t)get_from_hashmap(name, &names)] = (fluid_data){
            .density = SDL_strtod(
                (char*)row->at[(size_t)get_from_hashmap("density", &props)],
                NULL),
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

static void order_fluids_by_density(void) {
    // Initiate ordered_fluids
    for (int i = 0; i < FLUIDS_TOTAL; i++) {
        ordered[i] = i;
    }

    // Bubble sort order it
    bool found = true;
    while (found) {
        found = false;
        for (int i = 1; i < FLUIDS_TOTAL; i++) {
            if (data[ordered[i]].density > data[ordered[i - 1]].density) {
                int mem = ordered[i];
                ordered[i] = ordered[i - 1];
                ordered[i - 1] = mem;
                found = true;
            }
        }
    }
}

void FLUIDS_start(void) {
    load_fluid_data();
    order_fluids_by_density();
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
#define BBRIGHT [2][3]
#define BBLEFT [0][3]
#define BRIGHT [2][2]

#define SELF [1][1]

static void get_neighbor_fluids(Uint32 x, Uint32 y, fluid_cell (*nbs)[3][3]) {
    for (long i = (long)x - 1; i <= (long)x + 1; i++) {
        for (long j = (long)y - 1; j <= (long)y + 1; j++) {
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

static void
get_neighbors(Uint32 x, Uint32 y, Uint8 up_to, neighbor (*nbs)[3][4]) {
    for (long i = (long)x - 1; i <= (long)x + 1; i++) {
        for (long j = (long)y - 1; j <= (long)y + 2; j++) {
            if (i < 0 || j < 0 || i >= TILEMAP_SIZE_X * FLUIDS_SPLIT_X ||
                j >= TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y) {
                (*nbs)[i - x + 1][j - y + 1] = (neighbor){.solid = true};
                continue;
            }
            neighbor cell = (neighbor){0};
            cell.solid = is_solid(i, j);
            for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
                fluid_type type = ordered[t];
                cell.volume += grid[i][j].volume[type];
                if (type == up_to) break;
            }
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
    neighbor nbs[3][4] = {0};
    get_neighbors(x, y, type, &nbs);
    double total = nbs SELF.volume;
    neighbor bbot = nbs[1 + dir][3];
    neighbor bot = nbs[1 + dir][2];
    neighbor side = nbs[1 + dir][1];
    neighbor top = nbs[1 + dir][0];

    bool b = false;
    bool t = false;
    if ((!bot.solid && bot.volume < 1.0f - EPSILON) &&
        (bbot.solid || bbot.volume > 1.0f - EPSILON) &&
        (!side.solid && side.volume < EPSILON))
        b = true;
    if ((!side.solid && side.volume < total) &&
        (bot.solid || bot.volume > 1.0f - EPSILON) &&
        (!top.solid && top.volume < EPSILON))
        t = true;

    if (steps <= 1) return;
    if (b) {
        double qty = SDL_min(vol * (double)(steps - 1) / steps,
                             1.0f - nbs[1 + dir][2].volume);
        (*new)[x][y].volume[type] -= qty;
        flow_step(qty, type, steps - 1, dir, x + dir, y + 1, new);
    } else if (t) {
        double avg = (total + nbs[1 + dir][1].volume) / 2.0f;
        double qty = SDL_min(vol * (double)(steps - 1) / steps,
                             1.0f - nbs[1 + dir][1].volume);
        qty = SDL_min(qty, total - avg);
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

            // We iterate through liquid types in decrescent order of density,
            // counting the volume of a cell as the volume of all liquid types
            // iterated through so far. This means a given fluid will see all
            // fluids denser than itself when trying to fall/flow into a new
            // cell, ignoring all lighter fluids which should be handled by
            // de-pressurization when denser fluids fall into its cell.
            for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
                fluid_type type = ordered[t];
                double vol = grid[x][y].volume[type];
                if (vol < EPSILON) continue;

                neighbor nbs[3][4] = {0};
                get_neighbors(x, y, type, &nbs);
                if (nbs SELF.solid) continue;

                double total = nbs SELF.volume;

                // Check and de-pressurizes
                if (total > 1.0f + EPSILON) {
                    // Checks free neighbors without fluids/solids
                    int free_x[8] = {0};
                    int free_y[8] = {0};
                    size_t len = 0;
                    if (!nbs TLEFT.solid && nbs TLEFT.volume < EPSILON) {
                        free_x[len] = -1;
                        free_y[len] = -1;
                        len++;
                    }
                    if (!nbs TOP.solid && nbs TOP.volume < EPSILON) {
                        free_x[len] = 0;
                        free_y[len] = -1;
                        len++;
                    }
                    if (!nbs TRIGHT.solid && nbs TRIGHT.volume < EPSILON) {
                        free_x[len] = 1;
                        free_y[len] = -1;
                        len++;
                    }
                    if (!nbs LEFT.solid && nbs LEFT.volume < EPSILON) {
                        free_x[len] = -1;
                        free_y[len] = 0;
                        len++;
                    }
                    if (!nbs RIGHT.solid && nbs RIGHT.volume < EPSILON) {
                        free_x[len] = 1;
                        free_y[len] = 0;
                        len++;
                    }
                    if (!nbs BLEFT.solid && nbs BLEFT.volume < EPSILON) {
                        free_x[len] = -1;
                        free_y[len] = 1;
                        len++;
                    }
                    if (!nbs BOT.solid && nbs BOT.volume < EPSILON) {
                        free_x[len] = 0;
                        free_y[len] = 1;
                        len++;
                    }
                    if (!nbs BRIGHT.solid && nbs BRIGHT.volume < EPSILON) {
                        free_x[len] = 1;
                        free_y[len] = 1;
                        len++;
                    }
                    // Spits out excess fluid in equal quantity in all free
                    // neighbors
                    if (len > 0) {
                        double excess = SDL_min(total - 1.0f, vol);
                        double qty = excess / len;
                        for (int i = 0; i < len; i++) {
                            new[x + free_x[i]][y + free_y[i]].volume[type] +=
                                qty;
                        }
                        new[x][y].volume[type] -= excess;
                        continue;
                    }
                    // If there are no free neighbors, send excess fluid up
                    // if up is not solid
                    else if (!nbs TOP.solid) {
                        double qty = SDL_min(total - 1.0f, vol);
                        new[x][y - 1].volume[type] += qty;
                        new[x][y].volume[type] -= qty;
                        continue;
                    }
                }

                // Check and simulate falling
                if (nbs BOT.volume < EPSILON && !nbs BOT.solid &&
                    (total > 1.0f - EPSILON || nbs TOP.volume < EPSILON)) {
                    double qty = SDL_min(vol, dt * data[type].gravity);
                    new[x][y].volume[type] -= qty;
                    new[x][y + 1].volume[type] += qty;
                    continue;
                } else if (nbs BOT.volume > EPSILON && !nbs BOT.solid &&
                           nbs BOT.volume < 1.0f - EPSILON) {
                    double qty = SDL_min(vol, dt * data[type].gravity);
                    qty = SDL_min(qty, 1.0f - nbs BOT.volume);
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
                if ((!nbs BOT.solid && nbs BOT.volume <= EPSILON &&
                     total < 1.0f - EPSILON && nbs TOP.volume >= EPSILON) ||
                    (!nbs BOT.solid && nbs BOT.volume >= 1.0f - EPSILON &&
                     total < 1.0f - EPSILON && nbs TOP.volume >= EPSILON)) {
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
                if ((!nbs BRIGHT.solid && nbs BRIGHT.volume < 1.0f - EPSILON) &&
                    (nbs BBRIGHT.solid ||
                     nbs BBRIGHT.volume > 1.0f - EPSILON) &&
                    (!nbs RIGHT.solid && nbs RIGHT.volume < EPSILON)) {
                    bright = true;
                    flows++;
                } else if (!nbs RIGHT.solid && nbs RIGHT.volume < total) {
                    right = true;
                    flows++;
                }
                if ((!nbs BLEFT.solid && nbs BLEFT.volume < 1.0f - EPSILON) &&
                    (nbs BBLEFT.solid || nbs BBLEFT.volume > 1.0f - EPSILON) &&
                    (!nbs LEFT.solid && nbs LEFT.volume < EPSILON)) {
                    bleft = true;
                    flows++;
                } else if (!nbs LEFT.solid && nbs LEFT.volume < total) {
                    left = true;
                    flows++;
                }
                if (flows == 0) continue;
                if (flows == 2) {
                    double qty = SDL_min(dt * data[type].flow, vol) / 2.0f;
                    if (bleft) {
                        double lqty = SDL_min(qty, 1.0f - nbs BLEFT.volume);
                        new[x][y].volume[type] -= lqty;
                        flow_step(lqty,
                                  type,
                                  data[type].flow_range,
                                  -1,
                                  x - 1,
                                  y + 1,
                                  &new);
                    } else {
                        double avg = (total + nbs LEFT.volume) / 2.0f;
                        double lqty = SDL_min(qty, total - avg);
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
                        double rqty = SDL_min(qty, 1.0f - nbs BRIGHT.volume);
                        new[x][y].volume[type] -= rqty;
                        flow_step(rqty,
                                  type,
                                  data[type].flow_range,
                                  1,
                                  x + 1,
                                  y + 1,
                                  &new);
                    } else {
                        double avg = (total + nbs RIGHT.volume) / 2.0f;
                        double rqty = SDL_min(qty, total - avg);
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
                        double lqty = SDL_min(qty, 1.0f - nbs BLEFT.volume);
                        new[x][y].volume[type] -= lqty;
                        flow_step(lqty,
                                  type,
                                  data[type].flow_range,
                                  -1,
                                  x - 1,
                                  y + 1,
                                  &new);
                    } else if (left) {
                        double avg = (total + nbs LEFT.volume) / 2.0f;
                        double lqty = SDL_min(qty, total - avg);
                        new[x][y].volume[type] -= lqty;
                        flow_step(lqty,
                                  type,
                                  data[type].flow_range,
                                  -1,
                                  x - 1,
                                  y,
                                  &new);
                    } else if (bright) {
                        double rqty = SDL_min(qty, 1.0f - nbs BRIGHT.volume);
                        new[x][y].volume[type] -= rqty;
                        flow_step(rqty,
                                  type,
                                  data[type].flow_range,
                                  1,
                                  x + 1,
                                  y + 1,
                                  &new);
                    } else if (right) {
                        double avg = (total + nbs RIGHT.volume) / 2.0f;
                        double rqty = SDL_min(qty, total - avg);
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
                if (m->clear)
                    grid[x][y].volume[m->type] = 0.0f;
                else
                    grid[x][y].volume[m->type] += m->volume;
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

void FLUIDS_clear_at(Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type) {
    mod* m = SDL_malloc(sizeof(mod));
    *m = (mod){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .clear = true,
        .type = type,
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



void FLUIDS_draw(SDL_Renderer* renderer) {
    for (Uint32 x = 0; x < FLUIDS_SIMULATE_X * FLUIDS_SPLIT_X; x++) {
        for (Uint32 y = 0; y < FLUIDS_SIMULATE_Y * FLUIDS_SPLIT_Y; y++) {
            double total = 0.0f;
            for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
                fluid_type type = ordered[t];
#ifndef TEST
                SDL_SetRenderDrawColor(renderer,
                                       data[type].col_r,
                                       data[type].col_g,
                                       data[type].col_b,
                                       data[type].col_a);
#endif
                double vol = grid[x][y].volume[type];
                if (vol < EPSILON) continue;
                if (is_solid(x, y)) continue;

                total += vol;
                fluid_cell nbs[3][3] = {0};
                get_neighbor_fluids(x, y, &nbs);
                double tvol = nbs[1][0].volume[type];
                double bvol = nbs[1][2].solid ? 1.0f : nbs[1][2].volume[type];
                double below = total - vol;

                SDL_FRect rect = (SDL_FRect){0};
                if (below < EPSILON) {
                    if (tvol > EPSILON || (bvol < EPSILON && tvol < EPSILON)) {
                        rect.x = x * FLUIDS_CELL_W;
                        rect.y = y * FLUIDS_CELL_H;
                        rect.w = FLUIDS_CELL_W;
                        rect.h = FLUIDS_CELL_H;
                    } else {
                        double h = 1.0f;
                        if (vol > 0.25f) h += 1.0f;
                        if (vol > 0.5f) h += 1.0f;
                        if (vol > 0.75f) h += 1.0f;
                        rect.x = x * FLUIDS_CELL_W;
                        rect.y = y * FLUIDS_CELL_H + 4.0f - h;
                        rect.w = FLUIDS_CELL_W;
                        rect.h = h;
                    }
                } else {
                    double h = 1.0f;
                    if (vol > 0.25f) h += 1.0f;
                    if (vol > 0.5f) h += 1.0f;
                    if (vol > 0.75f) h += 1.0f;
                    double off_y = 1.0f;
                    if (below > 0.25f) off_y += 1.0f;
                    if (below > 0.5f) off_y += 1.0f;
                    if (below > 0.75f) off_y += 1.0f;
                    rect.x = x * FLUIDS_CELL_W;
                    rect.y = y * FLUIDS_CELL_H + 4.0f - h - off_y;
                    rect.w = FLUIDS_CELL_W;
                    rect.h = h;
                }
#ifndef TEST
                SDL_RenderFillRect(renderer, &rect);
#else
                stubbed_SDL_RenderFillRect(&rect);
#endif
            }
        }
    }
}



void FLUIDS_splash_at(Uint32 x, Uint32 y) {
    if (is_solid(x, y)) return;
    for (int type = 0; type < FLUIDS_TOTAL; type++) {
        double total = 0.0f;
        for (int i = -2; i < 2; i++) {
            if (!is_solid(x + i, y + 2) &&
                grid[x + i][y + 2].volume[type] > 0.1f) {
                FLUIDS_clear_at(x + i, y + 2, 1, 1, type);
                FLUIDS_set_at(x + i, y + 2, 1, 1, type, 0.1f);
                total += grid[x + i][y + 2].volume[type] - 0.1f;
            }
        }
        FLUIDS_set_at(x, y, 1, 1, type, total);
    }
}
