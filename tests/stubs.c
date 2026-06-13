#include "stubs.h"
#include "tests.h"
#include <SDL3/SDL.h>

static SDL_RenderTexture_call calls[SUB_CALLS_MAX] = {0};
static int calls_size = 0;

void stubbed_SDL_RenderTexture(SDL_FRect* srcrect, SDL_FRect* dstrect) {
    calls[calls_size].src = *srcrect;
    calls[calls_size].dst = *dstrect;
    calls_size++;
}

int get_SDL_RenderTexture_calls(SDL_RenderTexture_call** ptr) {
    *ptr = calls;
    return calls_size;
}

void clear_SDL_RenderTexture_calls(void) {
    for (int i = 0; i < calls_size; i++) {
        calls[i] = (SDL_RenderTexture_call){0};
    }
    calls_size = 0;
}
