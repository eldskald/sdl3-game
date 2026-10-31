#pragma once
#include "engine/graphics.h"
#include <SDL3/SDL.h>

// Calls made to the stubbed `GRAPHICS_*()` functions on
// `tests/stubbed/graphics.c`, which replaces `src/engine/graphics.c` on tests.

typedef struct {
    float x;
    float y;
} GRAPHICS_draw_point_call;

typedef struct {
    SDL_FRect rect;
} GRAPHICS_fill_rect_call;

// Points `ptr` to every vertex drawn with `GRAPHICS_draw_vertices()`, in the
// order they were drawn, and `texture` to the texture of the last of them.
// Returns how many there are.
size_t get_GRAPHICS_vertices(graphics_vertex** ptr, graphics_texture** texture);

// Points `ptr` to an array of all `GRAPHICS_draw_point()` calls. Returns the
// length of said array.
size_t get_GRAPHICS_draw_point_calls(GRAPHICS_draw_point_call** ptr);

// Points `ptr` to an array of all `GRAPHICS_fill_rect()` calls. Returns the
// length of said array.
size_t get_GRAPHICS_fill_rect_calls(GRAPHICS_fill_rect_call** ptr);

// Erase all calls.
void clear_stubbed_calls(void);
