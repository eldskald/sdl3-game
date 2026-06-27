#include <SDL3/SDL.h>
#include <defs.h>
#include <game-manager.h>
#include <inputs.h>

#define ACTION_NODE_MAX_INPUTS 16

typedef struct {
    action action;
    SDL_Keycode keyboard[ACTION_NODE_MAX_INPUTS];
    SDL_GamepadButton gamepad[ACTION_NODE_MAX_INPUTS];
    Uint8 keyboard_count;
    Uint8 gamepad_count;
} action_node;

static action_node map[] = {
    (action_node){
        .action = action_up,
        .keyboard = {SDLK_UP, SDLK_W},
        .gamepad = {SDL_GAMEPAD_BUTTON_DPAD_UP},
        .keyboard_count = 2,
        .gamepad_count = 1,
    },
    (action_node){
        .action = action_left,
        .keyboard = {SDLK_LEFT, SDLK_A},
        .gamepad = {SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        .keyboard_count = 2,
        .gamepad_count = 1,
    },
    (action_node){
        .action = action_down,
        .keyboard = {SDLK_DOWN, SDLK_S},
        .gamepad = {SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        .keyboard_count = 2,
        .gamepad_count = 1,
    },
    (action_node){
        .action = action_right,
        .keyboard = {SDLK_RIGHT, SDLK_D},
        .gamepad = {SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        .keyboard_count = 2,
        .gamepad_count = 1,
    },
    (action_node){
        .action = action_jump,
        .keyboard = {SDLK_J, SDLK_Z},
        .gamepad = {SDL_GAMEPAD_BUTTON_SOUTH},
        .keyboard_count = 2,
        .gamepad_count = 1,
    },
};

static action_state states[] = {
    (action_state){.action = action_up},
    (action_state){.action = action_left},
    (action_state){.action = action_down},
    (action_state){.action = action_right},
    (action_state){.action = action_jump},
};



static SDL_Gamepad* gamepad = NULL;

void INPUTS_process(SDL_Event* event) {
    // Resetting just_pressed and just_released values
    for (int i = 0; i < sizeof(states) / sizeof(action_state); i++) {
        states[i].just_released = false;
        states[i].just_pressed = true;
    }

    // Counting time held
    for (int i = 0; i < sizeof(states) / sizeof(action_state); i++)
        if (states[i].held)
            states[i].time_held += GAME_MANAGER_get_current_dt();

    // Handling key presses
    for (int i = 0; i < sizeof(map) / sizeof(action_node); i++) {
        for (int j = 0; j < map[i].keyboard_count; j++) {
            if (map[i].keyboard[j] == event->key.key) {
                if (states[i].held == false &&
                    event->type == SDL_EVENT_KEY_DOWN)
                    states[i].just_pressed = true;
                if (states[i].held == true && event->type == SDL_EVENT_KEY_UP) {
                    states[i].just_released = true;
                    states[i].time_held = 0.0f;
                }
                states[i].held = event->type == SDL_EVENT_KEY_DOWN;
            }
        }
    }

    // Handling gamepad connection
    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        if (gamepad == NULL) {
            gamepad = SDL_OpenGamepad(event->gdevice.which);
            if (!gamepad) {
                SDL_Log("Failed to open gamepad ID %u: %s",
                        (unsigned int)event->gdevice.which,
                        SDL_GetError());
            }
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        if (gamepad && (SDL_GetGamepadID(gamepad) == event->gdevice.which)) {
            SDL_CloseGamepad(gamepad);
            gamepad = NULL;
        }
    }

    // Handling button presses
    for (int i = 0; i < sizeof(map) / sizeof(action_node); i++) {
        for (int j = 0; j < map[i].gamepad_count; j++) {
            if (map[i].gamepad[j] == event->key.key) {
                if (states[i].held == false &&
                    event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                    states[i].just_pressed = true;
                if (states[i].held == true &&
                    event->type == SDL_EVENT_GAMEPAD_BUTTON_UP)
                    states[i].just_released = true;
                states[i].held = event->type == SDL_EVENT_KEY_DOWN;
            }
        }
    }
}

action_state INPUTS_get_action_state(action action) {
    return states[action];
}

void INPUTS_update_held_times(void) {
    for (int i = 0; i < sizeof(states) / sizeof(action_state); i++) {
        if (states[i].held)
            states[i].time_held += GAME_MANAGER_get_current_dt();
        else
            states[i].time_held = 0.0f;
    }
}
