#include "utils/logger.h"
#include <SDL3/SDL.h>

// Longest name a resource can have, longer ones are cut.
#define NAME_MAX_LEN 256

bool log_creation(int category, const void* resource, const char* fmt, ...) {
    char name[NAME_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(name, sizeof(name), fmt, args);
    va_end(args);

    if (!resource) {
        SDL_LogError(category, "Couldn't create %s: %s", name, SDL_GetError());
        return false;
    }
    name[0] = SDL_toupper(name[0]);
    SDL_LogInfo(category, "%s created successfully.", name);
    return true;
}
