#include "engine/physics.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/tilemap.h"
#include <SDL3/SDL.h>



typedef struct {
    struct {
        Uint16 x;
        Uint16 y;
    } tl;
    struct {
        Uint16 x;
        Uint16 y;
    } br;
} nbcoords;



// Fluid cells occupied by a rect, `(x,y)` being its top left cell and `(w,h)`
// how many cells it spans. Cells above or to the left of the grid are left out,
// so `w` or `h` are `0` or less when it's entirely out.
typedef struct {
    long x;
    long y;
    long w;
    long h;
} fluid_rect;

// Bodies are wrapped with the state the physics engine keeps on them, so it
// isn't exposed on `body`. `sleeping` turns off physics for it and
// `splash_cooldown` is how long until it can splash fluids again.
typedef struct {
    body body;
    bool sleeping;
    double splash_cooldown;
    double drop_timer;
} body_data;



static body_data* bodies = NULL;
static size_t bodies_cap = 0;
static dynarr nbsearch[TILEMAP_SIZE_X][TILEMAP_SIZE_Y] = {0};

static void double_dynarr(void) {
    body_data* new_bodies = SDL_calloc(bodies_cap << 1, sizeof(body_data));
    SDL_memcpy(new_bodies, bodies, bodies_cap * sizeof(body_data));
    SDL_free(bodies);
    bodies_cap = bodies_cap << 1;
    bodies = new_bodies;
}



static Uint16 pos_to_tile_x(double x) {
    return x / SPRITESHEET_CELL_X;
}

static Uint16 pos_to_tile_y(double y) {
    return y / SPRITESHEET_CELL_Y;
}

// Clamps a tile index to [0, size - 1] before converting it, so rects outside
// the tilemap end up on the edge cells.
static Uint16 clamp_tile(double tile, Uint16 size) {
    return SDL_clamp(tile, 0.0, (double)(size - 1));
}

static nbcoords get_rect_nbcoords(double x, double y, double w, double h) {
    double l = SDL_round(x);
    double t = SDL_round(y);
    double r = l + SDL_round(w);
    double b = t + SDL_round(h);
    double tl_x = SDL_floor(l / SPRITESHEET_CELL_X);
    double tl_y = SDL_floor(t / SPRITESHEET_CELL_Y);
    double br_x = SDL_floor(r / SPRITESHEET_CELL_X) -
                  ((int)r % SPRITESHEET_CELL_X == 0 ? 1 : 0);
    double br_y = SDL_floor(b / SPRITESHEET_CELL_Y) -
                  ((int)b % SPRITESHEET_CELL_Y == 0 ? 1 : 0);
    return (nbcoords){
        .tl = {.x = clamp_tile(tl_x, TILEMAP_SIZE_X),
               .y = clamp_tile(tl_y, TILEMAP_SIZE_Y)},
        .br = {.x = clamp_tile(br_x, TILEMAP_SIZE_X),
               .y = clamp_tile(br_y, TILEMAP_SIZE_Y)},
    };
}

static nbcoords get_body_nbcoords(body b) {
    return get_rect_nbcoords(b.x, b.y, b.w, b.h);
}

static void add_to_nbsearch(nbcoords c, size_t idx) {
    for (Uint16 x = c.tl.x; x <= c.br.x; x++) {
        for (Uint16 y = c.tl.y; y <= c.br.y; y++) {
            push_to_dynarr((void*)idx, &nbsearch[x][y]);
        }
    }
}

static void remove_from_nbsearch(nbcoords c, size_t idx) {
    for (Uint16 x = c.tl.x; x <= c.br.x; x++) {
        for (Uint16 y = c.tl.y; y <= c.br.y; y++) {
            remove_val_from_dynarr((void*)idx, &nbsearch[x][y]);
        }
    }
}

