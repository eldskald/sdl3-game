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
    action action;
    double time_held;
    bool held;
    bool just_pressed;
    bool just_released;
} action_state;

// Returns the current state of the given action.
action_state INPUTS_get_action_state(action action);

// Poll and processes inputs. Call on game manager events.
void INPUTS_process(SDL_Event* event);

// Updates held times. Must be called on some update thread.
void INPUTS_update_held_times(void);
