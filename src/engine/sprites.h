#pragma once
#include "defs.h"
#include <SDL3/SDL.h>

// Animation keyframes. Leaves the sprite at `coords_x,coords_y` for `duration`
// seconds. A duration of `0.0f` means no frame, so leave the last frames in the
// `animation.frames` zeroed out if you don't need all the frames.
//
// ```
// struct keyframe {
//     Uint8 coords_x, coords_y;
//     double duration;
// };
// ```
typedef struct {
    Uint8 coords_x, coords_y;
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
// `0,1`, `1,0` and `1,1`. Its `z_index` is the renderer's, see
// `RENDERER_draw_from_spritesheet()`.
// Its color is multiplied by `tint_r,tint_g,tint_b,tint_a`, which should be
// `1.0` to draw it as is and can go beyond `1.0` to make it bloom. Always set
// them, since they'd be zero, making the sprite invisible.
//
// When a non-looping animation ends on its own, `on_anim_finished_func` is
// called with the sprite's ID and `on_anim_finished_arg`, so whoever owns the
// sprite can deal with it. It's not called when the animation gets interrupted
// by `SPRITES_stop_anim()` or `SPRITES_play_anim()` and can be `NULL`. It runs
// on `SPRITES_update()` after every sprite was updated, so it's free to create,
// edit or delete sprites.
//
// ```
// struct sprite {
//     Sint32 pos_x, pos_y;
//     Uint8 coords_x, coords_y, coords_w, coords_h;
//     Sint8 z_index;
//     SDL_FlipMode flip_mode;
//     double tint_r, double tint_g, double tint_b, double tint_a;
//     void (*on_anim_finished_func)(size_t sprite_id, size_t arg);
//     size_t on_anim_finished_arg;
// };
// ```
typedef struct {
    Uint32 pos_x, pos_y;
    Uint8 coords_x, coords_y, coords_w, coords_h;
    Sint8 z_index;
    SDL_FlipMode flip_mode;
    double tint_r, tint_g, tint_b, tint_a;
    void (*on_anim_finished_func)(size_t sprite_id, size_t arg);
    size_t on_anim_finished_arg;
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

// Edits sprite data on `sprite_id`. Returns `0` if everything went fine, `1`
// if `sprite_id` pointed to an empty slot, or `2` if invalid sprite data
// (coords w and h must be `0` or more), or `3` if `sprite_id` is outsede the
// buffer's range.
int SPRITES_edit_sprite(size_t sprite_id, sprite data);



// Attached lines and triangles are drawn in the order they were attached, all
// right before the sprite on its z_index, so nothing else is drawn between
// them, but only while the sprite itself is on screen. Coords are in world
// pixels and colors are sRGB from `0.0` to `1.0`, which can go beyond `1.0` to
// make them bloom.

// Attaches the line from `(x1,y1)` to `(x2,y2)` with `col` to the
// sprite at `sprite_id`, drawn pixel by pixel with Bresenham's line algorithm,
// both ends included. Returns `0` if everything went fine, `1` if `sprite_id`
// pointed to an empty slot or `3` if `sprite_id` is outside the buffer's range.
int SPRITES_attach_line_to_sprite(
    size_t sprite_id, float x1, float y1, float x2, float y2, SDL_FColor col);

// Attaches the triangle `(x1,y1),(x2,y2),(x3,y3)` filled with `col` to the
// sprite at `sprite_id`. Only pixels whose centers are inside it are
// drawn, so a pixel's center is half a pixel past its integer coords. Returns
// `0` if everything went fine, `1` if `sprite_id` pointed to an empty slot or
// `3` if `sprite_id` is outside the buffer's range.
int SPRITES_attach_triangle_to_sprite(size_t sprite_id,
                                      float x1,
                                      float y1,
                                      float x2,
                                      float y2,
                                      float x3,
                                      float y3,
                                      SDL_FColor col);

// Removes all lines and triangles attached to the sprite at `sprite_id`,
// keeping their memory allocated so attaching them again is quick. Returns `0`
// if everything went fine, `1` if `sprite_id` pointed to an empty slot or `3`
// if `sprite_id` is outside the buffer's range.
int SPRITES_clear_attached_shapes(size_t sprite_id);



// Start playing animation on the sprite on the given id. Returns `0` when
// successful, `3` if `sprite_id` is outside the sprite buffer's range.
int SPRITES_play_anim(size_t sprite_id, animation anim);

// Stop current animation on the sprite on the given id. Returns `0` when
// successful, `3` if `sprite_id` is outside the sprite buffer's range, `1` if
// nothing is on `sprite_id`.
int SPRITES_stop_anim(size_t sprite_id);



// Allocates memory and other stuff. Call on start of the game.
void SPRITES_start(void);

// Frees allocated memory. Call when quitting the game.
void SPRITES_stop(void);

// Advances the animations of sprites within the simulation space from
// `CAMERA_get_simulation_rect()`, calling `on_anim_finished_func` of the ones
// that ended, and readies the sprites near the camera to be drawn by
// `SPRITES_draw()`. Call on update in the fixed thread.
void SPRITES_update(void);

// Queues the sprites readied by the last `SPRITES_update()` on the renderer at
// their z_index. Call on renderer update.
void SPRITES_draw(void);
