#pragma once
#include <SDL3/SDL.h>

// Gets the camera rect in world pixels, snapped to whole pixels so everything
// drawn relative to it lines up with the screen's pixels.
SDL_FRect CAMERA_get_rect();

void CAMERA_center_at(double x, double y);

void CAMERA_target_at(double x, double y);

void CAMERA_start(void);

void CAMERA_step(void);
