#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <stdbool.h>

int main(int argc, char** argv) {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize video! %s", SDL_GetError());
        return 1;
    }

    if (!SDL_CreateWindowAndRenderer(
            "Simple Game Loop", 800, 600, 0, &window, &renderer)) {
        SDL_Log("Failed to create window and renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    const float targetFrameTimeMS = 1000.0f / 60.0f; // ~16.67 ms for 60 FPS.

    while (running) {
        Uint64 frameStart = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, 100, 100, "Hello world!");

        SDL_RenderPresent(renderer);

        Uint64 frameEnd = SDL_GetPerformanceCounter();
        float elapsedMS = (frameEnd - frameStart) /
                          (float)SDL_GetPerformanceFrequency() * 1000.0f;

        if (elapsedMS < targetFrameTimeMS) {
            SDL_Delay((Uint32)(targetFrameTimeMS - elapsedMS));
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
