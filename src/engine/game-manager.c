#include "engine/game-manager.h"
#include "defs.h"
#include "engine/chunks.h"
#include "engine/fluids.h"
#include "engine/inputs.h"
#include "engine/physics.h"
#include "engine/reactions.h"
#include "engine/renderer.h"
#include "engine/sprites.h"
#include "player/player-core.h"
#include <SDL3/SDL.h>

#ifdef DEV
#include "dev/dev.h"
#endif

#undef SDL_HINT_MAIN_CALLBACK_RATE
#define SDL_HINT_MAIN_CALLBACK_RATE 0

#define SEC2NANOSEC 1000000000

static SDL_Thread* fixed_t = NULL;
static SDL_Thread* fluids_t = NULL;

static const Uint64 fluids_t_target_rate = SEC2NANOSEC / FLUIDS_T_TARGET_FPS;
static Uint64 fluids_t_start = 0;
static Uint64 fluids_t_diff = 0;

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
        PLAYER_update();

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

static int SDLCALL fluids_thread(void* _arg) { // NOLINT
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Started fluids loop thread.");

    while (running) {
        // Fluids thread loop

        FLUIDS_step();
        REACTIONS_do_fluid_fluid_to_tile_reactions();

        // Calculate and cap tick rate
        if (fluids_t_target_rate > SDL_GetTicksNS() - fluids_t_start) {
            fluids_t_diff = fluids_t_target_rate;
            SDL_DelayPrecise(fluids_t_target_rate -
                             (SDL_GetTicksNS() - fluids_t_start));
        } else {
            fluids_t_diff = SDL_GetTicksNS() - fluids_t_start;
        }
        fluids_t_start = SDL_GetTicksNS();
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Ending fluids loop thread.");
    return 0;
}



int GAME_MANAGER_start(void) {
#ifdef DEV
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_AUDIO, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_INFO);
#endif

    if (INPUTS_start()) return 1;
    if (RENDERER_start()) return 1;
    SPRITES_start();
    CHUNKS_start();
    PHYSICS_start();
    FLUIDS_start();
    REACTIONS_start();
    PLAYER_start();

#ifdef DEV
    DEV_start();
#endif

    fixed_t = SDL_CreateThread(fixed_thread, "fixed", NULL);
    fluids_t = SDL_CreateThread(fluids_thread, "fluids", NULL);

    return 0;
}

void GAME_MANAGER_stop(void) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Closing game...");

    SDL_WaitThread(fixed_t, NULL);
    SDL_WaitThread(fluids_t, NULL);

    INPUTS_stop();
    RENDERER_stop();
    SPRITES_stop();
    CHUNKS_stop();
    PHYSICS_stop();
    FLUIDS_stop();
    REACTIONS_stop();

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
    RENDERER_process(event);

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
    SDL_ThreadID fixed = SDL_GetThreadID(fixed_t);
    SDL_ThreadID fluids = SDL_GetThreadID(fluids_t);
    if (fixed == SDL_GetCurrentThreadID()) return GAME_MANAGER_get_fixed_dt();
    if (fluids == SDL_GetCurrentThreadID()) return GAME_MANAGER_get_fluids_dt();
    return GAME_MANAGER_get_main_dt();
}

bool GAME_MANAGER_is_fluids_thread(void) {
    return fluids_t && SDL_GetThreadID(fluids_t) == SDL_GetCurrentThreadID();
}

double GAME_MANAGER_get_fixed_dt() {
#ifndef TEST
    return (double)fixed_t_diff / (double)SEC2NANOSEC;
#else
    return 1.0f / (double)FIXED_T_TARGET_FPS;
#endif
}

double GAME_MANAGER_get_fluids_dt() {
#ifndef TEST
    return (double)fluids_t_diff / (double)SEC2NANOSEC;
#else
    return 1.0f / (double)FLUIDS_T_TARGET_FPS;
#endif
}

double GAME_MANAGER_get_main_dt() {
#ifndef TEST
    return (double)main_t_diff / (double)SEC2NANOSEC;
#else
    return 1.0f / (double)MAIN_T_TARGET_FPS;
#endif
}
