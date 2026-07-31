#pragma once
#include <SDL3/SDL.h>

typedef struct {
    SDL_FRect src;
    SDL_FRect dst;
    SDL_FlipMode flip_mode;
} SDL_RenderTexture_call;

typedef struct {
    float x;
    float y;
} SDL_RenderPoint_call;

typedef struct {
    SDL_FRect rect;
} SDL_RenderFillRect_call;

// Stub of `SDL_RenderTexture()`
void stubbed_SDL_RenderTexture(SDL_FRect* srcrect,
                               SDL_FRect* dstrect,
                               SDL_FlipMode flip_mode);

// Stub of `SDL_RenderPoint()`
void stubbed_SDL_RenderPoint(float x, float y);

// Stub of `SDL_RenderFillRect()`
void stubbed_SDL_RenderFillRect(SDL_FRect* rect);

// Points calls to an array of all `stubbed_SDL_RenderTexture()` calls. Returns
// the length of said array.
size_t get_SDL_RenderTexture_calls(SDL_RenderTexture_call** ptr);

// Points calls to an array of all `stubbed_SDL_RenderPoint()` calls. Returns
// the length of said array.
size_t get_SDL_RenderPoint_calls(SDL_RenderPoint_call** ptr);

// Points calls to an array of all `stubbed_SDL_RenderFillRect()` calls. Returns
// the length of said array.
size_t get_SDL_RenderFillRect_calls(SDL_RenderFillRect_call** ptr);

// Erase all calls.
void clear_stubbed_calls(void);
