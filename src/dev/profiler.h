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

// Writes the stats of the toggled thread. Call on renderer after everything, at
// the final draw step that goes straight to the screen instead of any buffer.
void PROFILER_draw(SDL_Renderer* renderer);



// Allocates memory for the timers. Call at the start of the game.
void PROFILER_start(void);

// Frees allocated memory by `PROFILER_start()`. Call when quitting.
void PROFILER_stop(void);
