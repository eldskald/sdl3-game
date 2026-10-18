#include "engine/physics.h"
#include "data-structs/dynarr.h"
#include "defs.h"
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



static body* bodies = NULL;
static size_t bodies_cap = 0;
static dynarr nbsearch[TILEMAP_SIZE_X][TILEMAP_SIZE_Y] = {0};

static void double_dynarr(void) {
    body* new_sprites = SDL_calloc(bodies_cap << 1, sizeof(body));
    SDL_memcpy(new_sprites, bodies, bodies_cap * sizeof(body));
    SDL_free(bodies);
    bodies_cap = bodies_cap << 1;
    bodies = new_sprites;
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

// Cells occupied by a rect snapped to pixels, w and h must be positive. Edges
// exactly on a cell border don't occupy the next cell.
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



static void move_body_horizontally(body* b, double d) {

    // tiles the corners of the body is in, snapped to pixels
    double tl_x = SDL_round(b->x);
    double tl_y = SDL_round(b->y);
    double tr_x = SDL_round(b->x + b->w);
    // double tr_y = SDL_round(b->y);
    // double bl_x = SDL_round(b->x);
    double bl_y = SDL_round(b->y + b->h);
    // double br_x = SDL_round(b->x + b->w);
    // double br_y = SDL_round(b->y + b->h);

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
                if (r == 1 || data.mat) {
                    if (SDL_roundf(b->y + b->h) <= SPRITESHEET_CELL_Y * j ||
                        SDL_roundf(b->y) >= SPRITESHEET_CELL_Y * (j + 1))
                        continue;
                    found = true;
                    b->x = SPRITESHEET_CELL_X * i - b->w;
                    b->vel_x = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->x += d;
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
                if (r == 1 || data.mat) {
                    if (SDL_roundf(b->y + b->h) <= SPRITESHEET_CELL_Y * j ||
                        SDL_roundf(b->y) >= SPRITESHEET_CELL_Y * (j + 1))
                        continue;
                    found = true;
                    b->x = SPRITESHEET_CELL_X * (i + 1);
                    b->vel_x = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->x += d;
    }
}

static void move_body_vertically(body* b, double d) {

    // tiles the corners of the body is in
    double tl_x = SDL_round(b->x);
    double tl_y = SDL_round(b->y);
    double tr_x = SDL_round(b->x + b->w);
    // double tr_y = SDL_round(b->y);
    // double bl_x = SDL_round(b->x);
    double bl_y = SDL_round(b->y + b->h);
    // double br_x = SDL_round(b->x + b->w);
    // double br_y = SDL_round(b->y + b->h);

    // when moving down
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
                if (r == 1 || data.mat) {
                    if (SDL_roundf(b->x + b->w) <= SPRITESHEET_CELL_X * i ||
                        SDL_roundf(b->x) >= SPRITESHEET_CELL_X * (i + 1))
                        continue;
                    found = true;
                    b->y = SPRITESHEET_CELL_Y * j - b->h;
                    b->vel_y = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->y += d;
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
                if (r == 1 || data.mat) {
                    if (SDL_roundf(b->x + b->w) <= SPRITESHEET_CELL_X * i ||
                        SDL_roundf(b->x) >= SPRITESHEET_CELL_X * (i + 1))
                        continue;
                    found = true;
                    b->y = SPRITESHEET_CELL_Y * (j + 1);
                    b->vel_y = 0.0f;
                    break;
                }
            }
            if (found) break;
        }
        // When no collisions were detected, move the full length
        if (!found) b->y += d;
    }
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
        if (bodies[i].w == 0) {
            bodies[i] = data;
            add_to_nbsearch(get_body_nbcoords(data), i);
            *body_id = i;
            return 0;
        }
    }
    double_dynarr();
    size_t index = bodies_cap >> 1;
    bodies[index] = data;
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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    remove_from_nbsearch(get_body_nbcoords(bodies[body_id]), body_id);
    bodies[body_id] = (body){0};
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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    *data = bodies[body_id];
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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to add velocity to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    bodies[body_id].vel_x += vel_x;
    bodies[body_id].vel_y += vel_y;
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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set velocity to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    bodies[body_id].vel_x = vel_x;
    bodies[body_id].vel_y = vel_y;
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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set position to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    nbcoords nbc0 = get_body_nbcoords(bodies[body_id]);
    bodies[body_id].x = x;
    bodies[body_id].y = y;
    nbcoords nbc = get_body_nbcoords(bodies[body_id]);
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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to update inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    nbcoords nbc0 = get_body_nbcoords(bodies[body_id]);
    bodies[body_id] = data;
    update_nbsearch(nbc0, get_body_nbcoords(data), body_id);
    return 0;
}



void PHYSICS_start(void) {
    bodies = SDL_calloc(STARTING_BODIES_DYNARR_CAP, sizeof(body));
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
    for (size_t i = 0; i < bodies_cap; i++) {
        if (bodies[i].w == 0 || bodies[i].h == 0) continue;
        if (bodies[i].sleeping) continue;
        if (SDL_fabs(bodies[i].vel_x) <= EPSILON &&
            SDL_fabs(bodies[i].vel_y) <= EPSILON)
            continue;

        double dt = GAME_MANAGER_get_current_dt();
        double dvx = dt * bodies[i].vel_x;
        double dvy = dt * bodies[i].vel_y;

        // body_base types collide only with the tilemap, body_area don't
        // collide at all.
        nbcoords nbc0 = get_body_nbcoords(bodies[i]);
        if (bodies[i].type == physics_actor) {
            move_body_horizontally(&bodies[i], dvx);
            move_body_vertically(&bodies[i], dvy);
        } else {
            bodies[i].x += dvx;
            bodies[i].y += dvy;
        }
        update_nbsearch(nbc0, get_body_nbcoords(bodies[i]), i);
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
                body b = bodies[id];
                if (b.sleeping) continue;

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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    body b = bodies[body_id];
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
        if (r == 1 || data.mat) {
            *check = true;
            return 0;
        }
    }
    *check = false;
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
    if (bodies[body_id].w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    body b = bodies[body_id];
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
        if (r == 1 || data.mat) {
            *check = true;
            return 0;
        }
    }
    *check = false;
    return 0;
}
