#include "engine/camera.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/game-manager.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_stdinc.h>

typedef struct {
    double speed;
    double smoothing_range;
} camera_data;

static camera_data data = (camera_data){0};
static double target_x = 0.0f;
static double target_y = 0.0f;
static double pos_x = 0.0f;
static double pos_y = 0.0f;

// Places the camera rect's center at `(x, y)`, clamped to the tilemap bounds.
static void set_center(double x, double y) {
    pos_x = SDL_clamp(x - WINDOW_SIZE_X / 2,
                      0.0f,
                      TILEMAP_SIZE_X * SPRITESHEET_CELL_X - WINDOW_SIZE_X);
    pos_y = SDL_clamp(y - WINDOW_SIZE_Y / 2,
                      0.0f,
                      TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y - WINDOW_SIZE_Y);
}



SDL_FRect CAMERA_get_rect() {
    return (SDL_FRect){
        .x = SDL_floor(pos_x),
        .y = SDL_floor(pos_y),
        .w = WINDOW_SIZE_X,
        .h = WINDOW_SIZE_Y,
    };
}

SDL_Rect CAMERA_get_simulation_rect(void) {
    SDL_FRect cam = CAMERA_get_rect();
    double x =
        (cam.x + cam.w / 2) / SPRITESHEET_CELL_X - SIMULATION_SPACE_X / 2;
    double y =
        (cam.y + cam.h / 2) / SPRITESHEET_CELL_Y - SIMULATION_SPACE_Y / 2;
    return (SDL_Rect){
        .x = SDL_clamp(
            (long)SDL_round(x), 0, TILEMAP_SIZE_X - SIMULATION_SPACE_X),
        .y = SDL_clamp(
            (long)SDL_round(y), 0, TILEMAP_SIZE_Y - SIMULATION_SPACE_Y),
        .w = SIMULATION_SPACE_X,
        .h = SIMULATION_SPACE_Y,
    };
}

void CAMERA_center_at(double x, double y) {
    set_center(x, y);
    target_x = x;
    target_y = y;
}

void CAMERA_target_at(double x, double y) {
    target_x = x;
    target_y = y;
}



void CAMERA_start(void) {
    char csv[] = {
#embed CAMERA_CSV_PATH suffix(, '\0')
    };

    parsing_field fields[] = {
        {"speed", parsing_double, &data.speed},
        {"smoothing_range", parsing_double, &data.smoothing_range},
    };
    dynarr parsed = CSV_parse(csv);
    PARSING_read_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);
}



void CAMERA_step(void) {
    double center_x = pos_x + WINDOW_SIZE_X / 2;
    double center_y = pos_y + WINDOW_SIZE_Y / 2;
    double d = SDL_sqrt((center_x - target_x) * (center_x - target_x) +
                        (center_y - target_y) * (center_y - target_y));
    double v = data.speed;
    if (d < data.smoothing_range) v *= d / data.smoothing_range;

    double step = v * GAME_MANAGER_get_current_dt();
    if (step >= d) {
        center_x = target_x;
        center_y = target_y;
    } else {
        center_x += (target_x - center_x) / d * step;
        center_y += (target_y - center_y) / d * step;
    }

    set_center(center_x, center_y);
}
