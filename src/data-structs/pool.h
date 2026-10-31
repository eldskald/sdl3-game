#pragma once
#include <SDL3/SDL.h>

#define POOL_INITIAL_CAP 8

// Slots of values of `data_size` bytes each, each with an id that's its index
// and never changes while the value is on the pool, so it can be handed out to
// refer to it. Removed values free their slots to be reused by the next ones
// added, and the pool doubles its cap when it's full. Use `init_pool()` or
// `init_pool_with()` to allocate its initial memory, `add_to_pool()` and
// `remove_from_pool()` to change it and `pool_at()` to get its values. Iterate
// it with ids from `0` to `cap`, skipping the ones `pool_at()` returns `NULL`
// for. Access `data_size` and `cap` but don't change these values. Adding might
// move `items`, so don't hold on to pointers to values across adds. Don't
// forget to `deinit_pool()` once you're done with it or else memory will leak.
//
// ```
// struct pool {
//     void* items;
//     bool* used; // whether each slot of items has a value
//     size_t data_size;
//     size_t cap;
// };
// ```
typedef struct {
    void* items;
    bool* used;
    size_t data_size;
    size_t cap;
} pool;

// Allocates memory for `p`, of values of `data_size` bytes.
void init_pool(pool* p, size_t data_size);

// Allocates memory for `p`, of values of `data_size` bytes, with room for `cap`
// of them.
void init_pool_with(pool* p, size_t data_size, size_t cap);

// Copies `val` to the first free slot of `p`, or zeroes it if `val` is `NULL`.
// Returns its id.
size_t add_to_pool(const void* val, pool* p);

// Frees the slot of id `id` on `p`.
void remove_from_pool(size_t id, pool* p);

// Pointer to the value of id `id` on `p`, or `NULL` if there's none.
void* pool_at(size_t id, const pool* p);

// Frees all memory from `p`.
void deinit_pool(pool* p);
