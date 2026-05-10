#include "manager.h"
#include "defs.h"
#include <SDL3/SDL.h>
#include <pthread.h>

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

const float sec2milisec = 1000.0f;

bool running = true;

const float main_thread_target_fps = 1000.0f / 60.0f;
const float update_thread_target_fps = 1000.0f / 30.0f;
float update_thread_dt = 0.0f;
float main_thread_dt = 0.0f;

bool pressing_1 = false;
bool pressing_2 = false;
bool pressing_3 = false;

int color = 0;

int _init_game(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize video! %s", SDL_GetError());
        return 1;
    }

    if (!SDL_CreateWindowAndRenderer(WINDOW_TITLE,
                                     WINDOW_SIZE_X,
                                     WINDOW_SIZE_Y,
                                     0,
                                     &window,
                                     &renderer)) {
        SDL_Log("Failed to create window and renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Log("Window and renderer initialized successfully.");
    return 0;
}

void _end_game(void) {
    SDL_Log("Closing game...");

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void _main_thread(void) {
    while (running) {
        Uint64 cycle_start = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    running = false;
                    break;
                case SDL_SCANCODE_1:
                    pressing_1 = true;
                    break;
                case SDL_SCANCODE_2:
                    pressing_2 = true;
                    break;
                case SDL_SCANCODE_3:
                    pressing_3 = true;
                    break;
                default:
                    break;
                }
                break;
            case SDL_EVENT_KEY_UP:
                switch (event.key.scancode) {
                case SDL_SCANCODE_1:
                    pressing_1 = false;
                    break;
                case SDL_SCANCODE_2:
                    pressing_2 = false;
                    break;
                case SDL_SCANCODE_3:
                    pressing_3 = false;
                    break;
                default:
                    break;
                }
                break;
            }
        }

        // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        switch (color) {
        case 1:
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            break;
        case 2:
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            break;
        case 3:
            SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
            break;
        default:
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            break;
        }
        SDL_RenderDebugText(renderer, 100, 100, "Hello world!");
        SDL_RenderDebugTextFormat(
            renderer, 100, 120, "main_thread_dt = %.2f", main_thread_dt);
        SDL_RenderDebugTextFormat(
            renderer, 100, 140, "update_thread_dt = %.2f", update_thread_dt);

        SDL_RenderPresent(renderer);
        // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

        Uint64 cycle_end = SDL_GetPerformanceCounter();
        float elapsed_ms = (float)(cycle_end - cycle_start) /
                           (float)SDL_GetPerformanceFrequency() * sec2milisec;
        if (elapsed_ms < main_thread_target_fps) {
            SDL_Delay((Uint32)(main_thread_target_fps - elapsed_ms));
        }
        main_thread_dt = (float)(SDL_GetPerformanceCounter() - cycle_start) /
                         (float)SDL_GetPerformanceFrequency() * sec2milisec;
    }
}

void* _update_thread(void* arg) {
    while (running) {
        Uint64 cycle_start = SDL_GetPerformanceCounter();

        if (pressing_1) color = 1;
        if (pressing_2) color = 2;
        if (pressing_3) color = 3;

        Uint64 cycle_end = SDL_GetPerformanceCounter();
        float elapsed_ms = (float)(cycle_end - cycle_start) /
                           (float)SDL_GetPerformanceFrequency() * sec2milisec;
        if (elapsed_ms < update_thread_target_fps) {
            SDL_Delay((Uint32)(update_thread_target_fps - elapsed_ms));
        }
        update_thread_dt = (float)(SDL_GetPerformanceCounter() - cycle_start) /
                           (float)SDL_GetPerformanceFrequency() * sec2milisec;
    }
    return NULL;
}

int GAME_manager_run(void) {
    int err = _init_game();
    if (err) return err;

    pthread_t update_thread = 0;
    pthread_create(&update_thread, NULL, _update_thread, NULL);

    _main_thread();

    pthread_join(update_thread, NULL);
    _end_game();

    return 0;
}
