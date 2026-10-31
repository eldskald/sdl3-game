#pragma once
#include <SDL3/SDL.h>

#define DYNARR_INITIAL_CAP 8

// A dynamic array of values of `data_size` bytes each, packed one after the
// other on `data`. Use `init_dynarr()` or `init_dynarr_with()` to allocate its
// initial memory, `dynarr_at()` to get its values and `push_to_dynarr()`,
// `grow_dynarr()` and `remove_at_from_dynarr()` to change it. Access
// `data_size`, `cap` and `len` but don't change these values. Pushing might
// move `data`, so don't hold on to pointers to values across pushes. Don't
// forget to `deinit_dynarr()` once you're done with it or else memory will
// leak.
//
// ```
// struct dynarr {
//     void* data;
//     size_t data_size;
//     size_t cap;
//     size_t len;
// };
// ```
typedef struct {
    void* data;
    size_t data_size;
    size_t cap;
    size_t len;
} dynarr;

// Allocates memory for `arr`, of values of `data_size` bytes.
void init_dynarr(dynarr* arr, size_t data_size);

// Allocates memory for `arr`, of values of `data_size` bytes, with room for
// `cap` of them. A `cap` of `0` allocates nothing until the first push, for
// when there are many dynarrs and most are left empty.
void init_dynarr_with(dynarr* arr, size_t data_size, size_t cap);

// Pointer to the value at `n` on `arr`.
void* dynarr_at(size_t n, const dynarr* arr);

// Copies `val` to the top of `arr`, or zeroes it if `val` is `NULL`. Returns
// the pointer to it on `arr`.
void* push_to_dynarr(const void* val, dynarr* arr);

// Adds `n` values to the top of `arr`, left uninitialized for the caller to
// write. Returns the pointer to the first of them.
void* grow_dynarr(size_t n, dynarr* arr);

// Removes the value at `n` from `arr`, moving the ones after it back.
void remove_at_from_dynarr(size_t n, dynarr* arr);

// Removes the first value on `arr` equal to the one at `val`, comparing their
// bytes.
void remove_val_from_dynarr(const void* val, dynarr* arr);

// Removes every value from `arr`, keeping its memory to be reused.
void clear_dynarr(dynarr* arr);

// Frees all memory from `arr`.
void deinit_dynarr(dynarr* arr);
