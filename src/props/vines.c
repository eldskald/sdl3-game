#include "props/vines.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/physics.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include <SDL3/SDL.h>

#ifdef TEST
#include "stubs.h"
#endif

#define VINE_SEGMENT_LENGTH (SPRITESHEET_CELL_Y / 2.0f)
#define VINE_FRAMES_TOTAL 3

// Settings for all vines, loaded from the props and ripples CSVs.
typedef struct {
    Uint8 col_r, col_g, col_b, col_a;
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
// `used` is so we know an empty index, since its points keep the vine's index
// as their `on_delete_arg`.
typedef struct {
    dynarr chain;
    dynarr under_fluid;
    double fade;
    bool used;
} vine;

// A run of visible segments of a vine, `len` points starting at `start` on its
// frame's `points`, all drawn with `alpha`.
typedef struct {
    size_t start, len;
    Uint8 alpha;
} vine_strip;

// What the update function saw of the vines, for the draw function to draw.
// Points are rounded world pixel coordinates, so the camera is only applied
// when drawing.
typedef struct {
    SDL_FPoint* points;
    size_t points_len, points_cap;
    vine_strip* strips;
    size_t strips_len, strips_cap;
} vine_frame;



static vine* vines = NULL;
static size_t vines_cap = 0;

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

static void double_vines_dynarr(void) {
    vine* new_vines = SDL_calloc(vines_cap << 1, sizeof(vine));
    SDL_memcpy(new_vines, vines, vines_cap * sizeof(vine));
    SDL_free(vines);
    vines_cap = vines_cap << 1;
    vines = new_vines;
}

// Might double the vines array, so don't hold on to vine pointers across it.
static size_t add_vine(double fade) {
    size_t index = vines_cap;
    for (size_t idx = 0; idx < vines_cap; idx++) {
        if (!vines[idx].used) {
            index = idx;
            break;
        }
    }
    if (index == vines_cap) double_vines_dynarr();
    vines[index] = (vine){.fade = fade, .used = true};
    init_dynarr(&vines[index].chain);
    init_dynarr(&vines[index].under_fluid);
    return index;
}

static size_t chain_at(vine* v, size_t i) {
    return (size_t)v->chain.at[i];
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
    while (at < vines[vine_idx].chain.len &&
           chain_at(&vines[vine_idx], at) != point_id)
        at++;
    if (at == vines[vine_idx].chain.len) return;

    if (at + 1 < vines[vine_idx].chain.len) {
        size_t tail_idx = add_vine(vines[vine_idx].fade);
        vine* v = &vines[vine_idx];
        vine* tail = &vines[tail_idx];
        for (size_t i = at + 1; i < v->chain.len; i++) {
            push_to_dynarr(v->chain.at[i], &tail->chain);
            push_to_dynarr(v->under_fluid.at[i], &tail->under_fluid);
            PHYSICS_set_prop_point_on_delete(
                chain_at(v, i), on_point_deleted, tail_idx);
        }
    }

    vine* v = &vines[vine_idx];
    while (v->chain.len > at) {
        remove_at_from_dynarr(v->chain.len - 1, &v->chain);
        remove_at_from_dynarr(v->under_fluid.len - 1, &v->under_fluid);
    }
    if (v->chain.len == 0) {
        clear_dynarr(&v->chain);
        clear_dynarr(&v->under_fluid);
        *v = (vine){0};
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
        push_to_dynarr((void*)id, &vines[vine_idx].chain);
        push_to_dynarr((void*)(size_t)false, &vines[vine_idx].under_fluid);
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
    set_pinned(chain_at(&vines[idx], 0), true);
    if (double_pinned) set_pinned(chain_at(&vines[idx], segments), true);
    return 0;
}

#ifdef TEST
dynarr VINES_test_get_vine(size_t vine_id) {
    dynarr chain = (dynarr){0};
    init_dynarr(&chain);
    for (size_t i = 0; i < vines[vine_id].chain.len; i++)
        push_to_dynarr(vines[vine_id].chain.at[i], &chain);
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
        push_to_dynarr((void*)point_id, doomed);
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
    while (vines[idx].used)
        PHYSICS_del_prop_point(chain_at(&vines[idx], vines[idx].chain.len - 1));
}

static void push_frame_point(vine_frame* f, SDL_FPoint p) {
    if (f->points_len == f->points_cap) {
        f->points_cap = f->points_cap << 1;
        f->points = SDL_realloc(f->points, f->points_cap * sizeof(SDL_FPoint));
    }
    f->points[f->points_len] = p;
    f->points_len++;
}

static void push_frame_strip(vine_frame* f, vine_strip s) {
    if (f->strips_len == f->strips_cap) {
        f->strips_cap = f->strips_cap << 1;
        f->strips = SDL_realloc(f->strips, f->strips_cap * sizeof(vine_strip));
    }
    f->strips[f->strips_len] = s;
    f->strips_len++;
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
    double alpha = settings.col_a;
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
                push_frame_strip(f,
                                 (vine_strip){.start = f->points_len,
                                              .len = 1,
                                              .alpha = (Uint8)alpha});
                push_frame_point(f, a);
                open = true;
            }
            push_frame_point(f, b);
            f->strips[f->strips_len - 1].len++;
        }
        a = b;
    }
}

