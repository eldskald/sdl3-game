#include "engine/game-manager.h"
#include "defs.h"
#include "dev/profiler.h"
#include "engine/audio.h"
#include "engine/bgs.h"
#include "engine/camera.h"
#include "engine/chunks.h"
#include "engine/entity.h"
#include "engine/events.h"
#include "engine/fluids.h"
#include "engine/inputs.h"
#include "engine/physics.h"
#include "engine/reactions.h"
#include "engine/renderer.h"
#include "engine/sfx.h"
#include "engine/sprites.h"
#include "engine/text.h"
#include "engine/tilemap.h"
#include "entities/frog.h"
#include "entities/spider.h"
#include "entities/wisp.h"
#include "player/player-core.h"
#include "props/vines.h"
#include "utils/logger.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_thread.h>

#ifdef DEV
#include "dev/dev.h"
#endif

#undef SDL_HINT_MAIN_CALLBACK_RATE
#define SDL_HINT_MAIN_CALLBACK_RATE 0

#define SEC2NANOSEC 1000000000



static app_args args = {0};

app_args GAME_MANAGER_get_args(void) {
    return args;
}

void GAME_MANAGER_parse_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (SDL_strcmp(arg, "muted") == 0) args.muted = true;
        if (SDL_strncmp(arg, "dev_level=", 10) == 0) args.dev_level = arg + 10;
    }
}



static SDL_Thread* fixed_t = NULL;
static SDL_Thread* fluids_t = NULL;

// Each loop's start is set right before it begins and its diff starts at the
// target rate, so the first ticks get a regular dt instead of zero or the whole
// time spent loading.
static const Uint64 fluids_t_target_rate = SEC2NANOSEC / FLUIDS_T_TARGET_FPS;
static Uint64 fluids_t_start = 0;
static Uint64 fluids_t_diff = SEC2NANOSEC / FLUIDS_T_TARGET_FPS;


static const Uint64 main_t_target_rate = SEC2NANOSEC / MAIN_T_TARGET_FPS;
static Uint64 main_t_start = 0;
static Uint64 main_t_diff = SEC2NANOSEC / MAIN_T_TARGET_FPS;

static bool running = true;



// When the fixed thread's tick `n` is due, in nanoseconds, counting from
// `start`.
static Uint64 fixed_tick_due(Uint64 start, Uint64 n) {
    return start + n * SEC2NANOSEC / FIXED_T_TARGET_FPS;
}

// A tick of the fixed thread, always `1.0 / FIXED_T_TARGET_FPS` seconds long.
static void fixed_tick(void) {
#if DEV && !TEST
    PROFILER_setup_timer("inputs");
#endif
    INPUTS_update();
#if DEV && !TEST
    PROFILER_finish_timer("inputs");
#endif

    PHYSICS_update();

#if DEV && !TEST
    PROFILER_setup_timer("vines");
#endif
    VINES_update();
#if DEV && !TEST
    PROFILER_finish_timer("vines");
#endif

#if DEV && !TEST
    PROFILER_setup_timer("player");
#endif
    PLAYER_update();
#if DEV && !TEST
    PROFILER_finish_timer("player");
#endif

#if DEV && !TEST
    PROFILER_setup_timer("entities");
#endif
    ENTITY_update();
#if DEV && !TEST
    PROFILER_finish_timer("entities");
#endif

#if DEV && !TEST
    PROFILER_setup_timer("camera");
#endif
    CAMERA_update();
#if DEV && !TEST
    PROFILER_finish_timer("camera");
#endif

#if DEV && !TEST
    PROFILER_setup_timer("sprites");
#endif
    SPRITES_update();
#if DEV && !TEST
    PROFILER_finish_timer("sprites");
#endif

#if DEV && !TEST
    PROFILER_setup_timer("fluids_bgs");
#endif
    FLUIDS_play_bgs();
#if DEV && !TEST
    PROFILER_finish_timer("fluids_bgs");
#endif

#if DEV && !TEST
    PROFILER_setup_timer("bgs");
#endif
    BGS_update();
#if DEV && !TEST
    PROFILER_finish_timer("bgs");
#endif

#if DEV && !TEST
    DEV_update();
#endif
}

