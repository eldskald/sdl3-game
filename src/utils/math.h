#pragma once
#include <SDL3/SDL.h>

// Returns true if `(x,y)` is inside `rect`.
bool is_in_rect(SDL_FRect rect, double x, double y);

// Returns true if `a` and `b` are intersecting.
bool are_intersecting(SDL_FRect a, SDL_FRect b);

// Linear interpolation between `a` and `b`, at `t`.
double lerp(double a, double b, double t);
