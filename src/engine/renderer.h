#pragma once
#include <SDL3/SDL.h>

// Loads textures, shaders, etc. Returns 0 when successful, 1 otherwise.
int RENDERER_start(void);

// Unloads textures, shaders, etc. Call when quitting the game.
void RENDERER_stop(void);

// Render step of game loop. Must happen on main thread.
void RENDERER_update(void);



// Draw calls aren't drawn right away, they're queued and drawn on
// `RENDERER_update()` in `z_index` order, higher ones on top of lower ones.
// Calls on the same `z_index` are drawn in the order they were queued. Calls
// with a `z_index` lower than `FLUIDS_Z_INDEX` are drawn under the fluids,
// which refract them, and the rest are drawn over them. Positions are in screen
// pixels, so the camera must already be applied. Call them on the `..._draw()`
// functions called by the renderer, on the main thread.
// Queues drawing the `coords_w,coords_h` cells of the spritesheet starting at
// cell `coords_x,coords_y` at `(x,y)`, flipped by `flip_mode`. Its color is
// multiplied by `tint`, which should be `1.0` to draw it as is and can go
// beyond `1.0` to make it bloom.
void RENDERER_draw_from_spritesheet(float x,
                                    float y,
                                    Uint8 coords_x,
                                    Uint8 coords_y,
                                    Uint8 coords_w,
                                    Uint8 coords_h,
                                    SDL_FlipMode flip_mode,
                                    SDL_FColor tint,
                                    Sint8 z_index);

// Queues filling the triangle `(x1,y1),(x2,y2),(x3,y3)` with `col`, blended by
// its alpha. Its color can go beyond `1.0` to make it bloom.
void RENDERER_draw_triangle(float x1,
                            float y1,
                            float x2,
                            float y2,
                            float x3,
                            float y3,
                            SDL_FColor col,
                            Sint8 z_index);

// Queues drawing a line from `(x1,y1)` to `(x2,y2)` with color `col`, blended
// by its alpha. When both ends are the same, it's drawn as a single point.
void RENDERER_draw_line(
    float x1, float y1, float x2, float y2, SDL_FColor col, Sint8 z_index);
