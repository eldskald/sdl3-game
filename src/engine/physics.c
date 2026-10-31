#include "engine/physics.h"
#include "data-structs/dynarr.h"
#include "data-structs/pool.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/tilemap.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

#ifdef DEV
#include "dev/profiler.h"
#endif



// A helper struct to facilitate neighbor search, `tl` stands for top-left and
// `br` stands for bottom right so we can know which tiles to loop searching for
// tiles.
typedef struct {
    struct {
        Uint16 x, y;
    } tl;
    struct {
        Uint16 x, y;
    } br;
} nbcoords;



// Fluid cells occupied by a rect, `(x,y)` being its top left cell and `(w,h)`
// how many cells it spans. Cells above or to the left of the grid are left out,
// so `w` or `h` are `0` or less when it's entirely out.
typedef struct {
    long x, y, w, h;
} fluid_rect;

// Bodies are wrapped with the state the physics engine keeps on them, so it
// isn't exposed on `body`. `splash_cooldown` is how long until it can splash
// fluids again.
typedef struct {
    body body;
    double splash_cooldown;
    double drop_timer;
} body_data;

// Prop points are wrapped with their previous position so we don't expose it on
// `prop_point`, as well as acceleration. `simulated` is set at the start of
// each tick, so points pushed out of the simulation space mid-tick still finish
// it with the rest. `connections` is a `size_t` dynarr of the ids of its
// connections.
typedef struct {
    prop_point point;
    double prev_x, prev_y;
    double prev_dt;
    double acc_x, acc_y;
    bool simulated;
    dynarr connections;
} prop_point_data;

// Prop connections
typedef struct {
    size_t point_a, point_b;
    double elasticity;
    double length;
    bool two_ways;
} prop_connection_data;



static pool bodies = (pool){0};
static dynarr nbsearch[TILEMAP_SIZE_X][TILEMAP_SIZE_Y] = {0};
static SDL_Rect sim = (SDL_Rect){0};

static pool points = (pool){0};
static pool connections = (pool){0};

static double ripples_max_speed = 0.0f;
static double ripples_strength = 0.0f;
static Uint32 ripples_size = 0;

static double prop_global_damping = 0.0f;
static double prop_body_collision_pad = 0.0f;

static body_data* get_body(size_t id) {
    return pool_at(id, &bodies);
}

static prop_point_data* get_point(size_t id) {
    return pool_at(id, &points);
}

static prop_connection_data* get_connection(size_t id) {
    return pool_at(id, &connections);
}

