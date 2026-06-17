#pragma once

// Loads textures, shaders, etc. Returns 0 when successful, 1 otherwise.
int RENDERER_start(void);

// Unloads textures, shaders, etc. Call when quitting the game.
void RENDERER_stop(void);

// Render step of game loop. Must happen on main thread.
void RENDERER_update(void);
