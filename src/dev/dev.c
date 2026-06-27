#include <SDL3/SDL.h>
#include <dev/dev.h>
#include <inputs.h>

void DEV_start(void) {
}

void DEV_update(void) {
    if (INPUTS_get_action_state(action_jump).held) {
        SDL_Log("jump being held for: %f",
                INPUTS_get_action_state(action_jump).time_held);
    }
}

void DEV_stop(void) {
}
