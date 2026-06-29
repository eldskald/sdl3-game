#pragma once
#include <SDL3/SDL.h>
#include <defs.h>

// All game actions.
//
// ```
// enum action {
//     action_up,
//     action_left,
//     action_down,
//     action_right,
//     action_jump,
// };
// ```
typedef enum {
    action_up,
    action_left,
    action_down,
    action_right,
    action_jump,
} action;

// Struct containing all input data of a given state.
//
// ```
// struct action_state {
//     action action;
//     double time_held;
//     bool held;
//     bool just_pressed;
//     bool just_released;
// };
// ```
typedef struct {
    double time_held;
    bool held;
    bool just_pressed;
    bool just_released;
} action_state;

// Returns the current state of the given action.
action_state INPUTS_get_action_state(action action);



// Initializes the system. Call when opening the game. Returns 0 when
// successful, 1 otherwise.
int INPUTS_start(void);

// Poll and processes inputs. Call on game manager events.
void INPUTS_process(SDL_Event* event);

// Updates held times. Must be called on some update thread.
void INPUTS_update(void);

// Frees allocated memory. Call when quitting.
void INPUTS_stop(void);



// Gets the direction the player is inputting in the X axis. `1` for right, `-1`
// for left and `0` for neutral.
Sint8 INPUTS_get_x_dir(void);

// Gets the direction the player is inputting in the Y axis. `1` for down, `-1`
// for up and `0` for neutral.
Sint8 INPUTS_get_y_dir(void);
