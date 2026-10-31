#include "engine/sprites.h"
#include "data-structs/dynarr.h"
#include "data-structs/pool.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/game-manager.h"
#include "engine/renderer.h"
#include <SDL3/SDL.h>

#define SPRITES_FRAMES_TOTAL 3

// Sprites are on a pool, so their `sprite_id` always points to them while they
// exist.
//
// A line or triangle attached to a sprite, in world pixels. Lines only use
// `x1,y1` and `x2,y2`.
//
// ```
// struct attached_shape {
//     attached_shape_type type; // attached_shape_line, attached_shape_triangle
//     float x1, y1, x2, y2, x3, y3;
//     SDL_FColor col;
// };
// ```
typedef enum {
    attached_shape_line,
    attached_shape_triangle,
} attached_shape_type;

typedef struct {
    attached_shape_type type;
    float x1, y1, x2, y2, x3, y3;
    SDL_FColor col;
} attached_shape;

// Its attached lines and triangles are on `shapes`, an `attached_shape` dynarr,
// in the order they were attached. `shapes_rect` is the rect bounding all of
// them in world pixels, only meaningful while it has any, and it gets expanded
// each time one is attached.
typedef struct {
    sprite sprite;
    animation anim;
    double anim_time;
    bool anim_playing;
    dynarr shapes;
    SDL_FRect shapes_rect;
} sprite_data;

// A sprite ready to be drawn, `x,y` is in world pixels so the camera is only
// applied when drawing. Its attached lines and triangles are the `shapes_len`
// ones starting at `shapes_start` on its frame's `shapes`, and `rect` bounds
// both them and the sprite, also in world pixels.
typedef struct {
    SDL_FRect rect;
    float x, y;
    Uint8 coords_x, coords_y, coords_w, coords_h;
    SDL_FlipMode flip_mode;
    SDL_FColor tint;
    Sint8 z_index;
    size_t shapes_start, shapes_len;
} sprite_draw;

// What the update function saw of the sprites, for the draw function to queue
// on the renderer.
typedef struct {
    dynarr draws;  // sprite_draw dynarr
    dynarr shapes; // attached_shape dynarr
} sprite_frame;

static pool sprites = (pool){0};

// Triple buffering: the update function builds `back` and swaps it with
// `ready`, the draw function swaps `ready` with `front` when `fresh` and draws
// `front`. Only the swaps need `frames_mutex`, each side owns its own frame.
static sprite_frame frames[SPRITES_FRAMES_TOTAL] = {0};
static Uint8 back = 0;
static Uint8 ready = 1;
static Uint8 front = 2;
static bool fresh = false;
static SDL_Mutex* frames_mutex = NULL;

static sprite_data* get_sprite(size_t id) {
    return pool_at(id, &sprites);
}



int SPRITES_new_sprite(sprite data, size_t* sprite_id) {
    if (data.coords_w == 0 || data.coords_h == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to create sprite with invalid data: { coords_w = "
                    "%d, coords_h = %d }",
                    data.coords_w,
                    data.coords_h);
        return 2;
    }
    sprite_data sd = (sprite_data){.sprite = data};
    init_dynarr_with(&sd.shapes, sizeof(attached_shape), 0);
    *sprite_id = add_to_pool(&sd, &sprites);
    return 0;
}


