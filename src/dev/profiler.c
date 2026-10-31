#include "dev/profiler.h"
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/game-manager.h"
#include <SDL3/SDL.h>

#define PAD 8.0f
#define SEPARATOR 6.0f
#define NEXT_LINE 12.0f
#define TOTAL_X 200.0f
#define RECT_WIDTH 400.0f
#define MILI2NANO 1000000

static char* thread_names[4] = {NULL, "main", "fixed", "fluids"};

static hashmap start[4] = {0};
static hashmap ready[4] = {0};
static Uint8 toggled = 0;

static SDL_Mutex* ready_mutexes[4] = {0};



void PROFILER_setup_timer(char* tag) {
    thread t = GAME_MANAGER_get_current_thread();
    Uint64 time = SDL_GetTicksNS();
    set_on_hashmap(&time, tag, &start[t]);
}

void PROFILER_finish_timer(char* tag) {
    thread t = GAME_MANAGER_get_current_thread();
    Uint64 time = SDL_GetTicksNS();
    Uint64 elapsed = time - *(Uint64*)get_from_hashmap(tag, &start[t]);
    set_on_hashmap(&elapsed, tag, &ready[t]);
}

Uint64 PROFILER_get_timer(char* tag) {
    thread t = GAME_MANAGER_get_current_thread();
    Uint64* elapsed = get_from_hashmap(tag, &ready[t]);
    return elapsed ? *elapsed : 0;
}



void PROFILER_start(void) {
    for (int i = 1; i <= 3; i++) {
        init_hashmap(&start[i], sizeof(Uint64));
        init_hashmap(&ready[i], sizeof(Uint64));
        ready_mutexes[i] = SDL_CreateMutex();
    }
}

void PROFILER_stop(void) {
    for (int i = 1; i <= 3; i++) {
        deinit_hashmap(&start[i]);
        deinit_hashmap(&ready[i]);
        SDL_DestroyMutex(ready_mutexes[i]);
    }
}



void PROFILER_toggle_stats(thread t) {
    toggled = toggled == t ? 0 : t;
}

void PROFILER_draw(SDL_Renderer* renderer) {
    if (!toggled) return;

    hashmap vals;
    init_hashmap(&vals, sizeof(Uint64));
    SDL_LockMutex(ready_mutexes[toggled]);
    for (size_t i = 0; i < ready[toggled].len; i++)
        set_on_hashmap(hashmap_value_at(i, &ready[toggled]),
                       hashmap_key_at(i, &ready[toggled]),
                       &vals);
    SDL_UnlockMutex(ready_mutexes[toggled]);

    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderFillRect(renderer,
                       &(SDL_FRect){
                           .x = 0.0f,
                           .y = 0.0f,
                           .w = RECT_WIDTH,
                           .h = NEXT_LINE + PAD + SEPARATOR,
                       });

    SDL_SetRenderDrawColor(renderer, COL_WHITE);
    SDL_RenderDebugTextFormat(
        renderer, PAD, PAD, "%s thread", thread_names[toggled]);

    float y = PAD + NEXT_LINE + SEPARATOR;
    Uint64 total = 0;
    for (size_t i = 0; i < vals.len; i++) {
        SDL_SetRenderDrawColor(renderer, COL_BLACK);
        SDL_RenderFillRect(renderer,
                           &(SDL_FRect){
                               .x = 0.0f,
                               .y = y,
                               .w = RECT_WIDTH,
                               .h = NEXT_LINE + PAD,
                           });
        const char* tag = hashmap_key_at(i, &vals);
        Uint64 ticks = *(Uint64*)hashmap_value_at(i, &vals);
        total += ticks;
        SDL_SetRenderDrawColor(renderer, COL_WHITE);
        SDL_RenderDebugTextFormat(
            renderer, PAD, y, "%s: %.3fms", tag, ((float)ticks) / MILI2NANO);
        y += NEXT_LINE;
    }

    SDL_SetRenderDrawColor(renderer, COL_WHITE);
    SDL_RenderDebugTextFormat(
        renderer, TOTAL_X, PAD, "total: %.3fms", ((float)total) / MILI2NANO);

    deinit_hashmap(&vals);
}
