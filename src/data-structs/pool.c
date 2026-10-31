#include "data-structs/pool.h"
#include <SDL3/SDL.h>

static void* get_slot(size_t id, const pool* p) {
    return (Uint8*)p->items + id * p->data_size;
}

static void double_cap(pool* p) {
    size_t new_cap = p->cap ? p->cap << 1 : POOL_INITIAL_CAP;
    p->items = SDL_realloc(p->items, new_cap * p->data_size);
    p->used = SDL_realloc(p->used, new_cap * sizeof(bool));
    SDL_memset(p->used + p->cap, 0, (new_cap - p->cap) * sizeof(bool));
    p->cap = new_cap;
}

void init_pool(pool* p, size_t data_size) {
    init_pool_with(p, data_size, POOL_INITIAL_CAP);
}

void init_pool_with(pool* p, size_t data_size, size_t cap) {
    if (!p) return;
    p->items = SDL_calloc(cap, data_size);
    p->used = SDL_calloc(cap, sizeof(bool));
    p->data_size = data_size;
    p->cap = cap;
}

size_t add_to_pool(const void* val, pool* p) {
    size_t id = 0;
    while (id < p->cap && p->used[id])
        id++;
    if (id == p->cap) double_cap(p);

    void* slot = get_slot(id, p);
    if (val)
        SDL_memcpy(slot, val, p->data_size);
    else
        SDL_memset(slot, 0, p->data_size);
    p->used[id] = true;
    return id;
}

void remove_from_pool(size_t id, pool* p) {
    if (id >= p->cap) return;
    p->used[id] = false;
}

void* pool_at(size_t id, const pool* p) {
    if (id >= p->cap || !p->used[id]) return NULL;
    return get_slot(id, p);
}

void deinit_pool(pool* p) {
    if (!p) return;
    SDL_free(p->items);
    SDL_free(p->used);
    p->items = NULL;
    p->used = NULL;
    p->cap = 0;
}
