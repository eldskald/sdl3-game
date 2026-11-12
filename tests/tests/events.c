#include "engine/events.h"
#include "engine/game-manager.h"
#include "engine/inputs.h"
#include "tests.h"
#include "tests/events.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// Quitting can't be undone, so it's tested last.
void test_events(void) {
    printf(YELLOW "\nTesting events...\n" RESET);

    INPUTS_start();

    SDL_Event event = (SDL_Event){0};
    event.key = (SDL_KeyboardEvent){.key = SDLK_Z};
    event.type = SDL_EVENT_KEY_DOWN;
    EVENTS_handle(&event);
    action_state state = INPUTS_get_action_state(action_jump);
    expect("Handled a jump key down event, checking if it reached the inputs "
           "and jump is held...",
           1,
           (bool[]){state.held == true});

    event = (SDL_Event){0};
    event.key = (SDL_KeyboardEvent){.key = SDLK_Z};
    event.type = SDL_EVENT_MOUSE_MOTION;
    EVENTS_handle(&event);
    state = INPUTS_get_action_state(action_jump);
    event = (SDL_Event){.type = SDL_EVENT_KEY_UP};
    event.key = (SDL_KeyboardEvent){.type = SDL_EVENT_KEY_UP, .key = SDLK_Z};
    EVENTS_handle(&event);
    expect("Handled a mouse motion event and a jump key up event, checking if "
           "only the key up released jump...",
           2,
           (bool[]){state.held == true,
                    INPUTS_get_action_state(action_jump).held == false});

    event = (SDL_Event){.type = SDL_EVENT_QUIT};
    expect("Handled a quit event, checking if the game manager is quitting...",
           1,
           (bool[]){GAME_MANAGER_events(&event) == 1});

    INPUTS_stop();
}
