#include "dev/fluids-debugger.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/renderer.h"
#include "engine/text.h"
#include <SDL3/SDL.h>

#define FONT font_main
#define LINE_MAX 8
#define CELL_X (WINDOW_SIZE_X / FLUIDS_CELL_W)
#define CELL_Y (WINDOW_SIZE_Y / FLUIDS_CELL_H)

static fluid_cell cells[CELL_X][CELL_Y] = {0};
static bool toggled = false;

static const SDL_FColor black = {0.0f, 0.0f, 0.0f, 1.0f};
static const SDL_FColor white = {1.0f, 1.0f, 1.0f, 1.0f};



void FLUIDS_DEBUGGER_toggle(void) {
    toggled = !toggled;
}

void FLUIDS_DEBUGGER_draw(void) {
    if (!toggled) return;

    SDL_FRect cam = CAMERA_get_rect();
    FLUIDS_get_at(SDL_floor(cam.x / FLUIDS_CELL_W),
                  SDL_floor(cam.y / FLUIDS_CELL_H),
                  SDL_floor(CELL_X),
                  SDL_floor(CELL_Y),
                  &cells);

    SDL_FRect screen = RENDERER_get_final_screen_rect();
    double fx = screen.w / WINDOW_SIZE_X;
    double fy = screen.h / WINDOW_SIZE_Y;

    for (int x = 0; x < CELL_X; x++) {
        for (int y = 0; y < CELL_Y; y++) {
            float vol = 0.0f;
            for (int i = 0; i < FLUIDS_TOTAL; i++)
                vol += cells[x][y].volume[i];
            if (vol < EPSILON) continue;

            char line[LINE_MAX];
            SDL_snprintf(line, LINE_MAX, "%.2f", vol);

            GRAPHICS_fill_rect(
                (SDL_FRect){
                    .x = x * FLUIDS_CELL_W * fx + screen.x + 1,
                    .y = y * FLUIDS_CELL_H * fy + screen.y + 1,
                    .w = FLUIDS_CELL_W * fx - 2,
                    .h = FLUIDS_CELL_H * fy - 2,
                },
                black);
            TEXT_draw_now(x * FLUIDS_CELL_W * fx + screen.x + 3,
                          y * FLUIDS_CELL_W * fy + screen.y + 3,
                          line,
                          FONT,
                          0,
                          white);
        }
    }
}
