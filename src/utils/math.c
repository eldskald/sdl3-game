#include "utils/math.h"
#include <SDL3/SDL.h>

bool is_in_rect(SDL_FRect rect, double x, double y) {
    return (rect.x < x && x < rect.x + rect.w && rect.y < y &&
            y < rect.y + rect.h);
}

bool are_intersecting(SDL_FRect a, SDL_FRect b) {
    SDL_FRect inter = (SDL_FRect){0};
    return SDL_GetRectIntersectionFloat(&a, &b, &inter);
}

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}
