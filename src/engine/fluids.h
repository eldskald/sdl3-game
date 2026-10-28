#pragma once
#include "data-structs/hashmap.h"
#include "defs.h"
#include <SDL3/SDL.h>

// Fluid types, for interactions etc.
//
// ```
// enum fluid_type { FLUIDS_TYPES };
// ```
typedef enum { FLUIDS_TYPES } fluid_type;

// Struct containing containing data about a fluid cell.
//
// struct fluid_cell {
//     double volume[FLUIDS_TOTAL];
//     bool solid;
// }
typedef struct {
    double volume[FLUIDS_TOTAL];
    bool solid;
} fluid_cell;

// Textures and render states the fluids are drawn with, owned by `renderer.c`.
// `texture` is what the fluid cells are drawn into and `render_states[i]` draws
// it with the fluids shader, sampling `ripples[i]` for the ripples. Ripples are
// ticked by drawing one of `ripples` into the other with `ripples_state`, which
// samples `texture` to keep them inside the fluids.
//
// ```
// struct fluids_canvas {
//     SDL_Texture* texture;
//     SDL_Texture* ripples[2];
//     SDL_GPURenderState* ripples_state;
//     SDL_GPURenderState* render_states[2];
// };
// ```
typedef struct {
    SDL_Texture* texture;
    SDL_Texture* ripples[2];
    SDL_GPURenderState* ripples_state;
    SDL_GPURenderState* render_states[2];
} fluids_canvas;



// Loads fluids data and allocates initial memory. Call at the start of the
// game.
void FLUIDS_start(void);

// Frees allocated memory at `FLUIDS_start()`. Call when quitting the game.
void FLUIDS_stop(void);



// Sets the fluid cells at rect `(x,y,w,h)` in fluid cell coords, all of type
// `type` and and volume `volume` on each cell. Applied right away when called
// from the fluids thread, otherwise queued and applied on the next step.
void FLUIDS_set_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type, double volume);

// Clear all fluids of type `type` on rect `(x,y,w,h)`. Applied right away when
// called from the fluids thread, otherwise queued and applied on the next step.
void FLUIDS_clear_at(Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type);

// Sets `cells` to be the rect `(x,y,w,h)`. Cells outside range are treated as
// solids.
void FLUIDS_get_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_cell (*cells)[w][h]);

// Queues a splash on the `w` columns from `(x,y)` to its right, applied on the
// next step, as if something hit them from above at `speed`. Each column takes
// up to `volume` of fluid from its surface cell, topping up from the cell under
// it, and throws it up as particles from just above it, in its slice of a fan
// split evenly among the columns, left to right. Both the volume and the
// particles' speed scale with `speed`, up to `body_max_speed`. Those and the
// rest of the splash's settings are on `FLUIDS_PARTICLES_CSV_PATH`. Volume that
// doesn't make a whole particle is put back. A column with no surface within
// `FLUIDS_SPLASH_SURFACE_RANGE` cells of `y`, or with a wall over its surface,
// is skipped.
void FLUIDS_splash_at(Uint32 x, Uint32 y, Uint32 w, double speed);

// Queues a ripple on the `size` square of pixels centered on pixel `(x,y)`,
// applied on the next ripples tick. It adds `strength` to their height, on top
// of any ripples already there, then spreads around the body of fluid it's in
// and fades out over time.
void FLUIDS_ripple_at(Uint32 x, Uint32 y, Uint32 size, double strength);

// Adds a particle of fluid of type `type` at `(x,y)` in world coords, moving
// with velocity `(vel_x,vel_y)`. It becomes fluid once it lands on a fluid cell
// or hits a wall. Returns `0` when successful, `1` when `(x,y)` is inside a
// wall and `2` when it is outside the simulated area. No particle is added on
// errors.
int FLUIDS_add_particle(
    fluid_type type, double x, double y, double vel_x, double vel_y);



// Ticks the simulation on the simulated rect, after centering it on the camera.
// Ripples are ticked as many times as this was called on the next
// `FLUIDS_draw()`, since they're simulated on the GPU. Call on update in any
// thread.
void FLUIDS_update(void);

// Draws the fluid cells around the screen into `canvas->texture`, ticks the
// ripples as many times as `FLUIDS_update()` was called since the last draw, up
// to `FLUIDS_RIPPLES_MAX_TICKS`, then draws the part of it on screen on the
// current render target with the fluids shader. `canvas->texture` and
// `canvas->ripples` must be `FLUIDS_RIPPLES_TEXTURE_RES_FACTOR` times the
// screen's size, covering that much of the world centered on
// the camera. Everything is drawn relative to the camera. Call on renderer
// update.
void FLUIDS_draw(SDL_Renderer* renderer, fluids_canvas* canvas);

// Gets the rect being simulated, in fluid cell coords. It's the simulation
// space from `CAMERA_get_simulation_rect()`, updated on each `FLUIDS_update()`.
// Cells outside it are treated as walls by the simulation.
SDL_Rect FLUIDS_get_simulated_rect(void);



// Get a `string-->fluid_type` map, used to translate the fluid names written on
// CSV files into their corresponding `fluid_type`. Needed mostly for reactions.
hashmap FLUIDS_get_string_fluid_types_map(void);
