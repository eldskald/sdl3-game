#pragma once
#include <SDL3/SDL.h>

// Loads the spiders' data from `SPIDER_CSV_PATH`. Call at the start of the
// game, after `ENTITY_start()`.
void SPIDER_start(void);

// Creates a spider entity with the bottom center of its body at `(x,y)`, in
// pixels, that follows the player around, climbing walls and ceilings. Returns
// its entity ID.
size_t SPIDER_create(double x, double y);
