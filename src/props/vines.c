#include "props/vines.h"
#include "data-structs/dynarr.h"
#include "data-structs/pool.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/physics.h"
#include "engine/renderer.h"
#include "engine/tilemap.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

#define VINE_SEGMENT_LENGTH (SPRITESHEET_CELL_Y / 2.0f)
#define VINE_FRAMES_TOTAL 3

// Settings for all vines, loaded from the props and ripples CSVs. `col` is
// loaded in the usual `0` to `255` range and converted to sRGB's `0.0` to
// `1.0`.
typedef struct {
    SDL_FColor col;
    double gravity;
    double fluid_gravity;
    bool destroyer_fluids[FLUIDS_TOTAL];
    double cut_duration;
    double fade_time;
    double ripple_min_speed;
    double ripple_max_speed;
    double ripple_strength;
    Uint32 ripple_size;
} vine_data;

// Each vine is a chain of prop points on `chain`, each one connected to the
// next. Its ends are its pins, it's loose once neither of them is pinned and
// then `fade` counts the time until it's deleted. `under_fluid` has a bool for
// each point on `chain`, whether it was in a fluid cell on the last update.
// `(x,y,w,h)` is the rect bounding its points on the last update, in pixels.
// Vines are on a pool since their points keep the vine's id as their
// `on_delete_arg`.
typedef struct {
    dynarr chain;       // size_t dynarr
    dynarr under_fluid; // bool dynarr
    double x, y, w, h;
    double fade;
} vine;

// A run of visible segments of a vine, `len` points starting at `start` on its
// frame's `points`, all drawn with `alpha`.
typedef struct {
    size_t start, len;
    float alpha;
} vine_strip;

// What the update function saw of the vines, for the draw function to draw.
// Points are rounded world pixel coordinates, so the camera is only applied
// when drawing.
typedef struct {
    dynarr points; // SDL_FPoint dynarr
    dynarr strips; // vine_strip dynarr
} vine_frame;



static pool vines = (pool){0};

static vine_data settings = (vine_data){0};

// Triple buffering: the update function builds `back` and swaps it with
// `ready`, the draw function swaps `ready` with `front` when `fresh` and draws
// `front`. Only the swaps need `frames_mutex`, each side owns its own frame.
static vine_frame frames[VINE_FRAMES_TOTAL] = {0};
static Uint8 back = 0;
static Uint8 ready = 1;
static Uint8 front = 2;
static bool fresh = false;
static SDL_Mutex* frames_mutex = NULL;

// Might grow the vines pool, so don't hold on to vine pointers across it.
static size_t add_vine(double fade) {
    vine v = (vine){.fade = fade};
    init_dynarr(&v.chain, sizeof(size_t));
    init_dynarr(&v.under_fluid, sizeof(bool));
    return add_to_pool(&v, &vines);
}

static vine* get_vine(size_t id) {
    return pool_at(id, &vines);
}

static size_t chain_at(vine* v, size_t i) {
    return *(size_t*)dynarr_at(i, &v->chain);
}

static bool is_in_tilemap(double x, double y) {
    return x >= 0.0f && x <= TILEMAP_SIZE_X * SPRITESHEET_CELL_X && y >= 0.0f &&
           y <= TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y;
}

// Pins must be on the bottom side of a tile, so their `y` must be on a tile
// boundary.
static bool is_on_tile_bottom(double y) {
    return SDL_fmod(y, SPRITESHEET_CELL_Y) == 0.0f;
}

// Called by the physics engine when any point of the vine at `vine_idx` gets
// deleted. The vine keeps the points before it and the ones after it become a
// new vine with the same fade, so their `on_delete_arg` gets updated. A vine
// with nothing left is freed here, even when it's the vine deleting itself.
static void on_point_deleted(size_t point_id, size_t vine_idx) {
    size_t at = 0;
    while (at < get_vine(vine_idx)->chain.len &&
           chain_at(get_vine(vine_idx), at) != point_id)
        at++;
    if (at == get_vine(vine_idx)->chain.len) return;

    if (at + 1 < get_vine(vine_idx)->chain.len) {
        size_t tail_idx = add_vine(get_vine(vine_idx)->fade);
        vine* v = get_vine(vine_idx);
        vine* tail = get_vine(tail_idx);
        for (size_t i = at + 1; i < v->chain.len; i++) {
            push_to_dynarr(dynarr_at(i, &v->chain), &tail->chain);
            push_to_dynarr(dynarr_at(i, &v->under_fluid), &tail->under_fluid);
            PHYSICS_set_prop_point_on_delete(
                chain_at(v, i), on_point_deleted, tail_idx);
        }
    }

    vine* v = get_vine(vine_idx);
    while (v->chain.len > at) {
        remove_at_from_dynarr(v->chain.len - 1, &v->chain);
        remove_at_from_dynarr(v->under_fluid.len - 1, &v->under_fluid);
    }
    if (v->chain.len == 0) {
        deinit_dynarr(&v->chain);
        deinit_dynarr(&v->under_fluid);
        remove_from_pool(vine_idx, &vines);
    }
}

