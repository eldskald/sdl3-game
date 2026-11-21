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

// Instantly moves the camera to its center becomes the `(x,y)` in pixel world
// coordinates.
void CAMERA_center_at(double x, double y);

// Makes the camera moves towards `(x,y)` with speed lerped from
// `smoothing_min_speed` to `smoothing_max_speed` by `distance / smoothing
// range`. If the distance is greater than `target_center_range`, it just
// instantly centers it target. All of these values are in the CSV.
void CAMERA_target_at(double x, double y);

// Loads the CSV values to initiate the camera system.
void CAMERA_start(void);

// Moves the camera to its current target based on frame time.
void CAMERA_update(void);
