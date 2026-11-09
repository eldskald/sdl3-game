#pragma once
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/graphics.h"
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

// Coords of a fluid cell on the grid.
//
// ```
// struct fluid_coords {
//     Uint32 x, y;
// };
// ```
typedef struct {
    Uint32 x, y;
} fluid_coords;

// A body of fluid of `type`, the cells with it on the simulated rect connected
// to each other by their sides. `cells` is a `fluid_coords` dynarr of them,
// `total_volume` the volume of `type` on all of them and `rect` the smallest
// rect around them, in fluid cell coords. `moving_cells` counts the cells
// moving down or diagonally down on their next moves, plus the ones over each
// cell moving down, since they fall along with it, so some cells are counted
// twice.
//
// ```
// struct fluid_body {
//     fluid_type type;
//     dynarr cells;
//     double total_volume;
//     SDL_Rect rect;
//     Uint16 moving_cells;
// };
// ```
typedef struct {
    fluid_type type;
    dynarr cells;
    double total_volume;
    SDL_Rect rect;
    Uint16 moving_cells;
} fluid_body;

// Textures and shaders the fluids are drawn with, owned by `renderer.c`.
// `texture` is what the fluid cells are drawn into, and it's drawn with
// `shader`, sampling the latest of `ripples` for the ripples and `pre_fluids`
// for everything drawn before the fluids. Ripples are ticked by drawing one of
// `ripples` into the other with `ripples_shader`, which samples `texture` to
// keep them inside the fluids.
//
// ```
// struct fluids_canvas {
//     graphics_texture* texture;
//     graphics_texture* ripples[2];
//     graphics_texture* pre_fluids;
//     SDL_GPUShader* shader;
//     SDL_GPUShader* ripples_shader;
// };
// ```
typedef struct {
    graphics_texture* texture;
    graphics_texture* ripples[2];
    graphics_texture* pre_fluids;
    SDL_GPUShader* shader;
    SDL_GPUShader* ripples_shader;
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
// solids. Thread safe.
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
// is skipped. If any column has at least a tile tall body of fluid within that
// range, `FLUIDS_SPLIT_Y` cells in a row with
// `FLUIDS_INTERACTION_VOL_THRESHOLD` volume or more each, it also plays a
// splash sound of the fluid with the most volume there, unless it's within
// `FLUIDS_SPLASH_SFX_COOLDOWN` seconds of the last one.
void FLUIDS_splash_at(Uint32 x, Uint32 y, Uint32 w, double speed);

// Queues a ripple on the `size` square of pixels centered on pixel `(x,y)`,
// applied on the next ripples tick. It adds `strength` to their height, on top
// of any ripples already there, then spreads around the body of fluid it's in
// and fades out over time. If the fluid cell on `(x,y)` and the one under it
// have `FLUIDS_RIPPLE_SFX_VOL_THRESHOLD` volume or more together, it also plays
// a slosh sound of the fluid with the most volume under the ripple, as heard
// from `(x,y)` and with `strength` as its max volume, unless it's within
// `FLUIDS_RIPPLE_SFX_COOLDOWN` seconds of the last one.
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
// `FLUIDS_prepare()`, since they're simulated on the GPU. Call on update in any
// thread.
void FLUIDS_update(void);

// Draws the fluid cells around the screen into `canvas->texture` and ticks the
// ripples as many times as `FLUIDS_update()` was called since the last time,
// up to `FLUIDS_RIPPLES_MAX_TICKS`. `canvas->texture` and `canvas->ripples`
// must be `FLUIDS_RIPPLES_TEXTURE_RES_FACTOR` times the screen's size, covering
// that much of the world centered on the camera. It leaves the render target on
// `canvas->texture`, so call it before setting the target `FLUIDS_draw()` draws
// on. Call on renderer update.
void FLUIDS_prepare(fluids_canvas* canvas);

// Draws the part of `canvas->texture` on screen, as `FLUIDS_prepare()` left
// it, on the whole current render target with the fluids shader, covering
// everything on it, then the fluid particles over it. Everything is drawn
// relative to the camera. Call on renderer update, after `FLUIDS_prepare()`.
void FLUIDS_draw(fluids_canvas* canvas);

// Writes on `codes` what's on each fluid cell of the simulated rect, row by row
// from its top left, so cell `(x,y)` of it is `codes[y * rect.w + x]`, and
// returns the rect, in fluid cell coords. Codes are the same as the red channel
// of the fluids' texture, see `draw_grid()` on `fluids.c`: `0` is air, walls
// are the red of `FLUIDS_WALL_COLOR` and fluids are
// `FLUIDS_SHADER_IDX_STEP * (type + 1)`. A cell is a fluid when it has at least
// `FLUIDS_INTERACTION_VOL_THRESHOLD` of them in total, coded as the densest of
// them there. `codes` must fit the whole rect. Call it in any thread.
SDL_Rect FLUIDS_get_codes(Uint8* codes);

// Gets the rect being simulated, in fluid cell coords. It's the simulation
// space from `CAMERA_get_simulation_rect()`, updated on each `FLUIDS_update()`.
// Cells outside it are treated as walls by the simulation.
SDL_Rect FLUIDS_get_simulated_rect(void);

// Plays the BGS tracks of large bodies of fluid around the camera, louder the
// closer they are to it. Call it from one thread only, the fixed one for now.
void FLUIDS_play_bgs(void);



// Get a `string-->fluid_type` map, used to translate the fluid names written on
// CSV files into their corresponding `fluid_type`. Needed mostly for reactions.
hashmap FLUIDS_get_string_fluid_types_map(void);
