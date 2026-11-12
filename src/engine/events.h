#pragma once
#include <SDL3/SDL.h>

// Handles an SDL event. Closing the window and `SDL_EVENT_QUIT`, sent when the
// OS or a signal like `SIGTERM` asks the game to quit, call
// `GAME_MANAGER_quit()`. Input events, from the keyboard and gamepads, are
// passed to `INPUTS_handle_inputs()`. Everything else is ignored for now. Call
// on game manager events.
void EVENTS_handle(SDL_Event* event);
