#pragma once
#include <SDL3/SDL.h>

// Thread type type, returned by `GAME_MANAGER_get_current_thread()`.
//
// ```
// enum thread { thread_main, thread_fixed, thread_fluid };
// ```
typedef enum { thread_main = 1, thread_fixed, thread_fluids } thread;

// Initializes game. Call on `SDL_AppInit()`. Returns `0` when successful, `1`
// otherwise.
int GAME_MANAGER_start(void);

// Main thread loop. Call on `SDL_AppIterate()`.
void GAME_MANAGER_update(void);

// Event handling. Call on `SDL_AppEvent()`. Returns `1` when game is quitting,
// `0` otherwise.
int GAME_MANAGER_events(SDL_Event* event);

// Unloads game. Call on `SDL_AppQuit()`.
void GAME_MANAGER_stop(void);

// Requests to quit the game.
void GAME_MANAGER_quit(void);

// Current thread's delta time.
double GAME_MANAGER_get_current_dt(void);

// Returns current thread.
thread GAME_MANAGER_get_current_thread(void);

// Whether the current thread is the fluids thread. Always `false` before the
// fluids thread starts, so calls from the main thread on tests aren't mistaken
// for it.
bool GAME_MANAGER_is_fluids_thread(void);

// Main thread's delta time.
double GAME_MANAGER_get_main_dt(void);

// Fixed thread's delta time.
double GAME_MANAGER_get_fixed_dt(void);

// Fluids thread's delta time
double GAME_MANAGER_get_fluids_dt(void);
