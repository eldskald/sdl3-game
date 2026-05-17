#include "game_manager.h"
#include "defs.h"
#include "inputs.h"
#include "renderer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>

#undef SDL_HINT_MAIN_CALLBACK_RATE
#define SDL_HINT_MAIN_CALLBACK_RATE 0

#define SEC2NANOSEC 1000000.0f

static SDL_Thread* fixed_t = NULL;

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

static const Uint64 fixed_t_target_fps = (Uint64)SEC2NANOSEC / 30;
static float fixed_t_dt = 0.0f;
static Uint64 fixed_t_start = 0;

static const Uint64 main_t_target_fps = (Uint64)SEC2NANOSEC / 60;
static float main_t_dt = 0.0f;
static Uint64 main_t_start = 0;

static bool running = true;

static int SDLCALL fixed_thread(void* _arg) { // NOLINT
    while (running) {

        // Update loop

        if (fixed_t_target_fps > SDL_GetTicksNS() - fixed_t_start) {
            SDL_DelayNS(fixed_t_target_fps -
                        (SDL_GetTicksNS() - fixed_t_start));
        }
        fixed_t_dt = (float)(SDL_GetTicksNS() - fixed_t_start) / SEC2NANOSEC;
        fixed_t_start = SDL_GetTicksNS();
    }
    return 0;
}

int GAME_MANAGER_start(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize video! %s", SDL_GetError());
        return 1;
    }

    if (!SDL_CreateWindowAndRenderer(WINDOW_TITLE,
                                     STARTING_WINDOW_SIZE_X,
                                     STARTING_WINDOW_SIZE_Y,
                                     0,
                                     &window,
                                     &renderer)) {
        SDL_Log("Failed to create window and renderer: %s", SDL_GetError());
        return 1;
    }
    SDL_Log("Window and renderer initialized successfully.");

    const char* base_path = SDL_GetBasePath();

    if (RENDERER_start((char*)base_path, renderer)) return 1;

    fixed_t = SDL_CreateThread(fixed_thread, "fixed", NULL);

    return 0;
}

void GAME_MANAGER_stop(void) {
    SDL_Log("Closing game...");

    SDL_WaitThread(fixed_t, NULL);

    RENDERER_stop();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int GAME_MANAGER_events(SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        running = false;
        return 1;
    }
    if (!running) return 1;

    INPUTS_process(event);

    return 0;
}

void GAME_MANAGER_update(void) {
    RENDERER_update(renderer);

    if (main_t_target_fps > SDL_GetTicksNS() - main_t_start) {
        SDL_DelayNS(main_t_target_fps - (SDL_GetTicksNS() - main_t_start));
    }
    main_t_dt = (float)(SDL_GetTicksNS() - main_t_start) / SEC2NANOSEC;
    main_t_start = SDL_GetTicksNS();
}

void GAME_MANAGER_quit(void) {
    running = false;
}

float GAME_MANAGER_get_fixed_dt() {
    return fixed_t_dt;
}

float GAME_MANAGER_get_main_dt() {
    return main_t_dt;
}