// Id at `n` on `ids`, a `size_t` dynarr.
static size_t id_at(size_t n, const dynarr* ids) {
    return *(size_t*)dynarr_at(n, ids);
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
    return SDL_clamp(tile, 0.0f, (double)(size - 1));
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



// Bodies are simulated when any of their tiles is within the simulation space.
static bool is_body_simulated(body b) {
    nbcoords c = get_body_nbcoords(b);
    return c.br.x >= sim.x && c.tl.x < sim.x + sim.w && c.br.y >= sim.y &&
           c.tl.y < sim.y + sim.h;
}

static bool is_point_simulated(prop_point p) {
    Uint16 x = pos_to_tile_x(p.x);
    Uint16 y = pos_to_tile_y(p.y);
    return x >= sim.x && x < sim.x + sim.w && y >= sim.y && y < sim.y + sim.h;
}

// Points outside the simulation space don't move, so they're treated as pinned
// by the points inside it they're connected to.
static bool is_point_fixed(prop_point_data* p) {
    return p->point.pinned || !p->simulated;
}

static void add_to_nbsearch(nbcoords c, size_t idx) {
    for (Uint16 x = c.tl.x; x <= c.br.x; x++) {
        for (Uint16 y = c.tl.y; y <= c.br.y; y++) {
            push_to_dynarr(&idx, &nbsearch[x][y]);
        }
    }
}

static void remove_from_nbsearch(nbcoords c, size_t idx) {
    for (Uint16 x = c.tl.x; x <= c.br.x; x++) {
        for (Uint16 y = c.tl.y; y <= c.br.y; y++) {
            remove_val_from_dynarr(&idx, &nbsearch[x][y]);
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



// First we mark which points are within the simulation space this tick, then
// among those we destroy points whose tile got filled by a solid non-platform
// tile, like when a reaction creates new tiles, so they only get destroyed here
// on the physics thread. Then we reset accelerations back to the one set on
// `prop_point`, then we let other interactions like spring connections apply
// force and modify the acceleration on `prop_point_data` which will be the one
// actually used to move them.
static void prepare_prop_points(void) {
    tile data = (tile){0};
    for (size_t idx = 0; idx < points.cap; idx++) {
        prop_point_data* p = get_point(idx);
        if (!p) continue;
        p->simulated = is_point_simulated(p->point);
        if (!p->simulated) continue;
        TILEMAP_get_at(
            pos_to_tile_x(p->point.x), pos_to_tile_y(p->point.y), &data);
        if (data.type && !data.platform) {
            PHYSICS_del_prop_point(idx);
            continue;
        }
        if (p->point.pinned) continue;
        p->acc_x = p->point.acc_x;
        p->acc_y = p->point.acc_y;
    }
}

static void apply_connections(bool springs) {
    for (size_t idx = 0; idx < connections.cap; idx++) {
        prop_connection_data* c = get_connection(idx);
        if (!c) continue;
        if ((c->elasticity > 0.0f && !springs) ||
            (c->elasticity == 0.0f && springs))
            continue;

        prop_point_data* p_a = get_point(c->point_a);
        prop_point_data* p_b = get_point(c->point_b);
        bool fixed_a = is_point_fixed(p_a);
        bool fixed_b = is_point_fixed(p_b);
        if (fixed_a && fixed_b) continue;
        double dx = p_a->point.x - p_b->point.x;
        double dy = p_a->point.y - p_b->point.y;
        double dist = SDL_sqrt(dx * dx + dy * dy);
        if (dist < EPSILON) {
            if (!fixed_a) p_a->point.x -= EPSILON;
            if (!fixed_b) p_b->point.x += EPSILON;
            dx = p_a->point.x - p_b->point.x;
            dist = SDL_sqrt(dx * dx + dy * dy);
        }
        double diff = dist - c->length;
        if (diff <= 0.0f && !c->two_ways) continue;
        double dir_x = dx / dist;
        double dir_y = dy / dist;
        if (c->elasticity > 0.0f) {
            if (!fixed_a) {
                p_a->acc_x -= dir_x * diff / c->elasticity;
                p_a->acc_y -= dir_y * diff / c->elasticity;
            }
            if (!fixed_b) {
                p_b->acc_x += dir_x * diff / c->elasticity;
                p_b->acc_y += dir_y * diff / c->elasticity;
            }
        } else {
            if (!fixed_a) {
                double fac = fixed_b ? 1.0f : 0.5f;
                p_a->point.x -= dir_x * fac * diff;
                p_a->point.y -= dir_y * fac * diff;
            }
            if (!fixed_b) {
                double fac = fixed_a ? 1.0f : 0.5f;
                p_b->point.x += dir_x * fac * diff;
                p_b->point.y += dir_y * fac * diff;
            }
        }
    }
}

// Props' `dt` is capped, so a long stall doesn't throw points across the map
// and leave ropes tangled, they slow down instead.
static double get_prop_dt(void) {
    return SDL_min(GAME_MANAGER_get_current_dt(), PHYSICS_PROP_MAX_DT);
}

// Verlet integration. Skipped when `dt` is zero, since points that never moved
// would divide by a zero `prev_dt`.
static void move_points(void) {
    if (get_prop_dt() <= 0.0f) return;
    for (size_t idx = 0; idx < points.cap; idx++) {
        prop_point_data* p = get_point(idx);
        if (!p) continue;
        if (is_point_fixed(p)) continue;

        // Verlet integration
        double dt = get_prop_dt();
        double prev_dt = p->prev_dt > 0.0f ? p->prev_dt : dt;
        double damping = prop_global_damping * (1.0f - p->point.damping);
        double dx = (p->point.x - p->prev_x) * damping * dt / prev_dt +
                    dt * dt * p->acc_x;
        double dy = (p->point.y - p->prev_y) * damping * dt / prev_dt +
                    dt * dt * p->acc_y;
        p->prev_x = p->point.x;
        p->prev_y = p->point.y;
        p->prev_dt = dt;

        // Clampped to be inside the tilemap space
        p->point.x = SDL_clamp(p->point.x + dx,
                               EPSILON,
                               TILEMAP_SIZE_X * SPRITESHEET_CELL_X - EPSILON);
        p->point.y = SDL_clamp(p->point.y + dy,
                               EPSILON,
                               TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y - EPSILON);
    }
}

static void move_and_collide(double x0,
                             double y0,
                             double x,
                             double y,
                             double* cx,
                             double* cy,
                             bool* v_collision,
                             bool* h_collision) {
    *v_collision = false;
    *h_collision = false;
    double dx = x - x0;
    double dy = y - y0;
    tile data = (tile){0};
    Uint16 prev_tx = pos_to_tile_x(x0);
    Uint16 prev_ty = pos_to_tile_y(y0);
    Uint16 curr_tx = pos_to_tile_x(x);
    Uint16 curr_ty = pos_to_tile_y(y);

    // Only check for tile collisions if there the point moved between
    // tiles, and if it did, the tile it started in will not count and if it did
    // not, just move it and report no collisions
    if (prev_tx == curr_tx && prev_ty == curr_ty) {
        *cx = x;
        *cy = y;
        return;
    }

    // If it didn't changed tiles horizontally, we only loop vertically
    if (prev_tx == curr_tx) {
        // `y` has to be long to allow it to be negative
        for (long y = dy > 0.0f ? prev_ty + 1 : prev_ty - 1;
             dy > 0.0f ? y <= curr_ty : y >= curr_ty;
             dy > 0.0f ? y++ : y--) {
            TILEMAP_get_at(curr_tx, y, &data);
            if (data.type && !data.platform) {
                *cy = dy > 0.0f ? y * SPRITESHEET_CELL_Y - EPSILON
                                : (y + 1) * SPRITESHEET_CELL_Y + EPSILON;
                *cx = x;
                *v_collision = true;
                return;
            }
        }
        *cx = x;
        *cy = y;
        return;
    }

    // If it didn't changed tiles vertically, we only loop horizontally
    else if (prev_ty == curr_ty) {
        // `x` has to be long to allow it to be negative
        for (long x = dx > 0.0f ? prev_tx + 1 : prev_tx - 1;
             dx > 0.0f ? x <= curr_tx : x >= curr_tx;
             dx > 0.0f ? x++ : x--) {
            TILEMAP_get_at(x, curr_ty, &data);
            if (data.type && !data.platform) {
                *cx = dx > 0.0f ? x * SPRITESHEET_CELL_X - EPSILON
                                : (x + 1) * SPRITESHEET_CELL_X + EPSILON;
                *cy = y;
                *h_collision = true;
                return;
            }
        }
        *cx = x;
        *cy = y;
        return;
    }

    // If it changed tiles both horizontally and vertically, we do an
    // Amanatides-Woo type of loop. The ray equation is `(y - y0) =
    // (dy / dx) * (x - x0)`, and we can use it to find the at which
    // height the line touches the next vertical border of a tile (at `x
    // = k * SPRITESHEET_CELL_X`), if that height is still within the
    // current tile, then the next tile the ray touches is the next one
    // sideways, otherwise it's the next one above or below.
    Uint16 tx = prev_tx;
    Uint16 ty = prev_ty;
    *cx = x0;
    *cy = y0;
    while (tx != curr_tx || ty != curr_ty) {
        // `diff` is what we have to add to `x` to reach the next tile
        double diff =
            dx > 0.0f ? (double)SPRITESHEET_CELL_X -
                            (double)((int)SDL_floor(*cx) % SPRITESHEET_CELL_X) -
                            (*cx - SDL_floor(*cx))
                      : -(double)((int)SDL_floor(*cx) % SPRITESHEET_CELL_X) -
                            (*cx - SDL_floor(*cx));
        // `h` is the height in the ray when we put it at `x + diff`, if
        // it is on another tile then the ray reaches the new tile from
        // above/below, otherwise the new tile is the one at `x + diff`.
        double h = y0 + (dy / dx) * (SDL_round(diff + *cx) - x0);
        if (pos_to_tile_y(h) != ty) {
            double ydiff =
                dy > 0.0f
                    ? (double)SPRITESHEET_CELL_Y -
                          (double)((int)SDL_floor(*cy) % SPRITESHEET_CELL_Y) -
                          (*cy - SDL_floor(*cy))
                    : -(double)((int)SDL_floor(*cy) % SPRITESHEET_CELL_Y) -
                          (*cy - SDL_floor(*cy));
            *cx = x0 + (dx / dy) * (SDL_round(ydiff + *cy) - y0);
            *cy = SDL_round(ydiff + *cy);
            ty += dy > 0.0f ? 1 : -1;
            TILEMAP_get_at(tx, ty, &data);
            if (data.type && !data.platform) {
                *cy += dy > 0.0f ? -EPSILON : EPSILON;
                *v_collision = true;
                return;
            }
            *cy += dy > 0.0f ? EPSILON : -EPSILON;
        } else {
            *cx = SDL_round(diff + *cx);
            *cy = h;
            tx += dx > 0.0f ? 1 : -1;
            TILEMAP_get_at(tx, ty, &data);
            if (data.type && !data.platform) {
                *cx += dx > 0.0f ? -EPSILON : EPSILON;
                *h_collision = true;
                return;
            }
            *cx += dx > 0.0f ? EPSILON : -EPSILON;
        }
    }
    *cx = x;
    *cy = y;
}

static void collide_points(void) {
    for (size_t idx = 0; idx < points.cap; idx++) {
        prop_point_data* p = get_point(idx);
        if (!p) continue;
        if (is_point_fixed(p)) continue;

        // Extrude from bodies
        if (!p->point.ignore_bodies) {
            Uint16 tile_x = pos_to_tile_x(p->point.x);
            Uint16 tile_y = pos_to_tile_y(p->point.y);
            dynarr* nb = &nbsearch[tile_x][tile_y];
            for (size_t i = 0; i < nb->len; i++) {
                body* b = &(get_body(id_at(i, nb))->body);
                if (b->x - prop_body_collision_pad <= p->point.x &&
                    p->point.x <= b->x + b->w + prop_body_collision_pad &&
                    b->y - prop_body_collision_pad <= p->point.y &&
                    p->point.y <= b->y + b->h + prop_body_collision_pad) {
                    double dist_left =
                        p->point.x - b->x + prop_body_collision_pad;
                    double dist_right =
                        b->x + b->w - p->point.x + prop_body_collision_pad;
                    double dist_top =
                        p->point.y - b->y + prop_body_collision_pad;
                    double dist_bot =
                        b->y + b->h - p->point.y + prop_body_collision_pad;
                    if (dist_left <=
                        SDL_min(SDL_min(dist_right, dist_top), dist_bot)) {
                        p->point.x -= dist_left;
                        continue;
                    } else if (dist_right <=
                               SDL_min(SDL_min(dist_left, dist_top),
                                       dist_bot)) {
                        p->point.x += dist_right;
                        continue;
                    } else if (dist_top <=
                               SDL_min(SDL_min(dist_left, dist_right),
                                       dist_bot)) {
                        p->point.y -= dist_top;
                        continue;
                    } else
                        p->point.y += dist_bot;
                }
            }
        }

        // Clamp before trying to find collisions on the tilemap
        p->point.x = SDL_clamp(
            p->point.x, EPSILON, TILEMAP_SIZE_X * SPRITESHEET_CELL_X - EPSILON);
        p->point.y = SDL_clamp(
            p->point.y, EPSILON, TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y - EPSILON);

        double temp_x = 0.0f;
        double temp_y = 0.0f;
        bool v_collision = false;
        bool h_collision = false;
        move_and_collide(p->prev_x,
                         p->prev_y,
                         p->point.x,
                         p->point.y,
                         &temp_x,
                         &temp_y,
                         &v_collision,
                         &h_collision);

        // If we find a collision, move and collide again to slide
        if (v_collision) {
            move_and_collide(temp_x,
                             temp_y,
                             p->point.x,
                             temp_y,
                             &temp_x,
                             &temp_y,
                             &v_collision,
                             &h_collision);
        } else if (h_collision) {
            move_and_collide(temp_x,
                             temp_y,
                             temp_x,
                             p->point.y,
                             &temp_x,
                             &temp_y,
                             &v_collision,
                             &h_collision);
        }

        // Clamp one last time when done
        p->point.x = SDL_clamp(
            temp_x, EPSILON, TILEMAP_SIZE_X * SPRITESHEET_CELL_X - EPSILON);
        p->point.y = SDL_clamp(
            temp_y, EPSILON, TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y - EPSILON);
    }
}



// Queues a ripple at the center of `b` when it's in a fluid cell, as strong as
// it was moving at `(vel_x,vel_y)`, up to `ripples_max_speed`.
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
    double speed =
        SDL_min(SDL_sqrt(vel_x * vel_x + vel_y * vel_y), ripples_max_speed);
    FLUIDS_ripple_at(
        x, y, ripples_size, ripples_strength * speed / ripples_max_speed);
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



static void move_bodies(void) {
    double dt = GAME_MANAGER_get_current_dt();
    for (size_t idx = 0; idx < bodies.cap; idx++) {
        body_data* b = get_body(idx);
        if (!b) continue;
        if (!is_body_simulated(b->body)) continue;
        b->splash_cooldown -= dt;
        b->drop_timer -= dt;
        if (SDL_fabs(b->body.vel_x) <= EPSILON &&
            SDL_fabs(b->body.vel_y) <= EPSILON)
            continue;

        double vel_x0 = b->body.vel_x;
        double vel_y0 = b->body.vel_y;
        double dx = dt * vel_x0;
        double dy = dt * vel_y0;

        // `physics_actor` types collide only with the tilemap,
        // `physics_intangible` don't collide at all.
        nbcoords nbc0 = get_body_nbcoords(b->body);
        double x0 = b->body.x;
        double y0 = b->body.y;
        if (b->body.type == physics_actor) {
            move_body_horizontally(b, dx);
            move_body_vertically(b, dy);
        } else {
            b->body.x += dx;
            b->body.y += dy;
        }
        update_nbsearch(nbc0, get_body_nbcoords(b->body), idx);

        // Only bodies that actually moved disturb the fluids, so ripples
        // around still ones fade out.
        if (SDL_fabs(b->body.x - x0) > EPSILON ||
            SDL_fabs(b->body.y - y0) > EPSILON) {
            ripple_fluids(b->body, vel_x0, vel_y0);
            splash_fluids(b, x0, y0, vel_y0);
        }
    }
}



int PHYSICS_new_body(body data, size_t* body_id) {
    if (data.w == 0 || data.h == 0 || data.x < 0.0f ||
        data.x + data.w > TILEMAP_SIZE_X * SPRITESHEET_CELL_X ||
        data.y < 0.0f ||
        data.y + data.h > TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to create body with invalid data: { w = %d, h = %d }",
            data.w,
            data.h);
        return 2;
    }
    *body_id = add_to_pool(&(body_data){.body = data}, &bodies);
    add_to_nbsearch(get_body_nbcoords(data), *body_id);
    return 0;
}

int PHYSICS_del_body(size_t body_id) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to delete body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    remove_from_nbsearch(get_body_nbcoords(get_body(body_id)->body), body_id);
    remove_from_pool(body_id, &bodies);
    return 0;
}

int PHYSICS_get_body(size_t body_id, body* data) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to get body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    *data = get_body(body_id)->body;
    return 0;
}

int PHYSICS_add_body_velocity(size_t body_id, double vel_x, double vel_y) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to add velocity to body_id=%lu but this is outside "
                    "the buffer range",
                    body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to add velocity to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    get_body(body_id)->body.vel_x += vel_x;
    get_body(body_id)->body.vel_y += vel_y;
    return 0;
}

