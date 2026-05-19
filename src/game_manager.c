#include "game_manager.h"
#include "inputs.h"
#include "renderer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#undef SDL_HINT_MAIN_CALLBACK_RATE
#define SDL_HINT_MAIN_CALLBACK_RATE 0

#define SEC2NANOSEC 1000000.0f

static SDL_Thread* fixed_t = NULL;

static const Uint64 fixed_t_target_rate = (Uint64)SEC2NANOSEC / 30;
static float fixed_t_dt = 0.0f;
static Uint64 fixed_t_start = 0;

static const Uint64 main_t_target_rate = (Uint64)SEC2NANOSEC / 60;
static float main_t_dt = 0.0f;
static Uint64 main_t_start = 0;

static bool running = true;



// FIXED THREAD ----------------------------------------------------------------
// Put all ticks on the fixed loop here
static int SDLCALL fixed_thread(void* _arg) { // NOLINT
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Started fixed loop thread.");

    while (running) {

        // Fixed thread loop

        // Calculate and cap tick rate
        Uint64 now = SDL_GetTicksNS();
        if (fixed_t_target_rate > now - fixed_t_start) {
            fixed_t_dt = (float)fixed_t_target_rate / SEC2NANOSEC;
            SDL_DelayNS(fixed_t_target_rate - (now - fixed_t_start));
        } else {
            fixed_t_dt =
                (float)(SDL_GetTicksNS() - fixed_t_start) / SEC2NANOSEC;
        }
        fixed_t_start = SDL_GetTicksNS();
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Ending fixed loop thread.");
    return 0;
}
// -----------------------------------------------------------------------------



// EXPORTED FUNCTIONS ----------------------------------------------------------

// Called on SDL_AppInit()
int GAME_MANAGER_start(void) {
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_AUDIO, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_INFO);

    const char* base_path = SDL_GetBasePath();

    if (RENDERER_start((char*)base_path)) return 1;

    fixed_t = SDL_CreateThread(fixed_thread, "fixed", NULL);

    return 0;
}


// Called on SDL_AppQuit()
void GAME_MANAGER_stop(void) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Closing game...");

    SDL_WaitThread(fixed_t, NULL);

    RENDERER_stop();

    SDL_Quit();
}


// Called on SDL_AppEvent()
int GAME_MANAGER_events(SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        running = false;
        return 1;
    }
    if (!running) return 1;

    INPUTS_process(event);

    return 0;
}


// Called on SDL_AppIterate()
void GAME_MANAGER_update(void) {

    // Main thread loop
    RENDERER_update();

    // Calculate and cap tick rate
    Uint64 now = SDL_GetTicksNS();
    if (main_t_target_rate > now - main_t_start) {
        main_t_dt = (float)main_t_target_rate / SEC2NANOSEC;
        SDL_DelayNS(main_t_target_rate - (now - main_t_start));
    } else {
        main_t_dt = (float)(SDL_GetTicksNS() - main_t_start) / SEC2NANOSEC;
    }
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
