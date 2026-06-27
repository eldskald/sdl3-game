#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <defs.h>
#include <game-manager.h>
#include <inputs.h>

#define ACTION_NODE_MAX_INPUTS 16
#define AXIS_MAX_VALUE 32678
#define ANALOG_STICK_DEADZONE 1024

typedef struct {
    action action;
    SDL_Keycode keyboard[ACTION_NODE_MAX_INPUTS];
    SDL_GamepadButton gamepad[ACTION_NODE_MAX_INPUTS];
    Uint8 keyboard_count;
    Uint8 gamepad_count;
} action_node;

typedef struct {
    action_state state;
    bool just_just_pressed;
    bool just_just_released;
} state_wrapper;



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

static state_wrapper states[] = {
    (state_wrapper){.state = (action_state){0}},
    (state_wrapper){.state = (action_state){0}},
    (state_wrapper){.state = (action_state){0}},
    (state_wrapper){.state = (action_state){0}},
    (state_wrapper){.state = (action_state){0}},
};

static SDL_Gamepad* gamepad = NULL;

static Sint8 x_dir = 0;
static Sint8 y_dir = 0;



void INPUTS_process(SDL_Event* event) {
    // Handling key presses
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        for (int i = 0; i < sizeof(map) / sizeof(action_node); i++) {
            for (int j = 0; j < map[i].keyboard_count; j++) {
                if (map[i].keyboard[j] == event->key.key) {
                    if (states[i].state.held == false &&
                        event->type == SDL_EVENT_KEY_DOWN) {
                        states[i].state.just_pressed = true;
                        states[i].just_just_pressed = true;
                    }
                    if (states[i].state.held == true &&
                        event->type == SDL_EVENT_KEY_UP) {
                        states[i].state.just_released = true;
                        states[i].just_just_released = true;
                        states[i].state.time_held = 0.0f;
                    }
                    states[i].state.held = event->type == SDL_EVENT_KEY_DOWN;
                }
            }
        }
    }

    // Handling gamepad connection
    else if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        if (gamepad == NULL) {
            gamepad = SDL_OpenGamepad(event->gdevice.which);
            if (!gamepad) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Failed to open gamepad ID %u: %s",
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
    else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
             event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        for (int i = 0; i < sizeof(map) / sizeof(action_node); i++) {
            for (int j = 0; j < map[i].gamepad_count; j++) {
                if (map[i].gamepad[j] == event->gbutton.button) {
                    if (states[i].state.held == false &&
                        event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                        states[i].state.just_pressed = true;
                        states[i].just_just_pressed = true;
                    }
                    if (states[i].state.held == true &&
                        event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
                        states[i].state.just_released = true;
                        states[i].just_just_released = true;
                        states[i].state.time_held = 0.0f;
                    }
                    states[i].state.held =
                        event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                }
            }
        }
    }

    x_dir = 0;
    y_dir = 0;
    if (states[action_up].state.held) y_dir += -1;
    if (states[action_down].state.held) y_dir += 1;
    if (states[action_left].state.held) x_dir += -1;
    if (states[action_right].state.held) x_dir += 1;

    if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        switch (event->gaxis.axis) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            if (event->gaxis.value >= ANALOG_STICK_DEADZONE) x_dir = 1;
            if (event->gaxis.value <= -ANALOG_STICK_DEADZONE) x_dir = -1;
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            if (event->gaxis.value >= ANALOG_STICK_DEADZONE) y_dir = 1;
            if (event->gaxis.value <= -ANALOG_STICK_DEADZONE) y_dir = -1;
            break;
        }
    }
}

action_state INPUTS_get_action_state(action action) {
    return states[action].state;
}

void INPUTS_update(void) {
    // Resetting just_pressed and just_released values
    for (int i = 0; i < sizeof(states) / sizeof(state_wrapper); i++) {
        if (states[i].just_just_pressed)
            states[i].just_just_pressed = false;
        else
            states[i].state.just_pressed = false;
        if (states[i].just_just_released)
            states[i].just_just_released = false;
        else
            states[i].state.just_released = false;
    }

    for (int i = 0; i < sizeof(states) / sizeof(state_wrapper); i++) {
        if (states[i].state.held)
            states[i].state.time_held += GAME_MANAGER_get_current_dt();
        else
            states[i].state.time_held = 0.0f;
    }
}



Sint8 INPUTS_get_x_dir(void) {
    return x_dir;
}

Sint8 INPUTS_get_y_dir(void) {
    return y_dir;
}



int INPUTS_start(void) {
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to initialize gamepad! %s",
                     SDL_GetError());
        return 1;
    }
    return 0;
}

void INPUTS_stop(void) {
    if (gamepad) SDL_CloseGamepad(gamepad);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}