int PHYSICS_set_body_velocity(size_t body_id, double vel_x, double vel_y) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set velocity to body_id=%lu but this is outside "
                    "the buffer range",
                    body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set velocity to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    get_body(body_id)->body.vel_x = vel_x;
    get_body(body_id)->body.vel_y = vel_y;
    return 0;
}

int PHYSICS_drop_down_body(size_t body_id) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to drop down body_id=%lu but this is outside the "
                    "buffer range",
                    body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to drop down inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    get_body(body_id)->drop_timer = PHYSICS_DROP_TIMER;
    return 0;
}

int PHYSICS_set_body_position(size_t body_id, double x, double y) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set position to body_id=%lu but this is outside "
                    "the buffer range",
                    body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set position to inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    nbcoords nbc0 = get_body_nbcoords(get_body(body_id)->body);
    get_body(body_id)->body.x = x;
    get_body(body_id)->body.y = y;
    nbcoords nbc = get_body_nbcoords(get_body(body_id)->body);
    update_nbsearch(nbc0, nbc, body_id);
    return 0;
}

int PHYSICS_edit_body(size_t body_id, body data) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to edit body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (data.w == 0 || data.h == 0 || data.x < 0.0f ||
        data.x + data.w > TILEMAP_SIZE_X * SPRITESHEET_CELL_X ||
        data.y < 0.0f ||
        data.y + data.h > TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit with invalid body data: { w = %d, h = %d }",
                    data.w,
                    data.h);
        return 2;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    nbcoords nbc0 = get_body_nbcoords(get_body(body_id)->body);
    get_body(body_id)->body = data;
    update_nbsearch(nbc0, get_body_nbcoords(data), body_id);
    return 0;
}


