#include "data-structs/dynarr.h"
#include <SDL3/SDL.h>

void init_dynarr(dynarr* arr) {
    if (!arr) return;
    arr->at = SDL_calloc(DYNARR_INITIAL_CAP, sizeof(void*));
    arr->cap = DYNARR_INITIAL_CAP;
    arr->len = 0;
}

void push_to_dynarr(void* val, dynarr* arr) {
    if (!arr) return;

    // Doubles cap if len reaches cap
    if (arr->len == arr->cap) {
        size_t new_cap = arr->cap << 1;
        void** new = SDL_calloc(new_cap, sizeof(void*));
        SDL_memcpy(new, arr->at, arr->cap * sizeof(void*));
        SDL_free(arr->at);
        arr->at = new;
        arr->cap = new_cap;
    }
    arr->at[arr->len] = val;
    arr->len++;
}

void remove_at_from_dynarr(size_t at, dynarr* arr) {
    if (!arr) return;
    if (at < 0 || at >= arr->len) return;
    for (size_t i = at; i < arr->len; i++) {
        arr->at[i] = arr->at[i + 1];
    }
    arr->len--;
}

void clear_dynarr(dynarr* arr) {
    if (!arr) return;
    SDL_free(arr->at);
    arr->at = NULL;
    arr->len = 0;
    arr->cap = 0;
}
