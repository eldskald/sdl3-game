#include "engine/sprites.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/game-manager.h"
#include <SDL3/SDL.h>

#ifdef TEST
#include "stubs.h"
#endif

#define SPRITES_FRAMES_TOTAL 3

// This is how we're doing it: we have this huge sprites array of sprite_data to
// represent all current sprites rendering. It is just a big block of memory.
// When SPRITES_new_sprite() is called, we iterate through it to find the
// smallest unoccupied index and place the sprite there, where it shall remain
// so that its sprite_id always points to it in the array. If the array gets
// full, we double its size like a dynamic array but hopefully it shouldn't need
// to.
typedef struct {
    sprite sprite;
    animation anim;
    double anim_time;
    bool anim_playing;
} sprite_data;

// A sprite ready to be drawn, `dst` is in world pixels so the camera is only
// applied when drawing.
typedef struct {
    SDL_FRect src;
    SDL_FRect dst;
    SDL_FlipMode flip_mode;
    float tint[4];
} sprite_draw;

// What the update function saw of the sprites, in z_index order, for the draw
// function to draw.
typedef struct {
    sprite_draw* draws;
    size_t len, cap;
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



// The z_index map is our data structure to order sprite ids in order of z_index
// that we should iterate through to draw them in z_index order. Each z_index is
// mapped to z_index + SPRITES_Z_INDEX_TOTAL / 2, so they all fit in the array.
// We need to update the map whenever a new sprite is added, removed or updated.
static dynarr z_index_map[SPRITES_Z_INDEX_TOTAL] = {0};

static void add_to_z_index_map(size_t sprite_id) {
    int map_index =
        sprites[sprite_id].sprite.z_index + (SPRITES_Z_INDEX_TOTAL >> 1);
    push_to_dynarr((void*)sprite_id, &z_index_map[map_index]);
}

static void remove_from_z_index_map(size_t sprite_id) {
    int map_index =
        sprites[sprite_id].sprite.z_index + (SPRITES_Z_INDEX_TOTAL >> 1);
    remove_val_from_dynarr((void*)sprite_id, &z_index_map[map_index]);
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
            add_to_z_index_map(i);
            *sprite_id = i;
            return 0;
        }
    }
    double_dynarr();
    size_t index = sprites_size / 2;
    sprites[index] = (sprite_data){.sprite = data};
    add_to_z_index_map(index);
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
    remove_from_z_index_map(sprite_id);
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

    if (data.z_index != sprites[sprite_id].sprite.z_index) {
        remove_from_z_index_map(sprite_id);
        sprites[sprite_id].sprite = data;
        add_to_z_index_map(sprite_id);
    } else {
        sprites[sprite_id].sprite = data;
    }
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