int SPRITES_del_sprite(size_t sprite_id) {
    if (sprite_id >= sprites.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete sprite_id=%lu but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (!get_sprite(sprite_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }
    deinit_dynarr(&get_sprite(sprite_id)->shapes);
    remove_from_pool(sprite_id, &sprites);
    return 0;
}


int SPRITES_get_sprite(size_t sprite_id, sprite* data) {
    if (sprite_id >= sprites.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to get sprite_id=%lu but this is outside the buffer range",
            sprite_id);
        return 3;
    }
    if (!get_sprite(sprite_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    *data = get_sprite(sprite_id)->sprite;
    return 0;
}


int SPRITES_edit_sprite(size_t sprite_id, sprite data) {
    if (sprite_id >= sprites.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit sprite_id=%lu but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (data.coords_w == 0 || data.coords_h == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit with invalid sprite data: { coords_w = "
                    "%d, coords_h = %d }",
                    data.coords_w,
                    data.coords_h);
        return 2;
    }
    if (!get_sprite(sprite_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    get_sprite(sprite_id)->sprite = data;
    return 0;
}



// Attaches `shape` to the sprite at `sprite_id`, expanding its `shapes_rect` by
// the first `points` points of `shape`.
static int attach_shape(size_t sprite_id, attached_shape shape, Uint8 points) {
    if (sprite_id >= sprites.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to attach shape to sprite_id=%lu but this is "
                    "outside the buffer range",
                    sprite_id);
        return 3;
    }
    if (!get_sprite(sprite_id)) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to attach shape to inexistent sprite at sprite_id=%lu",
            sprite_id);
        return 1;
    }

    sprite_data* sd = get_sprite(sprite_id);
    float xs[3] = {shape.x1, shape.x2, shape.x3};
    float ys[3] = {shape.y1, shape.y2, shape.y3};
    float left = xs[0], top = ys[0], right = xs[0], bottom = ys[0];
    for (Uint8 i = 1; i < points; i++) {
        left = SDL_min(left, xs[i]);
        top = SDL_min(top, ys[i]);
        right = SDL_max(right, xs[i]);
        bottom = SDL_max(bottom, ys[i]);
    }
    if (sd->shapes.len > 0) {
        SDL_FRect* r = &sd->shapes_rect;
        left = SDL_min(left, r->x);
        top = SDL_min(top, r->y);
        right = SDL_max(right, r->x + r->w);
        bottom = SDL_max(bottom, r->y + r->h);
    }
    sd->shapes_rect = (SDL_FRect){left, top, right - left, bottom - top};

    push_to_dynarr(&shape, &sd->shapes);
    return 0;
}

int SPRITES_attach_line_to_sprite(
    size_t sprite_id, float x1, float y1, float x2, float y2, SDL_FColor col) {
    return attach_shape(sprite_id,
                        (attached_shape){
                            .type = attached_shape_line,
                            .x1 = x1,
                            .y1 = y1,
                            .x2 = x2,
                            .y2 = y2,
                            .col = col,
                        },
                        2);
}

int SPRITES_attach_triangle_to_sprite(size_t sprite_id,
                                      float x1,
                                      float y1,
                                      float x2,
                                      float y2,
                                      float x3,
                                      float y3,
                                      SDL_FColor col) {
    return attach_shape(sprite_id,
                        (attached_shape){
                            .type = attached_shape_triangle,
                            .x1 = x1,
                            .y1 = y1,
                            .x2 = x2,
                            .y2 = y2,
                            .x3 = x3,
                            .y3 = y3,
                            .col = col,
                        },
                        3);
}

int SPRITES_clear_attached_shapes(size_t sprite_id) {
    if (sprite_id >= sprites.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to clear shapes of sprite_id=%lu but this is "
                    "outside the buffer range",
                    sprite_id);
        return 3;
    }
    if (!get_sprite(sprite_id)) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to clear shapes of inexistent sprite at sprite_id=%lu",
            sprite_id);
        return 1;
    }

    clear_dynarr(&get_sprite(sprite_id)->shapes);
    get_sprite(sprite_id)->shapes_rect = (SDL_FRect){0};
    return 0;
}



void SPRITES_start(void) {
    init_pool_with(&sprites, sizeof(sprite_data), SPRITES_STARTING_DYNARR_CAP);

    for (Uint8 i = 0; i < SPRITES_FRAMES_TOTAL; i++) {
        init_dynarr_with(
            &frames[i].draws, sizeof(sprite_draw), SPRITES_FRAME_STARTING_CAP);
        init_dynarr_with(&frames[i].shapes,
                         sizeof(attached_shape),
                         SPRITES_FRAME_STARTING_SHAPES_CAP);
    }
    back = 0;
    ready = 1;
    front = 2;
    fresh = false;
    frames_mutex = SDL_CreateMutex();
}