static double get_leg_length(const SDL_FPoint* path, size_t leg) {
    double dx = path[leg + 1].x - path[leg].x;
    double dy = path[leg + 1].y - path[leg].y;
    return SDL_sqrt(dx * dx + dy * dy);
}

static double get_path_length(const SDL_FPoint* path, size_t len) {
    double total = 0.0f;
    for (size_t leg = 0; leg + 1 < len; leg++)
        total += get_leg_length(path, leg);
    return total;
}

// Creates `segments + 1` points evenly spaced along the path through the `len`
// corners on `path` on the vine at `vine_idx`'s chain, each one connected to
// the next. Legs of zero length are fine, they're just skipped.
static void create_chain(size_t vine_idx,
                         const SDL_FPoint* path,
                         size_t len,
                         Uint8 segments) {
    double total = get_path_length(path, len);
    size_t leg = 0;
    double leg_start = 0.0f;
    size_t prev = 0;
    for (Uint16 i = 0; i <= segments; i++) {
        // Multiplying before dividing keeps whole distances exact
        double d = i * total / segments;
        while (leg + 2 < len && d > leg_start + get_leg_length(path, leg)) {
            leg_start += get_leg_length(path, leg);
            leg++;
        }
        double leg_len = get_leg_length(path, leg);
        double along = leg_len > 0.0f ? d - leg_start : 0.0f;
        double div = leg_len > 0.0f ? leg_len : 1.0f;
        prop_point p = (prop_point){
            .x = path[leg].x + (path[leg + 1].x - path[leg].x) * along / div,
            .y = path[leg].y + (path[leg + 1].y - path[leg].y) * along / div,
            .acc_y = settings.gravity,
            .on_delete_func = on_point_deleted,
            .on_delete_arg = vine_idx,
        };
        size_t id = 0;
        PHYSICS_new_prop_point(p, &id);
        push_to_dynarr(&id, &get_vine(vine_idx)->chain);
        push_to_dynarr(&(bool){false}, &get_vine(vine_idx)->under_fluid);
        if (i > 0)
            PHYSICS_connect_prop_points(
                prev, id, 0.0f, VINE_SEGMENT_LENGTH, true);
        prev = id;
    }
}

static void set_pinned(size_t point_id, bool pinned) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(point_id, &p);
    p.pinned = pinned;
    PHYSICS_edit_prop_point(point_id, p);
}

int VINES_add(const SDL_FPoint* path,
              size_t len,
              Uint8 segments,
              bool double_pinned) {
    bool valid = len >= 2 && segments > 0;
    for (size_t i = 0; valid && i < len; i++)
        valid = is_in_tilemap(path[i].x, path[i].y);
    if (valid)
        valid = get_path_length(path, len) > 0.0f &&
                is_on_tile_bottom(path[0].y) &&
                (!double_pinned || is_on_tile_bottom(path[len - 1].y));
    if (!valid) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to create vine with invalid data: { len = %lu, "
                    "segments = %d, double_pinned = %d }",
                    len,
                    segments,
                    double_pinned);
        return 2;
    }
    size_t idx = add_vine(0.0f);
    create_chain(idx, path, len, segments);
    set_pinned(chain_at(get_vine(idx), 0), true);
    if (double_pinned) set_pinned(chain_at(get_vine(idx), segments), true);
    return 0;
}

#ifdef TEST
dynarr VINES_test_get_vine(size_t vine_id) {
    vine* v = get_vine(vine_id);
    dynarr chain = (dynarr){0};
    init_dynarr(&chain, sizeof(size_t));
    for (size_t i = 0; i < v->chain.len; i++)
        push_to_dynarr(dynarr_at(i, &v->chain), &chain);
    return chain;
}
#endif