// Sprites are simulated when any part of them is within `space`, in tiles.
static bool is_simulated(sprite s, SDL_Rect space) {
    double l = s.pos_x;
    double t = s.pos_y;
    double r = l + SPRITESHEET_CELL_X * s.coords_w;
    double b = t + SPRITESHEET_CELL_Y * s.coords_h;
    return r > space.x * SPRITESHEET_CELL_X &&
           l < (space.x + space.w) * SPRITESHEET_CELL_X &&
           b > space.y * SPRITESHEET_CELL_Y &&
           t < (space.y + space.h) * SPRITESHEET_CELL_Y;
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

// Adds the sprite to `f` if it's near the camera. The margin is because the
// camera may move until the frame gets drawn.
static void add_to_frame(sprite_frame* f, sprite s, SDL_FRect cam) {
    sprite_draw d = (sprite_draw){
        .src =
            (SDL_FRect){
                .x = SPRITESHEET_CELL_X * s.coords_x,
                .y = SPRITESHEET_CELL_Y * s.coords_y,
                .w = SPRITESHEET_CELL_X * s.coords_w,
                .h = SPRITESHEET_CELL_Y * s.coords_h,
            },
        .dst =
            (SDL_FRect){
                .x = s.pos_x,
                .y = s.pos_y,
                .w = SPRITESHEET_CELL_X * s.coords_w,
                .h = SPRITESHEET_CELL_Y * s.coords_h,
            },
        .flip_mode = s.flip_mode,
        .tint = {s.tint_r, s.tint_g, s.tint_b, s.tint_a},
    };
    if (d.dst.x + d.dst.w <= cam.x - SPRITES_FRAME_CULL_MARGIN ||
        d.dst.y + d.dst.h <= cam.y - SPRITES_FRAME_CULL_MARGIN ||
        d.dst.x >= cam.x + cam.w + SPRITES_FRAME_CULL_MARGIN ||
        d.dst.y >= cam.y + cam.h + SPRITES_FRAME_CULL_MARGIN)
        return;
    push_draw(f, d);
}

void SPRITES_update(void) {
    SDL_FRect cam = CAMERA_get_rect();
    SDL_Rect space = CAMERA_get_simulation_rect();
    sprite_frame* f = &frames[back];
    f->len = 0;

    // Animation finished functions are only called after looping through
    // sprites, so they're free to create, edit or delete sprites.
    dynarr finished = (dynarr){0};
    init_dynarr(&finished);

    for (int i = 0; i < SPRITES_Z_INDEX_TOTAL; i++) {
        for (size_t j = 0; j < z_index_map[i].len; j++) {
            size_t id = (size_t)z_index_map[i].at[j];
            sprite_data* sd = &sprites[id];
            if (is_simulated(sd->sprite, space) && animate(sd))
                push_to_dynarr((void*)id, &finished);
            add_to_frame(f, sd->sprite, cam);
        }
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



void SPRITES_draw(SDL_Renderer* renderer,
                  SDL_Texture* spritesheet,
                  SDL_GPURenderState* tint_state) {
    SDL_LockMutex(frames_mutex);
    if (fresh) {
        Uint8 swap = front;
        front = ready;
        ready = swap;
        fresh = false;
    }
    SDL_UnlockMutex(frames_mutex);

#ifndef TEST
    // Uniforms are only set when the tint changes from the last sprite's
    SDL_SetGPURenderState(renderer, tint_state);
    float last_tint[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
#endif

    SDL_FRect cam = CAMERA_get_rect();
    sprite_frame* f = &frames[front];
    for (size_t i = 0; i < f->len; i++) {
        sprite_draw* d = &f->draws[i];
        SDL_FRect dstrect = (SDL_FRect){
            .x = d->dst.x - cam.x,
            .y = d->dst.y - cam.y,
            .w = d->dst.w,
            .h = d->dst.h,
        };
        if (dstrect.x + dstrect.w <= 0.0f || dstrect.y + dstrect.h <= 0.0f ||
            dstrect.x >= cam.w || dstrect.y >= cam.h)
            continue;
#ifndef TEST
        if (SDL_memcmp(d->tint, last_tint, sizeof(last_tint)) != 0) {
            SDL_SetGPURenderStateFragmentUniforms(
                tint_state, 0, d->tint, sizeof(d->tint));
            SDL_memcpy(last_tint, d->tint, sizeof(last_tint));
        }
        SDL_FPoint center = (SDL_FPoint){.x = 0.0f, .y = 0.0f};
        SDL_RenderTextureRotated(renderer,
                                 spritesheet,
                                 &d->src,
                                 &dstrect,
                                 0.0f,
                                 &center,
                                 d->flip_mode);
#else
        stubbed_SDL_RenderTexture(&d->src, &dstrect, d->flip_mode);
#endif
    }

#ifndef TEST
    SDL_SetGPURenderState(renderer, NULL);
#endif
}


void SPRITES_stop(void) {
    for (int i = 0; i < SPRITES_Z_INDEX_TOTAL; i++) {
        clear_dynarr(&z_index_map[i]);
    }
    SDL_free(sprites);
    sprites = NULL;
    sprites_size = 0;

    for (Uint8 i = 0; i < SPRITES_FRAMES_TOTAL; i++) {
        SDL_free(frames[i].draws);
        frames[i] = (sprite_frame){0};
    }
    SDL_DestroyMutex(frames_mutex);
    frames_mutex = NULL;
}