int SPRITES_play_anim(size_t sprite_id, animation anim) {
    if (sprite_id >= sprites.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate sprite_id=%lu but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (!get_sprite(sprite_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    get_sprite(sprite_id)->anim = anim;
    get_sprite(sprite_id)->anim_playing = true;
    get_sprite(sprite_id)->anim_time = 0.0f;
    return 0;
}

int SPRITES_stop_anim(size_t sprite_id) {
    if (sprite_id >= sprites.cap) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in sprite_id=%lu but this is outside the "
            "buffer range",
            sprite_id);
        return 3;
    }
    if (!get_sprite(sprite_id)) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in inexistent sprite at sprite_id=%lu",
            sprite_id);
        return 1;
    }

    get_sprite(sprite_id)->anim_playing = false;
    get_sprite(sprite_id)->anim_time = 0.0f;
    return 0;
}

// The rect bounding the sprite and its attached lines and triangles, in world
// pixels.
static SDL_FRect get_rect(sprite_data* sd) {
    SDL_FRect rect = (SDL_FRect){
        .x = sd->sprite.pos_x,
        .y = sd->sprite.pos_y,
        .w = SPRITESHEET_CELL_X * sd->sprite.coords_w,
        .h = SPRITESHEET_CELL_Y * sd->sprite.coords_h,
    };
    if (sd->shapes.len == 0) return rect;
    SDL_FRect l = sd->shapes_rect;
    float r = SDL_max(rect.x + rect.w, l.x + l.w);
    float b = SDL_max(rect.y + rect.h, l.y + l.h);
    rect.x = SDL_min(rect.x, l.x);
    rect.y = SDL_min(rect.y, l.y);
    rect.w = r - rect.x;
    rect.h = b - rect.y;
    return rect;
}

// Sprites are simulated when any part of their rect, which includes their
// attached lines and triangles, is within `space`, in tiles.
static bool is_simulated(SDL_FRect rect, SDL_Rect space) {
    return rect.x + rect.w > space.x * SPRITESHEET_CELL_X &&
           rect.x < (space.x + space.w) * SPRITESHEET_CELL_X &&
           rect.y + rect.h > space.y * SPRITESHEET_CELL_Y &&
           rect.y < (space.y + space.h) * SPRITESHEET_CELL_Y;
}

// Advances the sprite's animation, returning whether a non-looping one just
// ended.
static bool animate(sprite_data* sd) {
    if (!sd->anim_playing) return false;
    sd->anim_time += GAME_MANAGER_get_current_dt();
    animation anim = sd->anim;
    int i = 0;
    double time = sd->anim_time;
    while (time >= 0.0f && anim.frames[i].duration > 0.0f) {
        sd->sprite.coords_x = anim.frames[i].coords_x;
        sd->sprite.coords_y = anim.frames[i].coords_y;
        time -= anim.frames[i].duration;
        i++;
    }
    if (time < 0.0f) return false;
    if (anim.looped) {
        sd->anim_time = time;
        sd->sprite.coords_x = anim.frames[0].coords_x;
        sd->sprite.coords_y = anim.frames[0].coords_y;
        return false;
    }
    sd->anim_playing = false;
    sd->anim_time = 0.0f;
    return true;
}

// Adds the sprite to `f` if it's near the camera. The margin is because the
// camera may move until the frame gets drawn. Its rect includes its attached
// triangles, so a sprite off screen still gets added if any of them are on it.
static void
add_to_frame(sprite_frame* f, sprite_data* sd, SDL_FRect rect, SDL_FRect cam) {
    sprite s = sd->sprite;
    if (rect.x + rect.w <= cam.x - SPRITES_FRAME_CULL_MARGIN ||
        rect.y + rect.h <= cam.y - SPRITES_FRAME_CULL_MARGIN ||
        rect.x >= cam.x + cam.w + SPRITES_FRAME_CULL_MARGIN ||
        rect.y >= cam.y + cam.h + SPRITES_FRAME_CULL_MARGIN)
        return;

    size_t shapes_start = f->shapes.len;
    if (sd->shapes.len)
        SDL_memcpy(grow_dynarr(sd->shapes.len, &f->shapes),
                   sd->shapes.data,
                   sd->shapes.len * sizeof(attached_shape));

    push_to_dynarr(
        &(sprite_draw){
            .rect = rect,
            .x = s.pos_x,
            .y = s.pos_y,
            .coords_x = s.coords_x,
            .coords_y = s.coords_y,
            .coords_w = s.coords_w,
            .coords_h = s.coords_h,
            .flip_mode = s.flip_mode,
            .tint = {s.tint_r, s.tint_g, s.tint_b, s.tint_a},
            .z_index = s.z_index,
            .shapes_start = shapes_start,
            .shapes_len = sd->shapes.len,
        },
        &f->draws);
}

