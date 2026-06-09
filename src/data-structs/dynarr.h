#pragma once
#include <SDL3/SDL.h>

#define DYNARR_INITIAL_CAP 8

// A dynamic array. Use init_dynarr() to allocate initial memory, then use
// push_to_dynarr() and remove_at_from_dynarr() to manipulate it. Access cap and
// and len but don't change these values. Don't forget to clear_dynarr() once
// you're done with the dynarr or else memory will leak.
//
// struct dynarr {
//     void** at;
//     size_t cap;
//     size_t len;
// };
typedef struct {
    void** at;
    size_t cap;
    size_t len;
} dynarr;

// Allocates memory for arr.
void init_dynarr(dynarr* arr);

// Pushes val to the top of arr.
void push_to_dynarr(void* val, dynarr* arr);

// Removes the value at at from arr.
void remove_at_from_dynarr(size_t at, dynarr* arr);

// Frees all memory from arr.
void clear_dynarr(dynarr* arr);
