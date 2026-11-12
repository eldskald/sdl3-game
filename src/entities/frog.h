#pragma once
#include <SDL3/SDL.h>

// Loads the frogs' data from `FROG_CSV_PATH`. Call at the start of the game.
void FROG_start(void);

// Creates a frog entity with the bottom center of its body at `(x,y)`, in
// pixels, that follows the player around. Returns its entity ID.
size_t FROG_create(double x, double y);
