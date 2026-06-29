#pragma once
#include <SDL3/SDL.h>

typedef struct {
    SDL_FRect src;
    SDL_FRect dst;
    SDL_FlipMode flip_mode;
} SDL_RenderTexture_call;

// Stub of SDL_RenderTexture()
void stubbed_SDL_RenderTexture(SDL_FRect* srcrect,
                               SDL_FRect* dstrect,
                               SDL_FlipMode flip_mode);

// Points calls to an array of all stubbed_SDL_RenderTexture calls. Returns the
// length of said array.
int get_SDL_RenderTexture_calls(SDL_RenderTexture_call** ptr);

// Erase all calls.
void clear_SDL_RenderTexture_calls(void);
