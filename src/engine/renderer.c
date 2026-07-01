#include "engine/renderer.h"
#include "defs.h"
#include "engine/sprites.h"
#include "engine/tilemap.h"
#include <SDL3/SDL.h>

#ifndef TEST
static SDL_Window* window = NULL;
static SDL_Texture* base_screen = NULL;
#endif
static SDL_Renderer* renderer = NULL;
static SDL_Texture* spritesheet = NULL;



static int start_graphics_pipeline() {
#ifndef TEST
    // Initialize video
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
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

#endif

    return 0;
}

static void load_graphics_files(void) {
#ifndef TEST
    Uint8 mem[] = {
#embed SPRITESHEET_PATH
    };
    SDL_IOStream* stream = SDL_IOFromConstMem(mem, sizeof(mem));
    SDL_Surface* surface = SDL_LoadPNG_IO(stream, true);
    spritesheet = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Spritesheet loaded successfully.");
#endif
}



static void render_base_screen(void) {
#ifndef TEST
    SDL_SetRenderTarget(renderer, base_screen);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);
#endif

    SPRITES_update(renderer, spritesheet);
    TILEMAP_draw(renderer, spritesheet);

#ifndef TEST
    SDL_RenderPresent(renderer);
#endif
}



static void render_final_screen(void) {
#ifndef TEST
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
#endif
}



int RENDERER_start(void) {
    if (start_graphics_pipeline()) return 1;
    load_graphics_files();
    SPRITES_start();
    return 0;
}


void RENDERER_stop(void) {
#ifndef TEST
    SDL_DestroyTexture(base_screen);
    SDL_DestroyRenderer(renderer);
#endif
    SPRITES_stop();
#ifndef TEST
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
}


void RENDERER_update(void) {
    render_base_screen();
    render_final_screen();
}
