#pragma once
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



// Loads fluids data and allocates initial memory. Call at the start of the
// game.
void FLUIDS_start(void);

// Frees allocated memory at `FLUIDS_start()`. Call when quitting the game.
void FLUIDS_stop(void);



// Sets the fluid cells at rect `(x,y,w,h)` in fluid cell coords, all of type
// `type` and and volume `volume` on each cell.
void FLUIDS_set_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type, double volume);

// Clear all fluids on rect `(x,y,w,h)`.
void FLUIDS_clear_at(Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_type type);

// Sets `cells` to be the rect `(x,y,w,h)`. Cells outside range are treated as
// solids.
void FLUIDS_get_at(
    Uint32 x, Uint32 y, Uint32 w, Uint32 h, fluid_cell (*cells)[w][h]);

// Queues a splash at `(x,y)`, applied on the next step. It takes up to 1 of
// fluid from the surface cells of the columns to its left and right, topping
// up from the cells under them, and throws it up as particles in a fan from
// just above those cells. Volume that doesn't make a whole particle is put back
// on them. A column with no surface within `FLUID_SPLASH_SURFACE_RANGE` cells
// of `y`, or with a wall over its surface, is skipped, and if both are, there
// is no splash.
void FLUIDS_splash_at(Uint32 x, Uint32 y);

// Adds a particle of fluid of type `type` at `(x,y)` in world coords, moving
// with velocity `(vel_x,vel_y)`. It becomes fluid once it lands on a fluid cell
// or hits a wall. Returns `0` when successful, `1` when `(x,y)` is inside a
// wall and `2` when it is outside the simulated area. No particle is added on
// errors.
int FLUIDS_add_particle(
    fluid_type type, double x, double y, double vel_x, double vel_y);



// Ticks the simulation. Call on update in any thread.
void FLUIDS_step(void);

// Draws the fluid cells. Call on renderer update.
void FLUIDS_draw(SDL_Renderer* renderer);
