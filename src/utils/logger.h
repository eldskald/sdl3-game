#pragma once
#include <SDL3/SDL.h>

// Logs on `category` whether a resource was created, from whether `resource`
// isn't `NULL`. `fmt` and what follows it, formatted like `SDL_Log()`, name
// it in lowercase, like `"texture %s", name`. Created, it logs it as info, as
// "Texture spritesheet created successfully.", otherwise it logs SDL's error,
// as "Couldn't create texture spritesheet: <error>". Returns whether it was
// created.
bool log_creation(int category,
                  const void* resource,
                  SDL_PRINTF_FORMAT_STRING const char* fmt,
                  ...) SDL_PRINTF_VARARG_FUNC(3);