int PHYSICS_new_prop_point(prop_point data, size_t* point_id) {
    if (data.x < 0.0f || data.x > TILEMAP_SIZE_X * SPRITESHEET_CELL_X ||
        data.y < 0.0f || data.y > TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to create prop point with invalid data: { x = %.2f, "
                    "y = %.2f }",
                    data.x,
                    data.y);
        return 2;
    }
    prop_point_data p = (prop_point_data){
        .point = data,
        .prev_x = data.x,
        .prev_y = data.y,
    };
    init_dynarr(&p.connections, sizeof(size_t));
    *point_id = add_to_pool(&p, &points);
    return 0;
}

int PHYSICS_del_prop_point(size_t point_id) {
    if (point_id >= points.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to delete point_id=%lu but this is outside the buffer range",
            point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent prop point at point_id=%lu",
                    point_id);
        return 1;
    }
    prop_point_data* p = get_point(point_id);
    for (size_t i = 0; i < p->connections.len; i++) {
        size_t* conn_id = dynarr_at(i, &p->connections);
        prop_connection_data* conn = get_connection(*conn_id);
        if (conn->point_a != point_id)
            remove_val_from_dynarr(conn_id,
                                   &get_point(conn->point_a)->connections);
        if (conn->point_b != point_id)
            remove_val_from_dynarr(conn_id,
                                   &get_point(conn->point_b)->connections);
        remove_from_pool(*conn_id, &connections);
    }
    void (*on_delete_func)(size_t, size_t) = p->point.on_delete_func;
    size_t on_delete_arg = p->point.on_delete_arg;
    deinit_dynarr(&p->connections);
    remove_from_pool(point_id, &points);

    // Called last so the prop sees the point and its connections already gone
    if (on_delete_func) on_delete_func(point_id, on_delete_arg);
    return 0;
}