void VINES_update(void) {
    double dt = GAME_MANAGER_get_current_dt();
    SDL_FRect cam = CAMERA_get_rect();
    vine_frame* f = &frames[back];
    f->points_len = 0;
    f->strips_len = 0;

    // Deleting points mid-chain splits vines on `on_point_deleted()`, so we
    // only delete the ones doomed by fluids after looping through vines.
    dynarr doomed = (dynarr){0};
    init_dynarr(&doomed);

    for (size_t idx = 0; idx < vines_cap; idx++) {
        if (!vines[idx].used) continue;
        vine* v = &vines[idx];

        size_t doomed_before = doomed.len;
        for (size_t i = 0; i < v->chain.len; i++)
            v->under_fluid.at[i] = (void*)(size_t)check_fluid(
                chain_at(v, i), (bool)v->under_fluid.at[i], &doomed);

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
        PHYSICS_del_prop_point((size_t)doomed.at[i]);
    clear_dynarr(&doomed);

    SDL_LockMutex(frames_mutex);
    Uint8 swap = back;
    back = ready;
    ready = swap;
    fresh = true;
    SDL_UnlockMutex(frames_mutex);
}



void VINES_draw(SDL_Renderer* renderer) {
    SDL_LockMutex(frames_mutex);
    if (fresh) {
        Uint8 swap = front;
        front = ready;
        ready = swap;
        fresh = false;
    }
    SDL_UnlockMutex(frames_mutex);

#ifndef TEST
    SDL_BlendMode blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &blend_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    SDL_FRect cam = CAMERA_get_rect();
    vine_frame* f = &frames[front];
    for (size_t s = 0; s < f->strips_len; s++) {
        vine_strip strip = f->strips[s];
#ifndef TEST
        SDL_SetRenderDrawColor(renderer,
                               settings.col_r,
                               settings.col_g,
                               settings.col_b,
                               strip.alpha);
#endif
        for (size_t i = strip.start + 1; i < strip.start + strip.len; i++) {
            SDL_FPoint a = f->points[i - 1];
            SDL_FPoint b = f->points[i];
#ifndef TEST
            SDL_RenderLine(
                renderer, a.x - cam.x, a.y - cam.y, b.x - cam.x, b.y - cam.y);
#else
            stubbed_SDL_RenderLine(
                a.x - cam.x, a.y - cam.y, b.x - cam.x, b.y - cam.y);
#endif
        }
    }

#ifndef TEST
    SDL_SetRenderDrawBlendMode(renderer, blend_mode);
#endif
}



static void load_vine_data(const char* csv) {
    hashmap fluids_map = FLUIDS_get_string_fluid_types_map();
    dynarr parsed = CSV_parse(csv);

    for (int i = 0; i < parsed.len; i++) {
        dynarr* row = (dynarr*)parsed.at[i];
        char* key = (char*)row->at[0];
        if (SDL_strcmp(key, "vine_color") == 0) {
            settings.col_r = SDL_strtod((char*)row->at[1], NULL);
            settings.col_g = SDL_strtod((char*)row->at[2], NULL);
            settings.col_b = SDL_strtod((char*)row->at[3], NULL);
            settings.col_a = SDL_strtod((char*)row->at[4], NULL);
        } else if (SDL_strcmp(key, "vine_gravity") == 0)
            settings.gravity = SDL_strtod((char*)row->at[1], NULL);
        else if (SDL_strcmp(key, "vine_fluid_gravity") == 0)
            settings.fluid_gravity = SDL_strtod((char*)row->at[1], NULL);
        else if (SDL_strcmp(key, "vine_destroyer_fluids") == 0) {
            for (size_t j = 1; j < row->len; j++) {
                fluid_type type = (fluid_type)(size_t)get_from_hashmap(
                    (char*)row->at[j], &fluids_map);
                settings.destroyer_fluids[type] = true;
            }
        } else if (SDL_strcmp(key, "vine_cut_duration") == 0)
            settings.cut_duration = SDL_strtod((char*)row->at[1], NULL);
        else if (SDL_strcmp(key, "vine_fade_time") == 0)
            settings.fade_time = SDL_strtod((char*)row->at[1], NULL);
    }

    clear_hashmap(&fluids_map);
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Vines data loaded successfully.");
}

static void load_ripples_data(const char* csv) {
    dynarr parsed = CSV_parse(csv);

    for (int i = 0; i < parsed.len; i++) {
        char* key = (char*)((dynarr*)parsed.at[i])->at[0];
        char* val = (char*)((dynarr*)parsed.at[i])->at[1];
        if (SDL_strcmp(key, "prop_max_speed") == 0)
            settings.ripple_max_speed = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "prop_min_speed") == 0)
            settings.ripple_min_speed = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "prop_ripple_strength") == 0)
            settings.ripple_strength = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "new_prop_ripple_pix_size") == 0)
            settings.ripple_size = SDL_strtol(val, NULL, 10);
    }

    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Vines ripples data loaded successfully.");
}

