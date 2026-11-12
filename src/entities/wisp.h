#pragma once
#include <SDL3/SDL.h>

// Loads the wisps' data from `WISP_CSV_PATH`. Call at the start of the game,
// after `ENTITY_start()`.
void WISP_start(void);

// Creates a wisp entity with the bottom center of its body at `(x,y)`, in
// pixels, that flies after the player. Returns its entity ID.
size_t WISP_create(double x, double y);
