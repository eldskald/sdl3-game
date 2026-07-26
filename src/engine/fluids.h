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

// Creates a splash at `(x,y)` by taking fluids underneath and pressurizing it
// at `(x,y)`.
void FLUIDS_splash_at(Uint32 x, Uint32 y);



// Ticks the simulation. Call on update in any thread.
void FLUIDS_step(void);

// Draws the fluid cells. Call on renderer update.
void FLUIDS_draw(SDL_Renderer* renderer);
