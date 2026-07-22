#include "stubs.h"
#include "tests.h"
#include <SDL3/SDL.h>

static SDL_RenderTexture_call render_texture_calls[SUB_CALLS_MAX] = {0};
static size_t render_texture_calls_size = 0;

static SDL_RenderPoint_call render_point_calls[SUB_CALLS_MAX] = {0};
static size_t render_point_calls_size = 0;

void stubbed_SDL_RenderTexture(SDL_FRect* srcrect,
                               SDL_FRect* dstrect,
                               SDL_FlipMode flip_mode) {
    render_texture_calls[render_texture_calls_size].src = *srcrect;
    render_texture_calls[render_texture_calls_size].dst = *dstrect;
    render_texture_calls[render_texture_calls_size].flip_mode = flip_mode;
    render_texture_calls_size++;
}

void stubbed_SDL_RenderPoint(float x, float y) {
    render_point_calls[render_point_calls_size].x = x;
    render_point_calls[render_point_calls_size].y = y;
    render_point_calls_size++;
}

size_t get_SDL_RenderTexture_calls(SDL_RenderTexture_call** ptr) {
    *ptr = render_texture_calls;
    return render_texture_calls_size;
}

size_t get_SDL_RenderPoint_calls(SDL_RenderPoint_call** ptr) {
    *ptr = render_point_calls;
    return render_point_calls_size;
}

void clear_stubbed_calls(void) {
    for (size_t i = 0; i < render_texture_calls_size; i++) {
        render_texture_calls[i] = (SDL_RenderTexture_call){0};
    }
    render_texture_calls_size = 0;
    for (size_t i = 0; i < render_point_calls_size; i++) {
        render_point_calls[i] = (SDL_RenderPoint_call){0};
    }
    render_point_calls_size = 0;
}