int PHYSICS_get_prop_point(size_t point_id, prop_point* data) {
    if (point_id >= points.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to get point_id=%lu but this is outside the buffer range",
            point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent prop point at point_id=%lu",
                    point_id);
        return 1;
    }
    *data = get_point(point_id)->point;
    return 0;
}

int PHYSICS_edit_prop_point(size_t point_id, prop_point data) {
    if (point_id >= points.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to edit point_id=%lu but this is outside the buffer range",
            point_id);
        return 3;
    }
    if (data.x < 0.0f || data.x > TILEMAP_SIZE_X * SPRITESHEET_CELL_X ||
        data.y < 0.0f || data.y > TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit with invalid prop point data: { x = %.2f, y "
                    "= %.2f }",
                    data.x,
                    data.y);
        return 2;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit inexistent prop point at point_id=%lu",
                    point_id);
        return 1;
    }
    get_point(point_id)->point = data;
    get_point(point_id)->prev_x = data.x;
    get_point(point_id)->prev_y = data.y;
    return 0;
}

int PHYSICS_set_prop_point_acceleration(size_t point_id,
                                        double acc_x,
                                        double acc_y) {
    if (point_id >= points.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set acceleration of point_id=%lu but this is "
                    "outside the buffer range",
                    point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set acceleration of inexistent prop point at "
                    "point_id=%lu",
                    point_id);
        return 1;
    }
    get_point(point_id)->point.acc_x = acc_x;
    get_point(point_id)->point.acc_y = acc_y;
    return 0;
}

