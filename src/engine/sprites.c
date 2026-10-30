#include "engine/sprites.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/game-manager.h"
#include "engine/renderer.h"
#include <SDL3/SDL.h>

#define SPRITES_FRAMES_TOTAL 3

// This is how we're doing it: we have this huge sprites array of sprite_data to
// represent all current sprites rendering. It is just a big block of memory.
// When SPRITES_new_sprite() is called, we iterate through it to find the
// smallest unoccupied index and place the sprite there, where it shall remain
// so that its sprite_id always points to it in the array. If the array gets
// full, we double its size like a dynamic array but hopefully it shouldn't need
// to.
//
// Its attached lines are split across three dynarrs, since each slot only fits
// 8 bytes: `lines_from` has each line's `x1,y1` packed together, `lines_to` its
// `x2,y2` and `lines_col` its color. `lines_rect` is the rect bounding all its
// attached lines in world pixels, only meaningful while it has any, and it gets
// expanded each time a line is attached.
typedef struct {
    sprite sprite;
    animation anim;
    double anim_time;
    bool anim_playing;
    dynarr lines_from;
    dynarr lines_to;
    dynarr lines_col;
    SDL_FRect lines_rect;
} sprite_data;

// A line attached to a sprite, in world pixels.
typedef struct {
    float x1, y1, x2, y2;
    SDL_Color col;
} attached_line;

// A sprite ready to be drawn, `x,y` is in world pixels so the camera is only
// applied when drawing. Its attached lines are the `lines_len` ones starting at
// `lines_start` on its frame's `lines`, and `rect` bounds both them and the
// sprite, also in world pixels.
typedef struct {
    SDL_FRect rect;
    float x, y;
    Uint8 coords_x, coords_y, coords_w, coords_h;
    SDL_FlipMode flip_mode;
    SDL_FColor tint;
    Sint8 z_index;
    size_t lines_start, lines_len;
} sprite_draw;

// What the update function saw of the sprites, for the draw function to queue
// on the renderer.
typedef struct {
    sprite_draw* draws;
    size_t len, cap;
    attached_line* lines;
    size_t lines_len, lines_cap;
} sprite_frame;

static sprite_data* sprites = NULL;
static size_t sprites_size = 0;

// Triple buffering: the update function builds `back` and swaps it with
// `ready`, the draw function swaps `ready` with `front` when `fresh` and draws
// `front`. Only the swaps need `frames_mutex`, each side owns its own frame.
static sprite_frame frames[SPRITES_FRAMES_TOTAL] = {0};
static Uint8 back = 0;
static Uint8 ready = 1;
static Uint8 front = 2;
static bool fresh = false;
static SDL_Mutex* frames_mutex = NULL;

static void double_dynarr(void) {
    sprite_data* new_sprites =
        SDL_calloc(sprites_size << 1, sizeof(sprite_data));
    SDL_memcpy(new_sprites, sprites, sprites_size * sizeof(sprite_data));
    SDL_free(sprites);
    sprites_size = sprites_size << 1;
    sprites = new_sprites;
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
    for (size_t i = 0; i < sprites_size; i++) {
        if (sprites[i].sprite.coords_w == 0) {
            sprites[i] = (sprite_data){.sprite = data};
            *sprite_id = i;
            return 0;
        }
    }
    double_dynarr();
    size_t index = sprites_size / 2;
    sprites[index] = (sprite_data){.sprite = data};
    *sprite_id = index;
    return 0;
}


int SPRITES_del_sprite(size_t sprite_id) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete sprite_id=%lu but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }
    clear_dynarr(&sprites[sprite_id].lines_from);
    clear_dynarr(&sprites[sprite_id].lines_to);
    clear_dynarr(&sprites[sprite_id].lines_col);
    sprites[sprite_id] = (sprite_data){0};
    return 0;
}


int SPRITES_get_sprite(size_t sprite_id, sprite* data) {
    if (sprite_id >= sprites_size) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to get sprite_id=%lu but this is outside the buffer range",
            sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    *data = sprites[sprite_id].sprite;
    return 0;
}


int SPRITES_edit_sprite(size_t sprite_id, sprite data) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
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
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to edit inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    sprites[sprite_id].sprite = data;
    return 0;
}



static void* pack_floats(float a, float b) {
    float pair[2] = {a, b};
    void* packed = NULL;
    SDL_memcpy((void*)&packed, pair, sizeof(pair));
    return packed;
}

static void unpack_floats(void* packed, float* a, float* b) {
    float pair[2] = {0};
    SDL_memcpy(pair, (void*)&packed, sizeof(pair));
    *a = pair[0];
    *b = pair[1];
}

