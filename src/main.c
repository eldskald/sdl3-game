#define SDL_MAIN_USE_CALLBACKS
#include "game-manager.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** _appstate, int _argc, char** _argv) { // NOLINT
    if (GAME_MANAGER_start()) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Aborting.");
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* _appstate, SDL_Event* event) { // NOLINT
    if (GAME_MANAGER_events(event)) return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* _appstate) { // NOLINT
    GAME_MANAGER_update();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* _appstate, SDL_AppResult _result) { // NOLINT
    GAME_MANAGER_stop();
}