void SPRITES_update(void) {
    SDL_FRect cam = CAMERA_get_rect();
    SDL_Rect space = CAMERA_get_simulation_rect();
    sprite_frame* f = &frames[back];
    clear_dynarr(&f->draws);
    clear_dynarr(&f->shapes);

    // Animation finished functions are only called after looping through
    // sprites, so they're free to create, edit or delete sprites.
    dynarr finished = (dynarr){0};
    init_dynarr(&finished, sizeof(size_t));

    for (size_t id = 0; id < sprites.cap; id++) {
        sprite_data* sd = get_sprite(id);
        if (!sd) continue;
        SDL_FRect rect = get_rect(sd);
        if (is_simulated(rect, space) && animate(sd))
            push_to_dynarr(&id, &finished);
        add_to_frame(f, sd, rect, cam);
    }

    SDL_LockMutex(frames_mutex);
    Uint8 swap = back;
    back = ready;
    ready = swap;
    fresh = true;
    SDL_UnlockMutex(frames_mutex);

    // Skips sprites that got deleted or had another animation played on them
    // by previous calls.
    for (size_t i = 0; i < finished.len; i++) {
        size_t id = *(size_t*)dynarr_at(i, &finished);
        sprite_data* sd = get_sprite(id);
        if (!sd || sd->anim_playing) continue;
        sprite s = sd->sprite;
        if (s.on_anim_finished_func)
            s.on_anim_finished_func(id, s.on_anim_finished_arg);
    }
    deinit_dynarr(&finished);
}



void SPRITES_draw(void) {
    SDL_LockMutex(frames_mutex);
    if (fresh) {
        Uint8 swap = front;
        front = ready;
        ready = swap;
        fresh = false;
    }
    SDL_UnlockMutex(frames_mutex);

    SDL_FRect cam = CAMERA_get_rect();
    sprite_frame* f = &frames[front];
    for (size_t i = 0; i < f->draws.len; i++) {
        sprite_draw* d = dynarr_at(i, &f->draws);
        float x = d->x - cam.x;
        float y = d->y - cam.y;
        float rect_x = d->rect.x - cam.x;
        float rect_y = d->rect.y - cam.y;
        if (rect_x + d->rect.w <= 0.0f || rect_y + d->rect.h <= 0.0f ||
            rect_x >= cam.w || rect_y >= cam.h)
            continue;

        // Queued right before the sprite on its z_index, so nothing else can
        // be drawn between them
        for (size_t j = d->shapes_start; j < d->shapes_start + d->shapes_len;
             j++) {
            attached_shape* t = dynarr_at(j, &f->shapes);
            if (t->type == attached_shape_line)
                RENDERER_draw_line(t->x1 - cam.x,
                                   t->y1 - cam.y,
                                   t->x2 - cam.x,
                                   t->y2 - cam.y,
                                   t->col,
                                   d->z_index);
            else
                RENDERER_draw_triangle(t->x1 - cam.x,
                                       t->y1 - cam.y,
                                       t->x2 - cam.x,
                                       t->y2 - cam.y,
                                       t->x3 - cam.x,
                                       t->y3 - cam.y,
                                       t->col,
                                       d->z_index);
        }
        RENDERER_draw_from_spritesheet(x,
                                       y,
                                       d->coords_x,
                                       d->coords_y,
                                       d->coords_w,
                                       d->coords_h,
                                       d->flip_mode,
                                       d->tint,
                                       d->z_index);
    }
}


void SPRITES_stop(void) {
    for (size_t i = 0; i < sprites.cap; i++)
        if (get_sprite(i)) deinit_dynarr(&get_sprite(i)->shapes);
    deinit_pool(&sprites);

    for (Uint8 i = 0; i < SPRITES_FRAMES_TOTAL; i++) {
        deinit_dynarr(&frames[i].draws);
        deinit_dynarr(&frames[i].shapes);
    }
    SDL_DestroyMutex(frames_mutex);
    frames_mutex = NULL;
}
