#include "data-structs/dynarr.h"
#include <SDL3/SDL.h>

void init_dynarr(dynarr* arr, size_t data_size) {
    init_dynarr_with(arr, data_size, DYNARR_INITIAL_CAP);
}

void init_dynarr_with(dynarr* arr, size_t data_size, size_t cap) {
    if (!arr) return;
    arr->data = cap ? SDL_calloc(cap, data_size) : NULL;
    arr->data_size = data_size;
    arr->cap = cap;
    arr->len = 0;
}

void* dynarr_at(size_t n, const dynarr* arr) {
    return (Uint8*)arr->data + n * arr->data_size;
}

void* push_to_dynarr(const void* val, dynarr* arr) {
    if (!arr) return NULL;
    void* slot = grow_dynarr(1, arr);
    if (val)
        SDL_memcpy(slot, val, arr->data_size);
    else
        SDL_memset(slot, 0, arr->data_size);
    return slot;
}

void* grow_dynarr(size_t n, dynarr* arr) {
    if (!arr) return NULL;

    // Doubles cap until it fits, allocates initial cap if it's empty
    if (arr->len + n > arr->cap) {
        size_t new_cap = arr->cap ? arr->cap : DYNARR_INITIAL_CAP;
        while (arr->len + n > new_cap)
            new_cap <<= 1;
        arr->data = SDL_realloc(arr->data, new_cap * arr->data_size);
        arr->cap = new_cap;
    }
    void* slot = dynarr_at(arr->len, arr);
    arr->len += n;
    return slot;
}

void remove_at_from_dynarr(size_t n, dynarr* arr) {
    if (!arr) return;
    if (n >= arr->len) return;
    SDL_memmove(dynarr_at(n, arr),
                dynarr_at(n + 1, arr),
                (arr->len - n - 1) * arr->data_size);
    arr->len--;
}

void remove_val_from_dynarr(const void* val, dynarr* arr) {
    if (!arr) return;
    for (size_t i = 0; i < arr->len; i++) {
        if (SDL_memcmp(dynarr_at(i, arr), val, arr->data_size) == 0) {
            remove_at_from_dynarr(i, arr);
            return;
        }
    }
}

void clear_dynarr(dynarr* arr) {
    if (!arr) return;
    arr->len = 0;
}

void deinit_dynarr(dynarr* arr) {
    if (!arr) return;
    SDL_free(arr->data);
    arr->data = NULL;
    arr->len = 0;
    arr->cap = 0;
}