int PHYSICS_set_prop_point_damping(size_t point_id, double damping) {
    if (point_id >= points.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set damping of point_id=%lu but this is outside "
                    "the buffer range",
                    point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set damping of inexistent prop point at "
                    "point_id=%lu",
                    point_id);
        return 1;
    }
    get_point(point_id)->point.damping = damping;
    return 0;
}

// Verlet integration has no velocity, it's the distance from the previous
// position over the previous tick's dt, so we move the previous position.
int PHYSICS_set_prop_point_velocity(size_t point_id,
                                    double vel_x,
                                    double vel_y) {
    if (point_id >= points.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set velocity of point_id=%lu but this is outside "
                    "the buffer range",
                    point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set velocity of inexistent prop point at "
                    "point_id=%lu",
                    point_id);
        return 1;
    }
    prop_point_data* p = get_point(point_id);
    double prev_dt = p->prev_dt > 0.0f ? p->prev_dt : get_prop_dt();
    p->prev_x = p->point.x - vel_x * prev_dt;
    p->prev_y = p->point.y - vel_y * prev_dt;
    return 0;
}

int PHYSICS_get_prop_point_velocity(size_t point_id,
                                    double* vel_x,
                                    double* vel_y) {
    if (point_id >= points.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get velocity of point_id=%lu but this is outside "
                    "the buffer range",
                    point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get velocity of inexistent prop point at "
                    "point_id=%lu",
                    point_id);
        return 1;
    }
    // Points that never moved have no `prev_dt` and are standing still
    prop_point_data* p = get_point(point_id);
    *vel_x = p->prev_dt > 0.0f ? (p->point.x - p->prev_x) / p->prev_dt : 0.0f;
    *vel_y = p->prev_dt > 0.0f ? (p->point.y - p->prev_y) / p->prev_dt : 0.0f;
    return 0;
}