// The fixed thread always ticks the same `dt`, so everything it moves moves the
// same at any frame rate, like predicted trajectories. When it falls behind,
// it ticks as many times as it needs to catch up, but at most
// `FIXED_T_MAX_CATCH_UP_TICKS` times, dropping the rest of a long stall, so it
// slows down instead of falling further behind. Ticks are due on time from
// when it started counting, `ticks` of them so far, so they don't drift.
static int SDLCALL fixed_thread(void* _arg) { // NOLINT
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Started fixed loop thread.");
    Uint64 start = SDL_GetTicksNS();
    Uint64 ticks = 0;

    while (running) {
        Uint64 now = SDL_GetTicksNS();
        for (int caught_up = 0;
             running && fixed_tick_due(start, ticks) <= now &&
             caught_up < FIXED_T_MAX_CATCH_UP_TICKS;
             caught_up++) {
            fixed_tick();
            ticks++;
        }

        // Still behind, it counts again from the tick it just did
        now = SDL_GetTicksNS();
        if (fixed_tick_due(start, ticks) <= now) {
            start = now;
            ticks = 1;
        }

        // Waits for the next tick
        Uint64 due = fixed_tick_due(start, ticks);
        if (due > now) SDL_DelayPrecise(due - now);
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Ending fixed loop thread.");
    return 0;
}

static int SDLCALL fluids_thread(void* _arg) { // NOLINT
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Started fluids loop thread.");
    fluids_t_start = SDL_GetTicksNS();

    while (running) {
        // Fluids thread loop

#if DEV && !TEST
        PROFILER_setup_timer("fluids");
#endif
        FLUIDS_update();
#if DEV && !TEST
        PROFILER_finish_timer("fluids");
#endif

#if DEV && !TEST
        PROFILER_setup_timer("fluid_fluid_into_tile_reactions");
#endif
        REACTIONS_do_fluid_fluid_into_tile_reactions();
#if DEV && !TEST
        PROFILER_finish_timer("fluid_fluid_into_tile_reactions");
#endif

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
    if (TEXT_start()) return 1;
    AUDIO_start();
    SFX_start();
    BGS_start();
    SPRITES_start();
    TILEMAP_start();
    CHUNKS_start();
    PHYSICS_start();
    VINES_start();
    FLUIDS_start();
    REACTIONS_start();
    PLAYER_start();
    ENTITY_start();
    FROG_start();
    SPIDER_start();
    WISP_start();
#ifndef DEV
    ENTITY_generate_trajectories();
#endif
    CAMERA_start();

#if DEV && !TEST
    PROFILER_start();
    DEV_start();
#endif

    main_t_start = SDL_GetTicksNS();
    fixed_t = SDL_CreateThread(fixed_thread, "fixed", NULL);
    if (!log_creation(SDL_LOG_CATEGORY_APPLICATION, fixed_t, "fixed thread"))
        return 1;
    fluids_t = SDL_CreateThread(fluids_thread, "fluids", NULL);
    if (!log_creation(SDL_LOG_CATEGORY_APPLICATION, fluids_t, "fluids thread"))
        return 1;

    return 0;
}

void GAME_MANAGER_stop(void) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Closing game...");

    SDL_WaitThread(fixed_t, NULL);
    SDL_WaitThread(fluids_t, NULL);

    ENTITY_stop();
    INPUTS_stop();
    TEXT_stop();
    RENDERER_stop();
    SPRITES_stop();
    CHUNKS_stop();
    TILEMAP_stop();
    PHYSICS_stop();
    VINES_stop();
    FLUIDS_stop();
    REACTIONS_stop();
    SFX_stop();
    BGS_stop();
    AUDIO_stop();

#if DEV && !TEST
    PROFILER_stop();
    DEV_stop();
#endif

    SDL_Quit();
}



int GAME_MANAGER_events(SDL_Event* event) {
    EVENTS_handle(event);
    return running ? 0 : 1;
}



int GAME_MANAGER_update(void) {
    // Main thread loop
    if (!running) return 1;

    TEXT_update();

#if DEV && !TEST
    PROFILER_setup_timer("renderer");
#endif
    RENDERER_update();
#if DEV && !TEST
    PROFILER_finish_timer("renderer");
#endif

    // Calculate and cap tick rate
    if (main_t_target_rate > SDL_GetTicksNS() - main_t_start) {
        main_t_diff = main_t_target_rate;
        SDL_DelayPrecise(main_t_target_rate -
                         (SDL_GetTicksNS() - main_t_start));
    } else {
        main_t_diff = SDL_GetTicksNS() - main_t_start;
    }
    main_t_start = SDL_GetTicksNS();

    return 0;
}



void GAME_MANAGER_quit(void) {
    running = false;
}



// Whether `t` is the current thread. `SDL_GetThreadID(NULL)` is the current
// thread's ID, so threads not created yet, like on tests, must be checked.
static bool is_current_thread(SDL_Thread* t) {
    return t && SDL_GetThreadID(t) == SDL_GetCurrentThreadID();
}

double GAME_MANAGER_get_current_dt(void) {
    if (is_current_thread(fixed_t)) return GAME_MANAGER_get_fixed_dt();
    if (is_current_thread(fluids_t)) return GAME_MANAGER_get_fluids_dt();
    return GAME_MANAGER_get_main_dt();
}

thread GAME_MANAGER_get_current_thread(void) {
    if (is_current_thread(fixed_t)) return thread_fixed;
    if (is_current_thread(fluids_t)) return thread_fluids;
    return thread_main;
}

bool GAME_MANAGER_is_fluids_thread(void) {
    return is_current_thread(fluids_t);
}

double GAME_MANAGER_get_fixed_dt() {
    return 1.0 / FIXED_T_TARGET_FPS;
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