int SPRITES_attach_line_to_sprite(
    size_t sprite_id, float x1, float y1, float x2, float y2, SDL_Color col) {
    if (sprite_id >= sprites_size) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to attach line to sprite_id=%lu but this is outside "
                    "the buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to attach line to inexistent sprite at sprite_id=%lu",
            sprite_id);
        return 1;
    }

    sprite_data* sd = &sprites[sprite_id];
    SDL_FRect line = (SDL_FRect){
        .x = SDL_min(x1, x2),
        .y = SDL_min(y1, y2),
        .w = SDL_fabsf(x2 - x1),
        .h = SDL_fabsf(y2 - y1),
    };
    if (sd->lines_col.len == 0) {
        sd->lines_rect = line;
    } else {
        float r = SDL_max(sd->lines_rect.x + sd->lines_rect.w, line.x + line.w);
        float b = SDL_max(sd->lines_rect.y + sd->lines_rect.h, line.y + line.h);
        sd->lines_rect.x = SDL_min(sd->lines_rect.x, line.x);
        sd->lines_rect.y = SDL_min(sd->lines_rect.y, line.y);
        sd->lines_rect.w = r - sd->lines_rect.x;
        sd->lines_rect.h = b - sd->lines_rect.y;
    }

    void* packed_col = NULL;
    SDL_memcpy((void*)&packed_col, &col, sizeof(col));
    push_to_dynarr(pack_floats(x1, y1), &sprites[sprite_id].lines_from);
    push_to_dynarr(pack_floats(x2, y2), &sprites[sprite_id].lines_to);
    push_to_dynarr(packed_col, &sprites[sprite_id].lines_col);
    return 0;
}

int SPRITES_clear_attached_lines(size_t sprite_id) {
    if (sprite_id >= sprites_size) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to clear lines of sprite_id=%lu but this is outside "
                    "the buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to clear lines of inexistent sprite at sprite_id=%lu",
            sprite_id);
        return 1;
    }

    sprites[sprite_id].lines_from.len = 0;
    sprites[sprite_id].lines_to.len = 0;
    sprites[sprite_id].lines_col.len = 0;
    sprites[sprite_id].lines_rect = (SDL_FRect){0};
    return 0;
}



void SPRITES_start(void) {
    sprites = SDL_calloc(SPRITES_STARTING_DYNARR_CAP, sizeof(sprite_data));
    sprites_size = SPRITES_STARTING_DYNARR_CAP;

    for (Uint8 i = 0; i < SPRITES_FRAMES_TOTAL; i++) {
        frames[i] = (sprite_frame){
            .draws =
                SDL_calloc(SPRITES_FRAME_STARTING_CAP, sizeof(sprite_draw)),
            .cap = SPRITES_FRAME_STARTING_CAP,
            .lines = SDL_calloc(SPRITES_FRAME_STARTING_LINES_CAP,
                                sizeof(attached_line)),
            .lines_cap = SPRITES_FRAME_STARTING_LINES_CAP,
        };
    }
    back = 0;
    ready = 1;
    front = 2;
    fresh = false;
    frames_mutex = SDL_CreateMutex();
}



int SPRITES_play_anim(size_t sprite_id, animation anim) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate sprite_id=%lu but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    sprites[sprite_id].anim = anim;
    sprites[sprite_id].anim_playing = true;
    sprites[sprite_id].anim_time = 0.0f;
    return 0;
}

int SPRITES_stop_anim(size_t sprite_id) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in sprite_id=%lu but this is outside the "
            "buffer range",
            sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in inexistent sprite at sprite_id=%lu",
            sprite_id);
        return 1;
    }

    sprites[sprite_id].anim_playing = false;
    sprites[sprite_id].anim_time = 0.0f;
    return 0;
}

// The rect bounding the sprite and its attached lines, in world pixels.
static SDL_FRect get_rect(sprite_data* sd) {
    SDL_FRect rect = (SDL_FRect){
        .x = sd->sprite.pos_x,
        .y = sd->sprite.pos_y,
        .w = SPRITESHEET_CELL_X * sd->sprite.coords_w,
        .h = SPRITESHEET_CELL_Y * sd->sprite.coords_h,
    };
    if (sd->lines_col.len == 0) return rect;
    SDL_FRect l = sd->lines_rect;
    float r = SDL_max(rect.x + rect.w, l.x + l.w);
    float b = SDL_max(rect.y + rect.h, l.y + l.h);
    rect.x = SDL_min(rect.x, l.x);
    rect.y = SDL_min(rect.y, l.y);
    rect.w = r - rect.x;
    rect.h = b - rect.y;
    return rect;
}

