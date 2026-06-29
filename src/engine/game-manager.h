#pragma once
#include <SDL3/SDL.h>

// Initializes game. Call on SDL_AppInit(). Returns 0 when successful, 1
// otherwise.
int GAME_MANAGER_start(void);

// Main thread loop. Call on SDL_AppIterate().
void GAME_MANAGER_update(void);

// Event handling. Call on SDL_AppEvent(). Returns 1 when game is quitting, 0
// otherwise.
int GAME_MANAGER_events(SDL_Event* event);

// Unloads game. Call on SDL_AppQuit().
void GAME_MANAGER_stop(void);

// Requests to quit the game.
void GAME_MANAGER_quit(void);

// Current thread's delta time.
double GAME_MANAGER_get_current_dt(void);

// Main thread's delta time.
double GAME_MANAGER_get_main_dt(void);

// Fixed thread's delta time.
double GAME_MANAGER_get_fixed_dt(void);
