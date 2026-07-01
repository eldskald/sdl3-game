#pragma once
#include <SDL3/SDL.h>
#include <defs.h>

// Animation keyframes. Leaves the sprite at `coords_x,coords_y` for `duration`
// seconds. A duration of `0.0f` means no frame, so leave the last frames in the
// `animation.frames` zeroed out if you don't need all the frames.
//
// ```
// struct keyframe {
//     Uint8 coords_x;
//     Uint8 coords_y;
//     double duration;
// };
// ```
typedef struct {
    Uint8 coords_x;
    Uint8 coords_y;
    double duration;
} keyframe;

// An animation. Leaves the sprite at `frame[0]`'s `coords_x` `coords_y` for its
// duration, then moves on to `frames[1]` and so on. The moment it find a frame
// with duration `0.0f`, the animation ends. Always init frames with zeroes.
//
// ```
// struct animation {
//     bool looped;
//     keyframe frames[SPRITES_ANIM_MAX_KEYFRAMES];
// };
// ```
typedef struct {
    bool looped;
    keyframe frames[SPRITES_ANIM_MAX_KEYFRAMES];
} animation;

// A sprite from the the spritesheet. The `pos_x` `pos_y` mean its position on
// the screen in pixels, its `coords_x` `coords_y` mean its coordinates in the
// spritesheet in tiles, or cells, so `0,0` means the first tile, `1,0` means
// the one to its right, and so on. Its `coords_w,coords_h` means the size of
// the sprite, also in tile/cell size, so `0,0,2,2` means tiles coords `0,0`,
// `0,1`, `1,0` and `1,1`. Finally, higher z_index are rendered on top of lower
// ones.
//
// ```
// struct sprite {
//     Sint32 pos_x;
//     Sint32 pos_y;
//     Uint8 coords_x;
//     Uint8 coords_y;
//     Uint8 coords_w;
//     Uint8 coords_h;
//     Sint8 z_index;
// };
// ```
typedef struct {
    Uint32 pos_x;
    Uint32 pos_y;
    Uint8 coords_x;
    Uint8 coords_y;
    Uint8 coords_w;
    Uint8 coords_h;
    Sint8 z_index;
    SDL_FlipMode flip_mode;
} sprite;



// Adds sprite data to the rendering pipeline. Fills `sprite_id` with its id if
// successful, returning `0`. Otherwise, returns is `2` if data is invalid
// (coords w and h must be `0` or more).
int SPRITES_new_sprite(sprite data, size_t* sprite_id);

// Removes sprite from the rendering pipeline. Returns `0` if everything went
// fine, `1` if sprite_id pointed to an empty slot or `3` if `sprite_id` outside
// the buffer's range.
int SPRITES_del_sprite(size_t sprite_id);

// Fills data with the data from the sprite in `sprite_id`, returning `0` if
// successful. Otherwise, returns `1` if nothing is on that sprite_id or `3` if
// `sprite_id` is outside the buffer's range.
int SPRITES_get_sprite(size_t sprite_id, sprite* data);

// Updates sprite data on `sprite_id`. Returns `0` if everything went fine, `1`
// if `sprite_id` pointed to an empty slot, or `2` if invalid sprite data
// (coords w and h must be `0` or more), or `3` if `sprite_id` is outsede the
// buffer's range.
int SPRITES_update_sprite(size_t sprite_id, sprite data);



// Start playing animation on the sprite on the given id. Returns `0` when
// successful, `3` if `sprite_id` is outside the sprite buffer's range.
int SPRITES_play_anim(size_t sprite_id, animation anim);

// Stop current animation on the sprite on the given id. Returns `0` when
// successful, `3` if `sprite_id` is outside the sprite buffer's range, `1` if
// nothing is on `sprite_id`.
int SPRITES_stop_anim(size_t sprite_id);

// Makes finished true the exact frame the animation (non-looping) ended.
// Returns `0` shen successful, `3` if sprite_id is outside the sprite buffer's
// range, `1` if nothing is on `sprite_id`.
int SPRITES_anim_finished(size_t sprite_id, bool* finished);



// Allocates memory and other stuff. Call on start of the game.
void SPRITES_start(void);

// Frees allocated memory. Call when quitting the game.
void SPRITES_stop(void);

// Updates the sprites state and renders them. Call on renderer update.
void SPRITES_update(SDL_Renderer* renderer, SDL_Texture* spritesheet);