// A point has a ceiling when it's on the bottom side of a solid non-platform
// tile or at the top of the tilemap.
static bool has_ceiling(prop_point p) {
    double py = SDL_round(p.y);
    if (py == 0.0f) return true;
    if ((int)py % SPRITESHEET_CELL_Y != 0) return false;
    tile data = (tile){0};
    int r = TILEMAP_get_at(
        p.x / SPRITESHEET_CELL_X, py / SPRITESHEET_CELL_Y - 1, &data);
    return r == 1 || (data.type && !data.platform);
}

static bool check_pin(size_t point_id) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(point_id, &p);
    if (!p.pinned) return false;
    if (has_ceiling(p)) return true;
    set_pinned(point_id, false);
    return false;
}

// Queues a ripple at `(x,y)` as strong as the point was moving at
// `(vel_x,vel_y)`, up to `ripple_max_speed`. Points slower than
// `ripple_min_speed` don't ripple, so the small movements of Verlet integration
// don't disturb the fluid and ripples around still points fade out.
static void ripple_fluids(double x, double y, double vel_x, double vel_y) {
    double speed = SDL_sqrt(vel_x * vel_x + vel_y * vel_y);
    if (speed < settings.ripple_min_speed) return;
    speed = SDL_min(speed, settings.ripple_max_speed);
    FLUIDS_ripple_at(SDL_round(x),
                     SDL_round(y),
                     settings.ripple_size,
                     settings.ripple_strength * speed /
                         settings.ripple_max_speed);
}

// Checks the fluid cell the point at `point_id` is in, rippling it with the
// point's velocity before anything else changes it. If it has enough of the
// destroyer fluids, the point is added to `doomed` to be deleted once we're
// done looping through vines. Otherwise, when it enters enough of any fluid it
// stops and gets the fluid gravity, and when it leaves it gets the regular
// gravity back. Returns whether it's under fluid now.
static bool check_fluid(size_t point_id, bool was_under, dynarr* doomed) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(point_id, &p);
    fluid_cell cell[1][1];
    FLUIDS_get_at(p.x / FLUIDS_CELL_W, p.y / FLUIDS_CELL_H, 1, 1, &cell);
    if (cell[0][0].solid) return false;

    double total = 0.0f;
    double destroyer = 0.0f;
    for (Uint8 t = 0; t < FLUIDS_TOTAL; t++) {
        total += cell[0][0].volume[t];
        if (settings.destroyer_fluids[t]) destroyer += cell[0][0].volume[t];
    }
    if (total >= EPSILON) {
        double vel_x = 0.0f;
        double vel_y = 0.0f;
        PHYSICS_get_prop_point_velocity(point_id, &vel_x, &vel_y);
        ripple_fluids(p.x, p.y, vel_x, vel_y);
    }
    if (destroyer >= FLUIDS_INTERACTION_VOL_THRESHOLD) {
        push_to_dynarr(&point_id, doomed);
        return was_under;
    }

    bool under = total >= FLUIDS_INTERACTION_VOL_THRESHOLD;
    if (under && !was_under) {
        PHYSICS_set_prop_point_velocity(point_id, 0.0f, 0.0f);
        PHYSICS_set_prop_point_acceleration(
            point_id, 0.0f, settings.fluid_gravity);
    } else if (!under && was_under) {
        PHYSICS_set_prop_point_acceleration(point_id, 0.0f, settings.gravity);
    }
    return under;
}

// Deleting from the last point means each deletion only shortens the chain on
// `on_point_deleted()`, which frees the vine once the last one is gone.
static void delete_vine(size_t idx) {
    vine* v = NULL;
    while ((v = get_vine(idx)))
        PHYSICS_del_prop_point(chain_at(v, v->chain.len - 1));
}

static bool is_segment_visible(SDL_FPoint a, SDL_FPoint b, SDL_FRect cam) {
    return SDL_max(a.x, b.x) >= cam.x - PROPS_VINE_FRAME_CULL_MARGIN &&
           SDL_min(a.x, b.x) <= cam.x + cam.w + PROPS_VINE_FRAME_CULL_MARGIN &&
           SDL_max(a.y, b.y) >= cam.y - PROPS_VINE_FRAME_CULL_MARGIN &&
           SDL_min(a.y, b.y) <= cam.y + cam.h + PROPS_VINE_FRAME_CULL_MARGIN;
}

