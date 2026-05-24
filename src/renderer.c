#include "renderer.h"
#include "defs.h"
#include "sprites.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#ifdef DEV
#include "game-manager.h"
#endif

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* base_screen = NULL;



static int start_graphics_pipeline() {

    // Initialize video
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Failed to initialize video! %s",
                     SDL_GetError());
        return 1;
    }

    // Create window
    window = SDL_CreateWindow(WINDOW_TITLE,
                              STARTING_WINDOW_SIZE_X,
                              STARTING_WINDOW_SIZE_Y,
                              SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Failed to create window: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(window, MIN_WINDOW_SIZE_X, MIN_WINDOW_SIZE_Y);
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Window initialized successfully.");

    // Creating SDL_Renderer
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Couldn't create SDL_Renderer: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "SDL_Renderer created successfully.");

    // Creating SDL_Texture for the base game
    base_screen = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ABGR8888,
                                    SDL_TEXTUREACCESS_TARGET,
                                    WINDOW_SIZE_X,
                                    WINDOW_SIZE_Y);
    if (!base_screen) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture base_screen: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(base_screen, SDL_SCALEMODE_NEAREST);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture base_screen created successfully.");

    return 0;
}



static void render_base_screen(void) {
    SDL_SetRenderTarget(renderer, base_screen);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);

    SPRITES_update(renderer);

    SDL_RenderPresent(renderer);
}



static void render_final_screen(void) {
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);

    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);
    float window_ar = (float)window_w / (float)window_h;
    float base_ar = (float)WINDOW_SIZE_X / (float)WINDOW_SIZE_Y;

    // Center and stretch base_screen
    int x, y, w, h;
    if (window_ar >= base_ar) {
        h = window_h;
        w = window_h * base_ar;
        y = 0;
        x = (window_w - w) / 2;
    } else {
        w = window_w;
        h = window_w / base_ar;
        x = 0;
        y = (window_h - h) / 2;
    }

    // Render base_screen
    SDL_FRect dstrect = (SDL_FRect){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    SDL_RenderTexture(renderer, base_screen, NULL, &dstrect);

    SDL_RenderPresent(renderer);
}



int RENDERER_start(char* base_path) {
    if (start_graphics_pipeline()) return 1;
    if (SPRITES_start(base_path, renderer)) return 1;
    return 0;
}


void RENDERER_stop(void) {
    SDL_DestroyTexture(base_screen);
    SDL_DestroyRenderer(renderer);
    SPRITES_stop();
    SDL_DestroyWindow(window);
}


void RENDERER_update(void) {
    render_base_screen();
    render_final_screen();
}