int PHYSICS_connect_prop_points(
    size_t id_a, size_t id_b, double elasticity, double length, bool two_ways) {
    if (id_a >= points.cap || id_b >= points.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to connect id_a=%lu and id_b=%lu but one or both "
                    "are outside the buffer range",
                    id_a,
                    id_b);
        return 3;
    }
    if (elasticity < 0.0f || length <= 0.0f || id_a == id_b) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to connect prop points with invalid data: "
                    "elasticity=%.2f, length=%.2f, id_a=%lu, id_b=%lu",
                    elasticity,
                    length,
                    id_a,
                    id_b);
        return 2;
    }
    if (!get_point(id_a) || !get_point(id_b)) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to connect inexistent prop points at id_a=%lu and id_b=%lu",
            id_a,
            id_b);
        return 1;
    }
    for (size_t i = 0; i < get_point(id_a)->connections.len; i++) {
        prop_connection_data* conn =
            get_connection(id_at(i, &get_point(id_a)->connections));
        if (conn->point_a == id_b || conn->point_b == id_b) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Tried to connect already connected points id_a=%lu "
                        "and id_b=%lu",
                        id_a,
                        id_b);
            return 4;
        }
    }
    size_t id = add_to_pool(
        &(prop_connection_data){
            .point_a = id_a,
            .point_b = id_b,
            .elasticity = elasticity,
            .length = length,
            .two_ways = two_ways,
        },
        &connections);
    push_to_dynarr(&id, &get_point(id_a)->connections);
    push_to_dynarr(&id, &get_point(id_b)->connections);
    return 0;
}

int PHYSICS_set_prop_point_on_delete(size_t point_id,
                                     void (*func)(size_t point_id, size_t arg),
                                     size_t arg) {
    if (point_id >= points.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to set on delete of point_id=%lu but this is "
                    "outside the buffer range",
                    point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to set on delete of inexistent prop point at point_id=%lu",
            point_id);
        return 1;
    }
    get_point(point_id)->point.on_delete_func = func;
    get_point(point_id)->point.on_delete_arg = arg;
    return 0;
}

int PHYSICS_get_connected_points(size_t point_id, dynarr* arr) {
    if (point_id >= points.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get points connected to point_id=%lu but this is "
                    "outside the buffer range",
                    point_id);
        return 3;
    }
    if (!arr || !arr->data_size) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get connected points into an uninitialized "
                    "dynarr");
        return 2;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get points connected to inexistent prop point at "
                    "point_id=%lu",
                    point_id);
        return 1;
    }
    clear_dynarr(arr);
    for (size_t i = 0; i < get_point(point_id)->connections.len; i++) {
        prop_connection_data* conn =
            get_connection(id_at(i, &get_point(point_id)->connections));
        size_t other =
            conn->point_a == point_id ? conn->point_b : conn->point_a;
        push_to_dynarr(&other, arr);
    }
    return 0;
}

#ifdef TEST
void PHYSICS_test_get_connection(size_t connection_id,
                                 size_t* point_a,
                                 size_t* point_b,
                                 double* elasticity,
                                 double* length,
                                 bool* two_ways) {
    prop_connection_data* found = get_connection(connection_id);
    prop_connection_data* conn = found ? found : &(prop_connection_data){0};
    *point_a = conn->point_a;
    *point_b = conn->point_b;
    *elasticity = conn->elasticity;
    *length = conn->length;
    *two_ways = conn->two_ways;
}
#endif



static void load_ripples_data(const char* csv) {
    parsing_field fields[] = {
        {"body_max_speed", parsing_double, &ripples_max_speed},
        {"body_ripple_strength", parsing_double, &ripples_strength},
        {"new_body_ripple_pix_size", parsing_uint32, &ripples_size},
    };
    dynarr parsed = CSV_parse(csv);
    PARSING_read_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Ripples data loaded successfully.");
}

static void load_props_data(const char* csv) {
    parsing_field fields[] = {
        {"prop_global_damping", parsing_double, &prop_global_damping},
        {"prop_body_collision_pad", parsing_double, &prop_body_collision_pad},
    };
    dynarr parsed = CSV_parse(csv);
    PARSING_read_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Props data loaded successfully.");
}

