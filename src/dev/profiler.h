#pragma once
#include "engine/game-manager.h"
#include <SDL3/SDL.h>

// Start to count time for the given tag
void PROFILER_setup_timer(char* tag);

// Stop counting and records the time for the given tag
void PROFILER_finish_timer(char* tag);

// Returns the latest
Uint64 PROFILER_get_timer(char* tag);



// Toggles the display of the given thread's stats
void PROFILER_toggle_stats(thread t);

// Draws the stats of the toggled thread right away on the current target, in
// its pixels from the top left, with `GRAPHICS_...()` and `TEXT_draw_now()`.
// The renderer calls it over the window, after post processing.
void PROFILER_draw(void);



// Allocates memory for the timers. Call at the start of the game.
void PROFILER_start(void);

// Frees allocated memory by `PROFILER_start()`. Call when quitting.
void PROFILER_stop(void);
