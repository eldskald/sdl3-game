#include <SDL3/SDL.h>
#include <data-structs/dynarr.h>
#include <defs.h>
#include <engine/game-manager.h>
#include <engine/physics.h>
#include <engine/tilemap.h>



static body* bodies = NULL;
static size_t bodies_cap = 0;

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
    return inter.w > FABS_ZERO_DIFF && inter.h > FABS_ZERO_DIFF;
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
                if (r == 1 || data.mat != tile_null) {
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
                if (r == 1 || data.mat != tile_null) {
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
                if (r == 1 || data.mat != tile_null) {
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
                if (r == 1 || data.mat != tile_null) {
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
            *body_id = i;
            return 0;
        }
    }
    double_dynarr();
    size_t index = (bodies_cap >> 1) + 1;
    bodies[index] = data;
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
    bodies[body_id].x = x;
    bodies[body_id].y = y;
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
    bodies[body_id] = data;
    return 0;
}



void PHYSICS_start(void) {
    bodies = SDL_calloc(STARTING_SPRITES_DYNARR_CAP, sizeof(body));
    bodies_cap = STARTING_SPRITES_DYNARR_CAP;
}

void PHYSICS_stop(void) {
    SDL_free(bodies);
    bodies = NULL;
    bodies_cap = 0;
}



void PHYSICS_update(void) {
    for (size_t i = 0; i < bodies_cap; i++) {
        if (bodies[i].w == 0 || bodies[i].h == 0) continue;
        if (bodies[i].sleeping) continue;
        if (SDL_fabs(bodies[i].vel_x) <= FABS_ZERO_DIFF &&
            SDL_fabs(bodies[i].vel_y) <= FABS_ZERO_DIFF)
            continue;

        double dt = GAME_MANAGER_get_current_dt();
        double dvx = dt * bodies[i].vel_x;
        double dvy = dt * bodies[i].vel_y;

        // body_base types collide only with the tilemap, body_area don't
        // collide at all.
        if (bodies[i].type == physics_actor) {
            move_body_horizontally(&bodies[i], dvx);
            move_body_vertically(&bodies[i], dvy);
        } else {
            bodies[i].x += dvx;
            bodies[i].y += dvy;
        }
    }
}



dynarr PHYSICS_get_intersecting_bodies(SDL_Rect rect) {
    dynarr arr = (dynarr){0};
    init_dynarr(&arr);
    for (size_t i = 0; i < bodies_cap; i++) {
        if (bodies[i].w == 0 || bodies[i].h == 0) continue;
        if (bodies[i].sleeping) continue;
        if (are_intersecting(bodies[i].x,
                             bodies[i].y,
                             bodies[i].w,
                             bodies[i].h,
                             rect.x,
                             rect.y,
                             rect.w,
                             rect.h))
            push_to_dynarr((void*)i, &arr);
    }
    return arr;
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
        if (r == 1 || data.mat != tile_null) {
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
        if (r == 1 || data.mat != tile_null) {
            *check = true;
            return 0;
        }
    }
    *check = false;
    return 0;
}
