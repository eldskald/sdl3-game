#pragma once
#include <SDL3/SDL.h>

// Gets the camera rect in world pixels, snapped to whole pixels so everything
// drawn relative to it lines up with the screen's pixels.
SDL_FRect CAMERA_get_rect();

// Gets the simulation space in tiles, `SIMULATION_SPACE_X` by
// `SIMULATION_SPACE_Y` tiles centered on the camera. It slides instead of
// shrinking at the tilemap bounds, so it always has the same size. Nothing
// outside it is simulated.
SDL_Rect CAMERA_get_simulation_rect(void);

void CAMERA_center_at(double x, double y);

void CAMERA_target_at(double x, double y);

void CAMERA_start(void);

void CAMERA_step(void);
