#pragma once
#include <SDL3/SDL.h>

// Loads textures, shaders, etc. Returns 0 when successful, 1 otherwise.
int RENDERER_start(char *base_path);

// Unloads textures, shaders, etc. Call when quitting the game.
void RENDERER_stop(void);

// Render step of game loop. Must happen on main thread.
void RENDERER_update(void);
