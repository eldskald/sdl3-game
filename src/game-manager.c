#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <chunks.h>
#include <defs.h>
#include <game-manager.h>
#include <inputs.h>
#include <physics.h>
#include <renderer.h>

#ifdef DEV
#include <dev/dev.h>
#endif

#undef SDL_HINT_MAIN_CALLBACK_RATE
#define SDL_HINT_MAIN_CALLBACK_RATE 0

#define SEC2NANOSEC 1000000000

static SDL_Thread* fixed_t = NULL;

static const Uint64 fixed_t_target_rate = SEC2NANOSEC / FIXED_T_TARGET_FPS;
static Uint64 fixed_t_start = 0;
static Uint64 fixed_t_diff = 0;

static const Uint64 main_t_target_rate = SEC2NANOSEC / MAIN_T_TARGET_FPS;
static Uint64 main_t_start = 0;
static Uint64 main_t_diff = 0;

static bool running = true;



static int SDLCALL fixed_thread(void* _arg) { // NOLINT
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Started fixed loop thread.");

    while (running) {
        // Fixed thread loop

        INPUTS_update();
        PHYSICS_update();

#ifdef DEV
        DEV_update();
#endif

        // Calculate and cap tick rate
        if (fixed_t_target_rate > SDL_GetTicksNS() - fixed_t_start) {
            fixed_t_diff = fixed_t_target_rate;
            SDL_DelayPrecise(fixed_t_target_rate -
                             (SDL_GetTicksNS() - fixed_t_start));
        } else {
            fixed_t_diff = SDL_GetTicksNS() - fixed_t_start;
        }
        fixed_t_start = SDL_GetTicksNS();
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Ending fixed loop thread.");
    return 0;
}



int GAME_MANAGER_start(void) {
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_AUDIO, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_INFO);

    if (RENDERER_start()) return 1;
    if (INPUTS_start()) return 1;
    CHUNKS_start();
    PHYSICS_start();

#ifdef DEV
    DEV_start();
#endif


    fixed_t = SDL_CreateThread(fixed_thread, "fixed", NULL);

    return 0;
}


void GAME_MANAGER_stop(void) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Closing game...");

    SDL_WaitThread(fixed_t, NULL);

    INPUTS_stop();
    RENDERER_stop();
    CHUNKS_stop();
    PHYSICS_stop();

#ifdef DEV
    DEV_stop();
#endif

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

    // Main thread loop
    RENDERER_update();

    // Calculate and cap tick rate
    if (main_t_target_rate > SDL_GetTicksNS() - main_t_start) {
        main_t_diff = main_t_target_rate;
        SDL_DelayPrecise(main_t_target_rate -
                         (SDL_GetTicksNS() - main_t_start));
    } else {
        main_t_diff = SDL_GetTicksNS() - main_t_start;
    }
    main_t_start = SDL_GetTicksNS();
}


void GAME_MANAGER_quit(void) {
    running = false;
}


double GAME_MANAGER_get_current_dt(void) {
#ifndef TEST
    SDL_ThreadID fixed = SDL_GetThreadID(fixed_t);
    if (fixed == SDL_GetCurrentThreadID()) return GAME_MANAGER_get_fixed_dt();
    return GAME_MANAGER_get_main_dt();
#else
    return 1.0f / 60.0f;
#endif
}


double GAME_MANAGER_get_fixed_dt() {
#ifndef TEST
    return (double)fixed_t_diff / (double)SEC2NANOSEC;
#else
    return 1.0f / 60.0f;
#endif
}


double GAME_MANAGER_get_main_dt() {
#ifndef TEST
    return (double)main_t_diff / (double)SEC2NANOSEC;
#else
    return 1.0f / 60.0f;
#endif
}