// Sprites are simulated when any part of their rect, which includes their
// attached lines, is within `space`, in tiles.
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

static void push_draw(sprite_frame* f, sprite_draw d) {
    if (f->len == f->cap) {
        f->cap = f->cap << 1;
        f->draws = SDL_realloc(f->draws, f->cap * sizeof(sprite_draw));
    }
    f->draws[f->len] = d;
    f->len++;
}

static void push_line(sprite_frame* f, attached_line l) {
    if (f->lines_len == f->lines_cap) {
        f->lines_cap = f->lines_cap << 1;
        f->lines = SDL_realloc(f->lines, f->lines_cap * sizeof(attached_line));
    }
    f->lines[f->lines_len] = l;
    f->lines_len++;
}

// Adds the sprite to `f` if it's near the camera. The margin is because the
// camera may move until the frame gets drawn. Its rect includes its attached
// lines, so a sprite off screen still gets added if any of them are on it.
static void
add_to_frame(sprite_frame* f, sprite_data* sd, SDL_FRect rect, SDL_FRect cam) {
    sprite s = sd->sprite;
    if (rect.x + rect.w <= cam.x - SPRITES_FRAME_CULL_MARGIN ||
        rect.y + rect.h <= cam.y - SPRITES_FRAME_CULL_MARGIN ||
        rect.x >= cam.x + cam.w + SPRITES_FRAME_CULL_MARGIN ||
        rect.y >= cam.y + cam.h + SPRITES_FRAME_CULL_MARGIN)
        return;

    size_t lines_start = f->lines_len;
    for (size_t i = 0; i < sd->lines_col.len; i++) {
        attached_line l = (attached_line){0};
        unpack_floats(sd->lines_from.at[i], &l.x1, &l.y1);
        unpack_floats(sd->lines_to.at[i], &l.x2, &l.y2);
        SDL_memcpy(&l.col, (void*)&sd->lines_col.at[i], sizeof(l.col));
        push_line(f, l);
    }

    push_draw(f,
              (sprite_draw){
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
                  .lines_start = lines_start,
                  .lines_len = sd->lines_col.len,
              });
}

void SPRITES_update(void) {
    SDL_FRect cam = CAMERA_get_rect();
    SDL_Rect space = CAMERA_get_simulation_rect();
    sprite_frame* f = &frames[back];
    f->len = 0;
    f->lines_len = 0;

    // Animation finished functions are only called after looping through
    // sprites, so they're free to create, edit or delete sprites.
    dynarr finished = (dynarr){0};
    init_dynarr(&finished);

    for (size_t id = 0; id < sprites_size; id++) {
        sprite_data* sd = &sprites[id];
        if (sd->sprite.coords_w == 0) continue;
        SDL_FRect rect = get_rect(sd);
        if (is_simulated(rect, space) && animate(sd))
            push_to_dynarr((void*)id, &finished);
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
        size_t id = (size_t)finished.at[i];
        sprite s = sprites[id].sprite;
        if (s.coords_w == 0 || sprites[id].anim_playing) continue;
        if (s.on_anim_finished_func)
            s.on_anim_finished_func(id, s.on_anim_finished_arg);
    }
    clear_dynarr(&finished);
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
    for (size_t i = 0; i < f->len; i++) {
        sprite_draw* d = &f->draws[i];
        float x = d->x - cam.x;
        float y = d->y - cam.y;
        float rect_x = d->rect.x - cam.x;
        float rect_y = d->rect.y - cam.y;
        if (rect_x + d->rect.w <= 0.0f || rect_y + d->rect.h <= 0.0f ||
            rect_x >= cam.w || rect_y >= cam.h)
            continue;

        // Queued right before the sprite on its z_index, so nothing else can
        // be drawn between them
        for (size_t j = d->lines_start; j < d->lines_start + d->lines_len;
             j++) {
            attached_line* l = &f->lines[j];
            RENDERER_draw_line(l->x1 - cam.x,
                               l->y1 - cam.y,
                               l->x2 - cam.x,
                               l->y2 - cam.y,
                               l->col,
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
    for (size_t i = 0; i < sprites_size; i++) {
        clear_dynarr(&sprites[i].lines_from);
        clear_dynarr(&sprites[i].lines_to);
        clear_dynarr(&sprites[i].lines_col);
    }
    SDL_free(sprites);
    sprites = NULL;
    sprites_size = 0;

    for (Uint8 i = 0; i < SPRITES_FRAMES_TOTAL; i++) {
        SDL_free(frames[i].draws);
        SDL_free(frames[i].lines);
        frames[i] = (sprite_frame){0};
    }
    SDL_DestroyMutex(frames_mutex);
    frames_mutex = NULL;
}
