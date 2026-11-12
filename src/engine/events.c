#include "engine/events.h"
#include "engine/game-manager.h"
#include "engine/inputs.h"
#include <SDL3/SDL.h>

static bool is_quit_event(Uint32 type) {
    return type == SDL_EVENT_WINDOW_CLOSE_REQUESTED || type == SDL_EVENT_QUIT;
}

static bool is_input_event(Uint32 type) {
    switch (type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        return true;
    default:
        return false;
    }
}

void EVENTS_handle(SDL_Event* event) {
    if (is_quit_event(event->type))
        GAME_MANAGER_quit();
    else if (is_input_event(event->type))
        INPUTS_handle_inputs(event);
}