void VINES_start(void) {
#ifndef TEST
    char csv[] = {
#embed PHYSICS_PROPS_CSV_PATH suffix(, '\0')
    };
    char ripples_csv[] = {
#embed FLUIDS_RIPPLES_CSV_PATH suffix(, '\0')
    };
#else
    char csv[] = {
#embed TESTS_PHYSICS_PROPS_CSV_PATH suffix(, '\0')
    };
    char ripples_csv[] = {
#embed TESTS_FLUIDS_RIPPLES_CSV_PATH suffix(, '\0')
    };
#endif

    load_vine_data(csv);
    load_ripples_data(ripples_csv);

    vines = SDL_calloc(PROPS_VINE_STARTING_DYNARR_CAP, sizeof(vine));
    vines_cap = PROPS_VINE_STARTING_DYNARR_CAP;

    for (Uint8 i = 0; i < VINE_FRAMES_TOTAL; i++) {
        frames[i] = (vine_frame){
            .points = SDL_calloc(PROPS_VINE_FRAME_STARTING_POINTS_CAP,
                                 sizeof(SDL_FPoint)),
            .points_cap = PROPS_VINE_FRAME_STARTING_POINTS_CAP,
            .strips = SDL_calloc(PROPS_VINE_FRAME_STARTING_STRIPS_CAP,
                                 sizeof(vine_strip)),
            .strips_cap = PROPS_VINE_FRAME_STARTING_STRIPS_CAP,
        };
    }
    back = 0;
    ready = 1;
    front = 2;
    fresh = false;
    frames_mutex = SDL_CreateMutex();
}

void VINES_stop(void) {
    for (size_t idx = 0; idx < vines_cap; idx++) {
        clear_dynarr(&vines[idx].chain);
        clear_dynarr(&vines[idx].under_fluid);
    }
    SDL_free(vines);
    vines = NULL;
    vines_cap = 0;

    for (Uint8 i = 0; i < VINE_FRAMES_TOTAL; i++) {
        SDL_free(frames[i].points);
        SDL_free(frames[i].strips);
        frames[i] = (vine_frame){0};
    }
    SDL_DestroyMutex(frames_mutex);
    frames_mutex = NULL;
}