static SDL_FPoint get_frame_point(size_t point_id) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(point_id, &p);
    return (SDL_FPoint){SDL_round(p.x), SDL_round(p.y)};
}

// Adds each run of visible segments of the vine as a strip on `f`. Fades
// linearly to transparent on its last `fade_time` seconds.
static void add_to_frame(vine_frame* f, vine* v, bool loose, SDL_FRect cam) {
    float alpha = settings.col.a;
    double left = settings.cut_duration - v->fade;
    if (loose && left < settings.fade_time)
        alpha *= SDL_max(left, 0.0f) / settings.fade_time;

    bool open = false;
    SDL_FPoint a = get_frame_point(chain_at(v, 0));
    for (size_t i = 1; i < v->chain.len; i++) {
        SDL_FPoint b = get_frame_point(chain_at(v, i));
        if (!is_segment_visible(a, b, cam)) {
            open = false;
        } else {
            if (!open) {
                push_to_dynarr(&(vine_strip){.start = f->points.len,
                                             .len = 1,
                                             .alpha = alpha},
                               &f->strips);
                push_to_dynarr(&a, &f->points);
                open = true;
            }
            push_to_dynarr(&b, &f->points);
            ((vine_strip*)dynarr_at(f->strips.len - 1, &f->strips))->len++;
        }
        a = b;
    }
}

// Walks the chain to fit the vine's rect around its points.
static void update_rect(vine* v) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point(chain_at(v, 0), &p);
    double min_x = p.x;
    double max_x = p.x;
    double min_y = p.y;
    double max_y = p.y;
    for (size_t i = 1; i < v->chain.len; i++) {
        PHYSICS_get_prop_point(chain_at(v, i), &p);
        min_x = SDL_min(min_x, p.x);
        max_x = SDL_max(max_x, p.x);
        min_y = SDL_min(min_y, p.y);
        max_y = SDL_max(max_y, p.y);
    }
    v->x = min_x;
    v->y = min_y;
    v->w = max_x - min_x;
    v->h = max_y - min_y;
}

// Whether the vine's rect is entirely outside `space`, in tiles.
static bool is_outside_simulation(vine* v, SDL_Rect space) {
    double l = space.x * SPRITESHEET_CELL_X;
    double t = space.y * SPRITESHEET_CELL_Y;
    double r = (space.x + space.w) * SPRITESHEET_CELL_X;
    double b = (space.y + space.h) * SPRITESHEET_CELL_Y;
    return v->x + v->w < l || v->x >= r || v->y + v->h < t || v->y >= b;
}

void VINES_update(void) {
    double dt = GAME_MANAGER_get_current_dt();
    SDL_FRect cam = CAMERA_get_rect();
    SDL_Rect space = CAMERA_get_simulation_rect();
    vine_frame* f = &frames[back];
    clear_dynarr(&f->points);
    clear_dynarr(&f->strips);

    // Deleting points mid-chain splits vines on `on_point_deleted()`, so we
    // only delete the ones doomed by fluids after looping through vines.
    dynarr doomed = (dynarr){0};
    init_dynarr(&doomed, sizeof(size_t));

    for (size_t idx = 0; idx < vines.cap; idx++) {
        vine* v = get_vine(idx);
        if (!v) continue;

        // The simulation space is larger than the screen, so vines outside it
        // aren't drawn either.
        update_rect(v);
        if (is_outside_simulation(v, space)) continue;

        size_t doomed_before = doomed.len;
        for (size_t i = 0; i < v->chain.len; i++) {
            bool* under = dynarr_at(i, &v->under_fluid);
            *under = check_fluid(chain_at(v, i), *under, &doomed);
        }

        bool first_pinned = check_pin(chain_at(v, 0));
        bool last_pinned = check_pin(chain_at(v, v->chain.len - 1));
        bool loose = !first_pinned && !last_pinned;
        if (loose) {
            v->fade += dt;
            if (v->fade >= settings.cut_duration) {
                // Its doomed points get deleted here along with the rest
                while (doomed.len > doomed_before)
                    remove_at_from_dynarr(doomed.len - 1, &doomed);
                delete_vine(idx);
                continue;
            }
        }

        add_to_frame(f, v, loose, cam);
    }

    for (size_t i = 0; i < doomed.len; i++)
        PHYSICS_del_prop_point(*(size_t*)dynarr_at(i, &doomed));
    deinit_dynarr(&doomed);

    SDL_LockMutex(frames_mutex);
    Uint8 swap = back;
    back = ready;
    ready = swap;
    fresh = true;
    SDL_UnlockMutex(frames_mutex);
}



