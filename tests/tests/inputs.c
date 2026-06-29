#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <defs.h>
#include <engine/inputs.h>
#include <stdio.h>
#include <tests.h>
#include <tests/inputs.h>

void test_inputs(void) {
    printf(YELLOW "\nTesting inputs...\n" RESET);

    INPUTS_start();

    SDL_Event event = (SDL_Event){0};
    event.key = (SDL_KeyboardEvent){.key = SDLK_Z};
    event.type = SDL_EVENT_KEY_DOWN;
    INPUTS_process(&event);
    action_state state = INPUTS_get_action_state(action_jump);
    expect("Processing jump key down event, checking jump action state...",
           4,
           (bool[]){state.just_pressed == true,
                    state.just_released == false,
                    state.held == true,
                    state.time_held == 0.0f});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just pressed is true for one "
           "frame...",
           1,
           (bool[]){state.just_pressed == true});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just pressed is already false...",
           1,
           (bool[]){state.just_pressed == false});

    for (int i = 0; i < 28; i++)
        INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update enough times to increase held time, checking jump "
           "action state...",
           4,
           (bool[]){state.just_pressed == false,
                    state.just_released == false,
                    state.held == true,
                    SDL_abs(state.time_held - 0.5f) < FABS_ZERO_DIFF});

    event.key = (SDL_KeyboardEvent){.key = SDLK_Z};
    event.type = SDL_EVENT_KEY_UP;
    INPUTS_process(&event);
    state = INPUTS_get_action_state(action_jump);
    expect("Processing jump key up event, checking jump action state...",
           4,
           (bool[]){state.just_pressed == false,
                    state.just_released == true,
                    state.held == false,
                    state.time_held < FABS_ZERO_DIFF});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just released is true for one "
           "frame...",
           1,
           (bool[]){state.just_released == true});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just released is already false...",
           1,
           (bool[]){state.just_released == false});



    event.key = (SDL_KeyboardEvent){.key = SDLK_UP};
    event.type = SDL_EVENT_KEY_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing up key down, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == -1});

    event.key = (SDL_KeyboardEvent){.key = SDLK_DOWN};
    event.type = SDL_EVENT_KEY_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing down key down, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 0});

    event.key = (SDL_KeyboardEvent){.key = SDLK_UP};
    event.type = SDL_EVENT_KEY_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing up key up, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 1});

    event.key = (SDL_KeyboardEvent){.key = SDLK_DOWN};
    event.type = SDL_EVENT_KEY_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing down key up, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 0});

    event.key = (SDL_KeyboardEvent){.key = SDLK_LEFT};
    event.type = SDL_EVENT_KEY_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing left key down, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == -1});

    event.key = (SDL_KeyboardEvent){.key = SDLK_RIGHT};
    event.type = SDL_EVENT_KEY_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing right key down, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 0});

    event.key = (SDL_KeyboardEvent){.key = SDLK_LEFT};
    event.type = SDL_EVENT_KEY_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing left key up, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 1});

    event.key = (SDL_KeyboardEvent){.key = SDLK_RIGHT};
    event.type = SDL_EVENT_KEY_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing right key up, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 0});



    event = (SDL_Event){0};
    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_SOUTH};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    INPUTS_process(&event);
    state = INPUTS_get_action_state(action_jump);
    expect("Processing jump button down event, checking jump action state...",
           4,
           (bool[]){state.just_pressed == true,
                    state.just_released == false,
                    state.held == true,
                    state.time_held == 0.0f});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just pressed is true for one "
           "frame...",
           1,
           (bool[]){state.just_pressed == true});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just pressed is already false...",
           1,
           (bool[]){state.just_pressed == false});

    for (int i = 0; i < 28; i++)
        INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update enough times to increase held time, checking jump "
           "action state...",
           4,
           (bool[]){state.just_pressed == false,
                    state.just_released == false,
                    state.held == true,
                    SDL_abs(state.time_held - 0.5f) < FABS_ZERO_DIFF});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_SOUTH};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    INPUTS_process(&event);
    state = INPUTS_get_action_state(action_jump);
    expect("Processing jump button up event, checking jump action state...",
           4,
           (bool[]){state.just_pressed == false,
                    state.just_released == true,
                    state.held == false,
                    state.time_held < FABS_ZERO_DIFF});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just released is true for one "
           "frame...",
           1,
           (bool[]){state.just_released == true});

    INPUTS_update();
    state = INPUTS_get_action_state(action_jump);
    expect("Ticking update once, checking if just released is already false...",
           1,
           (bool[]){state.just_released == false});



    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_UP};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing up button down, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == -1});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing down button down, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 0});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_UP};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing up button up, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 1});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing down button up, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 0});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing left button down, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == -1});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing right button down, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 0});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing left button up, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 1});

    event.gbutton =
        (SDL_GamepadButtonEvent){.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing right button up, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 0});



    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    event.gaxis.value = (Sint16)32000;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion to the right, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 1});

    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    event.gaxis.value = (Sint16)-32000;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion to the left, checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == -1});

    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    event.gaxis.value = (Sint16)32000;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion down, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 1});

    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    event.gaxis.value = (Sint16)-32000;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion up, checking dir values...",
           1,
           (bool[]){INPUTS_get_y_dir() == -1});

    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    event.gaxis.value = (Sint16)900;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion to the right under the deadzone, "
           "checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 0});

    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    event.gaxis.value = (Sint16)-900;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion to the left under the deadzone, "
           "checking dir values...",
           1,
           (bool[]){INPUTS_get_x_dir() == 0});

    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    event.gaxis.value = (Sint16)900;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion down under the deadzone, checking dir "
           "values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 0});

    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    event.gaxis.value = (Sint16)-900;
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    INPUTS_process(&event);
    INPUTS_update();
    expect("Processing thumbstick motion up under the deadzone, checking dir "
           "values...",
           1,
           (bool[]){INPUTS_get_y_dir() == 0});


    INPUTS_stop();
}