static void update_nbsearch(nbcoords old, nbcoords new, size_t idx) {
    if (old.tl.x == new.tl.x && old.tl.y == new.tl.y && old.br.x == new.br.x &&
        old.br.y == new.br.y)
        return;
    remove_from_nbsearch(old, idx);
    add_to_nbsearch(new, idx);
}



static bool are_intersecting(double ax,
                             double ay,
                             double aw,
                             double ah,
                             double bx,
                             double by,
                             double bw,
                             double bh) {
    SDL_FRect inter = (SDL_FRect){0};
    SDL_GetRectIntersectionFloat(
        &(SDL_FRect){
            .x = SDL_roundf(ax),
            .y = SDL_roundf(ay),
            .w = SDL_roundf(aw),
            .h = SDL_roundf(ah),
        },
        &(SDL_FRect){
            .x = SDL_roundf(bx),
            .y = SDL_roundf(by),
            .w = SDL_roundf(bw),
            .h = SDL_roundf(bh),
        },
        &inter);
    return inter.w > EPSILON && inter.h > EPSILON;
}



static void move_body_horizontally(body_data* b, double d) {

    // tiles the corners of the body is in, snapped to pixels
    double tl_x = SDL_round(b->body.x);
    double tl_y = SDL_round(b->body.y);
    double tr_x = SDL_round(b->body.x + b->body.w);
    double bl_y = SDL_round(b->body.y + b->body.h);

    // when moving right
    if (d >= 0.0f) {
        bool found = false;
        Uint16 from_x = pos_to_tile_x(tr_x);
        Uint16 to_x = pos_to_tile_x(tr_x + d);
        Uint16 from_y = pos_to_tile_y(tl_y);
        Uint16 to_y = pos_to_tile_y(bl_y);
        for (Uint16 i = from_x; i <= to_x; i++) {
            for (Uint16 j = from_y; j <= to_y; j++) {
                tile data = (tile){0};
                int r = TILEMAP_get_at(i, j, &data);
                if (r == 1 || (data.type && !data.platform)) {
                    if (SDL_roundf(b->body.y + b->body.h) <=
                            SPRITESHEET_CELL_Y * j ||
                        SDL_roundf(b->body.y) >= SPRITESHEET_CELL_Y * (j + 1))
                        continue;
                    found = true;
                    b->body.x = SPRITESHEET_CELL_X * i - b->body.w;
                    b->body.vel_x = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->body.x += d;
    }
    // when moving left
    else {
        bool found = false;
        Uint16 from_x = pos_to_tile_x(tl_x);
        Uint16 to_x = pos_to_tile_x(tl_x + d);
        Uint16 from_y = pos_to_tile_y(tl_y);
        Uint16 to_y = pos_to_tile_y(bl_y);
        for (Uint16 i = from_x; i >= to_x; i--) {
            for (Uint16 j = from_y; j <= to_y; j++) {
                tile data = (tile){0};
                int r = TILEMAP_get_at(i, j, &data);
                if (r == 1 || (data.type && !data.platform)) {
                    if (SDL_roundf(b->body.y + b->body.h) <=
                            SPRITESHEET_CELL_Y * j ||
                        SDL_roundf(b->body.y) >= SPRITESHEET_CELL_Y * (j + 1))
                        continue;
                    found = true;
                    b->body.x = SPRITESHEET_CELL_X * (i + 1);
                    b->body.vel_x = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->body.x += d;
    }
}

static void move_body_vertically(body_data* b, double d) {

    // tiles the corners of the body is in
    double tl_x = SDL_round(b->body.x);
    double tl_y = SDL_round(b->body.y);
    double tr_x = SDL_round(b->body.x + b->body.w);
    double bl_y = SDL_round(b->body.y + b->body.h);

    // when moving down, only here we care if tile is platform or if body is
    // dropping down
    if (d >= 0.0f) {
        bool found = false;
        Uint16 from_y = pos_to_tile_y(bl_y);
        Uint16 to_y = pos_to_tile_y(bl_y + d);
        Uint16 from_x = pos_to_tile_x(tl_x);
        Uint16 to_x = pos_to_tile_x(tr_x);
        for (Uint16 i = from_x; i <= to_x; i++) {
            for (Uint16 j = from_y; j <= to_y; j++) {
                tile data = (tile){0};
                int r = TILEMAP_get_at(i, j, &data);
                if (r == 1 || data.type) {
                    if (data.platform &&
                        (b->drop_timer > 0.0f || bl_y > SPRITESHEET_CELL_Y * j))
                        continue;
                    if (SDL_roundf(b->body.x + b->body.w) <=
                            SPRITESHEET_CELL_X * i ||
                        SDL_roundf(b->body.x) >= SPRITESHEET_CELL_X * (i + 1))
                        continue;
                    found = true;
                    b->body.y = SPRITESHEET_CELL_Y * j - b->body.h;
                    b->body.vel_y = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->body.y += d;
    }
    // when moving up
    else {
        bool found = false;
        Uint16 from_y = pos_to_tile_y(tl_y);
        Uint16 to_y = pos_to_tile_y(tl_y + d);
        Uint16 from_x = pos_to_tile_x(tl_x);
        Uint16 to_x = pos_to_tile_x(tr_x);
        for (Uint16 i = from_x; i <= to_x; i++) {
            for (Uint16 j = from_y; j >= to_y; j--) {
                tile data = (tile){0};
                int r = TILEMAP_get_at(i, j, &data);
                if (r == 1 || (data.type && !data.platform)) {
                    if (SDL_roundf(b->body.x + b->body.w) <=
                            SPRITESHEET_CELL_X * i ||
                        SDL_roundf(b->body.x) >= SPRITESHEET_CELL_X * (i + 1))
                        continue;
                    found = true;
                    b->body.y = SPRITESHEET_CELL_Y * (j + 1);
                    b->body.vel_y = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->body.y += d;
    }
}



// Queues a ripple at the center of `b` when it's in a fluid cell, as strong as
// it was moving at `(vel_x,vel_y)`, up to `FLUIDS_RIPPLE_MAX_SPEED`.
static void ripple_fluids(body b, double vel_x, double vel_y) {
    double x = SDL_round(b.x + b.w / 2.0f);
    double y = SDL_round(b.y + b.h / 2.0f);
    if (x < 0.0f || y < 0.0f) return;
    fluid_cell cell[1][1];
    FLUIDS_get_at(x / FLUIDS_CELL_W, y / FLUIDS_CELL_H, 1, 1, &cell);
    if (cell[0][0].solid) return;
    double vol = 0.0f;
    for (Uint8 t = 0; t < FLUIDS_TOTAL; t++)
        vol += cell[0][0].volume[t];
    if (vol < EPSILON) return;
    double speed = SDL_min(SDL_sqrt(vel_x * vel_x + vel_y * vel_y),
                           FLUIDS_RIPPLE_MAX_SPEED);
    FLUIDS_ripple_at(
        x, y, FLUIDS_RIPPLE_STRENGTH * speed / FLUIDS_RIPPLE_MAX_SPEED);
}



// Fluid cells occupied by a rect snapped to pixels, w and h must be positive.
// Edges exactly on a cell border don't occupy the next cell.
static fluid_rect get_fluid_rect(double x, double y, double w, double h) {
    double l = SDL_round(x);
    double t = SDL_round(y);
    double r = l + SDL_round(w);
    double b = t + SDL_round(h);
    long from_x = SDL_max(SDL_floor(l / FLUIDS_CELL_W), 0.0);
    long from_y = SDL_max(SDL_floor(t / FLUIDS_CELL_H), 0.0);
    long to_x = SDL_floor((r - 1.0f) / FLUIDS_CELL_W);
    long to_y = SDL_floor((b - 1.0f) / FLUIDS_CELL_H);
    return (fluid_rect){.x = from_x,
                        .y = from_y,
                        .w = to_x - from_x + 1,
                        .h = to_y - from_y + 1};
}

static bool are_same_fluid_rects(fluid_rect a, fluid_rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// Whether any cell of `r` has fluid.
static bool has_fluid(fluid_rect r) {
    if (r.w <= 0 || r.h <= 0) return false;
    fluid_cell cells[r.w][r.h];
    FLUIDS_get_at(r.x, r.y, r.w, r.h, &cells);
    for (long i = 0; i < r.w; i++) {
        for (long j = 0; j < r.h; j++) {
            if (cells[i][j].solid) continue;
            for (Uint8 t = 0; t < FLUIDS_TOTAL; t++)
                if (cells[i][j].volume[t] > EPSILON) return true;
        }
    }
    return false;
}

// Splashes the fluids `b` just entered, from `(x0,y0)` to where it is now. When
// its rect wasn't touching any fluid cell before but now is, every column it
// spans is splashed at its bottom row, as hard as it was falling at `vel_y`.
// Assumes it entered through a horizontal surface, from above.
static void splash_fluids(body_data* b, double x0, double y0, double vel_y) {
    if (b->splash_cooldown > 0.0f) return;
    fluid_rect from = get_fluid_rect(x0, y0, b->body.w, b->body.h);
    fluid_rect to = get_fluid_rect(b->body.x, b->body.y, b->body.w, b->body.h);
    if (are_same_fluid_rects(from, to)) return;
    if (has_fluid(from) || !has_fluid(to)) return;
    FLUIDS_splash_at(to.x, to.y + to.h - 1, to.w, SDL_fabs(vel_y));
    b->splash_cooldown = PHYSICS_SPLASH_COOLDOWN;
}



int PHYSICS_new_body(body data, size_t* body_id) {
    if (data.w == 0 || data.h == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to create body with invalid data: { w = %d, h = %d }",
            data.w,
            data.h);
        return 2;
    }
    for (size_t i = 0; i < bodies_cap; i++) {
        if (bodies[i].body.w == 0) {
            bodies[i] = (body_data){.body = data};
            add_to_nbsearch(get_body_nbcoords(data), i);
            *body_id = i;
            return 0;
        }
    }
    double_dynarr();
    size_t index = bodies_cap >> 1;
    bodies[index] = (body_data){.body = data};
    add_to_nbsearch(get_body_nbcoords(data), index);
    *body_id = index;
    return 0;
}

int PHYSICS_del_body(size_t body_id) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to delete body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    remove_from_nbsearch(get_body_nbcoords(bodies[body_id].body), body_id);
    bodies[body_id] = (body_data){0};
    return 0;
}

int PHYSICS_get_body(size_t body_id, body* data) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to get body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    *data = bodies[body_id].body;
    return 0;
}

int PHYSICS_add_velocity(size_t body_id, double vel_x, double vel_y) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to add velocity to body_id=%lu but this is outside "
                    "the buffer range",
                    body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to add velocity to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    bodies[body_id].body.vel_x += vel_x;
    bodies[body_id].body.vel_y += vel_y;
    return 0;
}

int PHYSICS_set_velocity(size_t body_id, double vel_x, double vel_y) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set velocity to body_id=%lu but this is outside "
                    "the buffer range",
                    body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set velocity to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    bodies[body_id].body.vel_x = vel_x;
    bodies[body_id].body.vel_y = vel_y;
    return 0;
}

int PHYSICS_drop_down(size_t body_id) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to drop down body_id=%lu but this is outside the "
                    "buffer range",
                    body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to drop down inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    bodies[body_id].drop_timer = PHYSICS_DROP_TIMER;
    return 0;
}

int PHYSICS_set_position(size_t body_id, double x, double y) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set position to body_id=%lu but this is outside "
                    "the buffer range",
                    body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set position to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    nbcoords nbc0 = get_body_nbcoords(bodies[body_id].body);
    bodies[body_id].body.x = x;
    bodies[body_id].body.y = y;
    nbcoords nbc = get_body_nbcoords(bodies[body_id].body);
    update_nbsearch(nbc0, nbc, body_id);
    return 0;
}

int PHYSICS_update_body(size_t body_id, body data) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to update body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (data.w == 0 || data.h == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to update with invalid sprite data: { w = %d, h = %d }",
            data.w,
            data.h);
        return 2;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to update inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    nbcoords nbc0 = get_body_nbcoords(bodies[body_id].body);
    bodies[body_id].body = data;
    update_nbsearch(nbc0, get_body_nbcoords(data), body_id);
    return 0;
}



void PHYSICS_start(void) {
    bodies = SDL_calloc(STARTING_BODIES_DYNARR_CAP, sizeof(body_data));
    bodies_cap = STARTING_BODIES_DYNARR_CAP;
    // nbsearch cells start zeroed and allocate lazily on their first push
}

void PHYSICS_stop(void) {
    SDL_free(bodies);
    bodies = NULL;
    bodies_cap = 0;
    for (Uint16 x = 0; x < TILEMAP_SIZE_X; x++) {
        for (Uint16 y = 0; y < TILEMAP_SIZE_Y; y++) {
            clear_dynarr(&nbsearch[x][y]);
        }
    }
}



void PHYSICS_update(void) {
    double dt = GAME_MANAGER_get_current_dt();
    for (size_t i = 0; i < bodies_cap; i++) {
        if (bodies[i].body.w == 0 || bodies[i].body.h == 0) continue;
        if (bodies[i].sleeping) continue;
        bodies[i].splash_cooldown -= dt;
        bodies[i].drop_timer -= dt;
        if (SDL_fabs(bodies[i].body.vel_x) <= EPSILON &&
            SDL_fabs(bodies[i].body.vel_y) <= EPSILON)
            continue;

        double dvx = dt * bodies[i].body.vel_x;
        double dvy = dt * bodies[i].body.vel_y;
        double vel_x0 = bodies[i].body.vel_x;
        double vel_y0 = bodies[i].body.vel_y;

        // body_base types collide only with the tilemap, body_area don't
        // collide at all.
        nbcoords nbc0 = get_body_nbcoords(bodies[i].body);
        double x0 = bodies[i].body.x;
        double y0 = bodies[i].body.y;
        if (bodies[i].body.type == physics_actor) {
            move_body_horizontally(&bodies[i], dvx);
            move_body_vertically(&bodies[i], dvy);
        } else {
            bodies[i].body.x += dvx;
            bodies[i].body.y += dvy;
        }
        update_nbsearch(nbc0, get_body_nbcoords(bodies[i].body), i);

        // Only bodies that actually moved disturb the fluids, so ripples
        // around still ones fade out.
        if (SDL_fabs(bodies[i].body.x - x0) > EPSILON ||
            SDL_fabs(bodies[i].body.y - y0) > EPSILON) {
            ripple_fluids(bodies[i].body, vel_x0, vel_y0);
            splash_fluids(&bodies[i], x0, y0, vel_y0);
        }
    }
}



int PHYSICS_get_intersecting_bodies(SDL_Rect rect, dynarr* arr) {
    if (rect.x < 0 || rect.y < 0 ||
        rect.x + rect.w > TILEMAP_SIZE_X * SPRITESHEET_CELL_X ||
        rect.y + rect.h > TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get bodies intersecting rect { x = %d, y = %d, "
                    "w = %d, h = %d } but it is outside the tilemap",
                    rect.x,
                    rect.y,
                    rect.w,
                    rect.h);
        return 1;
    }
    if (!arr || !arr->at) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get intersecting bodies into an uninitialized "
                    "dynarr");
        return 2;
    }
    arr->len = 0;
    if (rect.w <= 0 || rect.h <= 0) return 0;

    nbcoords nbc = get_rect_nbcoords(rect.x, rect.y, rect.w, rect.h);
    for (Uint16 x = nbc.tl.x; x <= nbc.br.x; x++) {
        for (Uint16 y = nbc.tl.y; y <= nbc.br.y; y++) {
            for (size_t i = 0; i < nbsearch[x][y].len; i++) {
                size_t id = (size_t)nbsearch[x][y].at[i];
                if (bodies[id].sleeping) continue;
                body b = bodies[id].body;

                // A body spanning many cells is only checked on the first
                // cell both it and rect share, to avoid duplicates
                nbcoords bnbc = get_body_nbcoords(b);
                if (x != SDL_max(bnbc.tl.x, nbc.tl.x) ||
                    y != SDL_max(bnbc.tl.y, nbc.tl.y))
                    continue;

                if (are_intersecting(
                        b.x, b.y, b.w, b.h, rect.x, rect.y, rect.w, rect.h))
                    push_to_dynarr((void*)id, arr);
            }
        }
    }
    return 0;
}

int PHYSICS_is_on_floor(size_t body_id, bool* check) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    body b = bodies[body_id].body;
    double by = SDL_round(b.y + b.h);
    if ((int)by % SPRITESHEET_CELL_Y != 0) {
        *check = false;
        return 0;
    }
    double bl = SDL_round(b.x);
    double br = SDL_round(b.x + b.w);
    Uint16 ytile = pos_to_tile_y(by);
    Uint16 from = pos_to_tile_x(bl);
    Uint16 to = pos_to_tile_x(br) - ((int)br % SPRITESHEET_CELL_X == 0 ? 1 : 0);
    for (Uint16 i = from; i <= to; i++) {
        tile data = (tile){0};
        int r = TILEMAP_get_at(i, ytile, &data);
        if (r == 1 || data.type) {
            *check = true;
            return 0;
        }
    }
    *check = false;
    return 0;
}

int PHYSICS_is_on_platform(size_t body_id, bool* check) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    body b = bodies[body_id].body;
    double by = SDL_round(b.y + b.h);
    if ((int)by % SPRITESHEET_CELL_Y != 0) {
        *check = false;
        return 0;
    }
    double bl = SDL_round(b.x);
    double br = SDL_round(b.x + b.w);
    Uint16 ytile = pos_to_tile_y(by);
    Uint16 from = pos_to_tile_x(bl);
    Uint16 to = pos_to_tile_x(br) - ((int)br % SPRITESHEET_CELL_X == 0 ? 1 : 0);
    bool found = false;
    for (Uint16 i = from; i <= to; i++) {
        tile data = (tile){0};
        int r = TILEMAP_get_at(i, ytile, &data);
        if (r == 1 || (data.type && !data.platform)) {
            *check = false;
            return 0;
        }
        if (data.platform) found = true;
    }
    *check = found;
    return 0;
}

int PHYSICS_is_on_ceiling(size_t body_id, bool* check) {
    if (body_id >= bodies_cap || body_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (bodies[body_id].body.w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    body b = bodies[body_id].body;
    double by = SDL_round(b.y);
    if (by == 0.0f) {
        *check = true;
        return 0;
    }
    if ((int)by % SPRITESHEET_CELL_Y != 0) {
        *check = false;
        return 0;
    }
    double bl = SDL_round(b.x);
    double br = SDL_round(b.x + b.w);
    Uint16 ytile = pos_to_tile_y(by) - 1;
    Uint16 from = pos_to_tile_x(bl);
    Uint16 to = pos_to_tile_x(br) - ((int)br % SPRITESHEET_CELL_X == 0 ? 1 : 0);
    for (Uint16 i = from; i <= to; i++) {
        tile data = (tile){0};
        int r = TILEMAP_get_at(i, ytile, &data);
        if (r == 1 || (data.type && !data.platform)) {
            *check = true;
            return 0;
        }
    }
    *check = false;
    return 0;
}