void VINES_draw(void) {
    SDL_LockMutex(frames_mutex);
    if (fresh) {
        Uint8 swap = front;
        front = ready;
        ready = swap;
        fresh = false;
    }
    SDL_UnlockMutex(frames_mutex);

    SDL_FRect cam = CAMERA_get_rect();
    vine_frame* f = &frames[front];
    for (size_t s = 0; s < f->strips.len; s++) {
        vine_strip strip = *(vine_strip*)dynarr_at(s, &f->strips);
        SDL_FColor col = settings.col;
        col.a = strip.alpha;
        for (size_t i = strip.start + 1; i < strip.start + strip.len; i++) {
            SDL_FPoint a = *(SDL_FPoint*)dynarr_at(i - 1, &f->points);
            SDL_FPoint b = *(SDL_FPoint*)dynarr_at(i, &f->points);
            RENDERER_draw_line(a.x - cam.x,
                               a.y - cam.y,
                               b.x - cam.x,
                               b.y - cam.y,
                               col,
                               PROPS_VINE_Z_INDEX);
        }
    }
}



static void load_vine_data(const char* csv) {
    fluid_type destroyers[FLUIDS_TOTAL] = {0};
    parsing_field fields[] = {
        {"vine_color", parsing_color, &settings.col},
        {"vine_gravity", parsing_double, &settings.gravity},
        {"vine_fluid_gravity", parsing_double, &settings.fluid_gravity},
        {"vine_destroyer_fluids",
         parsing_fluid,
         destroyers,
         .count = FLUIDS_TOTAL},
        {"vine_cut_duration", parsing_double, &settings.cut_duration},
        {"vine_fade_time", parsing_double, &settings.fade_time},
    };
    dynarr parsed = CSV_parse(csv);
    PARSING_read_keys(&parsed, fields, SDL_arraysize(fields));
    size_t n = PARSING_count_values(&parsed, "vine_destroyer_fluids");
    for (size_t i = 0; i < n && i < FLUIDS_TOTAL; i++)
        settings.destroyer_fluids[destroyers[i]] = true;
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Vines data loaded successfully.");
}

static void load_ripples_data(const char* csv) {
    parsing_field fields[] = {
        {"prop_max_speed", parsing_double, &settings.ripple_max_speed},
        {"prop_min_speed", parsing_double, &settings.ripple_min_speed},
        {"prop_ripple_strength", parsing_double, &settings.ripple_strength},
        {"new_prop_ripple_pix_size", parsing_uint32, &settings.ripple_size},
    };
    dynarr parsed = CSV_parse(csv);
    PARSING_read_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Vines ripples data loaded successfully.");
}

void VINES_start(void) {
    char csv[] = {
#embed PHYSICS_PROPS_CSV_PATH suffix(, '\0')
    };
    char ripples_csv[] = {
#embed FLUIDS_RIPPLES_CSV_PATH suffix(, '\0')
    };

    load_vine_data(csv);
    load_ripples_data(ripples_csv);

    init_pool_with(&vines, sizeof(vine), PROPS_VINE_STARTING_DYNARR_CAP);

    for (Uint8 i = 0; i < VINE_FRAMES_TOTAL; i++) {
        init_dynarr_with(&frames[i].points,
                         sizeof(SDL_FPoint),
                         PROPS_VINE_FRAME_STARTING_POINTS_CAP);
        init_dynarr_with(&frames[i].strips,
                         sizeof(vine_strip),
                         PROPS_VINE_FRAME_STARTING_STRIPS_CAP);
    }
    back = 0;
    ready = 1;
    front = 2;
    fresh = false;
    frames_mutex = SDL_CreateMutex();
}

void VINES_stop(void) {
    for (size_t idx = 0; idx < vines.cap; idx++) {
        vine* v = get_vine(idx);
        if (!v) continue;
        deinit_dynarr(&v->chain);
        deinit_dynarr(&v->under_fluid);
    }
    deinit_pool(&vines);

    for (Uint8 i = 0; i < VINE_FRAMES_TOTAL; i++) {
        deinit_dynarr(&frames[i].points);
        deinit_dynarr(&frames[i].strips);
    }
    SDL_DestroyMutex(frames_mutex);
    frames_mutex = NULL;
}