void PHYSICS_start(void) {
    char ripples_csv[] = {
#embed FLUIDS_RIPPLES_CSV_PATH suffix(, '\0')
    };
    char props_csv[] = {
#embed PHYSICS_PROPS_CSV_PATH suffix(, '\0')
    };

    load_ripples_data(ripples_csv);
    load_props_data(props_csv);

    init_pool_with(
        &bodies, sizeof(body_data), PHYSICS_STARTING_BODIES_DYNARR_CAP);
    for (Uint16 x = 0; x < TILEMAP_SIZE_X; x++)
        for (Uint16 y = 0; y < TILEMAP_SIZE_Y; y++)
            init_dynarr_with(&nbsearch[x][y], sizeof(size_t), 0);
    init_pool_with(&points,
                   sizeof(prop_point_data),
                   PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP);
    init_pool_with(&connections,
                   sizeof(prop_connection_data),
                   PHYSICS_STARTING_PROP_CONNECTIONS_DYNARR_CAP);

    sim = CAMERA_get_simulation_rect();
}

void PHYSICS_stop(void) {
    deinit_pool(&bodies);
    for (Uint16 x = 0; x < TILEMAP_SIZE_X; x++)
        for (Uint16 y = 0; y < TILEMAP_SIZE_Y; y++)
            deinit_dynarr(&nbsearch[x][y]);

    for (size_t idx = 0; idx < points.cap; idx++)
        if (get_point(idx)) deinit_dynarr(&get_point(idx)->connections);
    deinit_pool(&points);
    deinit_pool(&connections);
}



void PHYSICS_update(void) {
    sim = CAMERA_get_simulation_rect();

#if DEV && !TEST
    PROFILER_setup_timer("physics_bodies");
#endif
    move_bodies();
#if DEV && !TEST
    PROFILER_finish_timer("physics_bodies");
#endif

#if DEV && !TEST
    PROFILER_setup_timer("physics_props");
#endif
    prepare_prop_points();
    apply_connections(true);
    move_points();
    for (int i = 0; i < PHYSICS_PROP_ROPE_ITERATIONS; i++)
        apply_connections(false);
    collide_points();
#if DEV && !TEST
    PROFILER_finish_timer("physics_props");
#endif
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
    if (!arr || !arr->data_size) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get intersecting bodies into an uninitialized "
                    "dynarr");
        return 2;
    }
    clear_dynarr(arr);
    if (rect.w <= 0 || rect.h <= 0) return 0;

    nbcoords nbc = get_rect_nbcoords(rect.x, rect.y, rect.w, rect.h);
    for (Uint16 x = nbc.tl.x; x <= nbc.br.x; x++) {
        for (Uint16 y = nbc.tl.y; y <= nbc.br.y; y++) {
            for (size_t i = 0; i < nbsearch[x][y].len; i++) {
                size_t id = id_at(i, &nbsearch[x][y]);
                body b = get_body(id)->body;

                // A body spanning many cells is only checked on the first
                // cell both it and rect share, to avoid duplicates
                nbcoords bnbc = get_body_nbcoords(b);
                if (x != SDL_max(bnbc.tl.x, nbc.tl.x) ||
                    y != SDL_max(bnbc.tl.y, nbc.tl.y))
                    continue;

                if (are_intersecting(
                        b.x, b.y, b.w, b.h, rect.x, rect.y, rect.w, rect.h))
                    push_to_dynarr(&id, arr);
            }
        }
    }
    return 0;
}

int PHYSICS_is_body_on_floor(size_t body_id, bool* check) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    body b = get_body(body_id)->body;
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

        // Dropping bodies go through platforms, so they aren't floor
        if (data.platform && get_body(body_id)->drop_timer > 0.0f) continue;
        if (r == 1 || data.type) {
            *check = true;
            return 0;
        }
    }
    *check = false;
    return 0;
}

int PHYSICS_is_body_on_platform(size_t body_id, bool* check) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    if (get_body(body_id)->drop_timer > 0.0f) {
        *check = false;
        return 0;
    }
    body b = get_body(body_id)->body;
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

int PHYSICS_is_body_on_ceiling(size_t body_id, bool* check) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    body b = get_body(body_id)->body;
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

int PHYSICS_is_body_within_the_simulation(size_t body_id, bool* check) {
    if (body_id >= bodies.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check body_id=%lu but this is outside the buffer range",
            body_id);
        return 3;
    }
    if (!get_body(body_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent body at body_id=%lu",
                    body_id);
        return 1;
    }
    *check = is_body_simulated(get_body(body_id)->body);
    return 0;
}

int PHYSICS_is_prop_point_within_the_simulation(size_t point_id, bool* check) {
    if (point_id >= points.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check point_id=%lu but this is outside the buffer range",
            point_id);
        return 3;
    }
    if (!get_point(point_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check inexistent prop point at point_id=%lu",
                    point_id);
        return 1;
    }
    *check = is_point_simulated(get_point(point_id)->point);
    return 0;
}
