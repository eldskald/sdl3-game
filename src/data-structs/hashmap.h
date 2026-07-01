#pragma once
#include "data-structs/list.h"
#include <SDL3/SDL.h>

#define KEY_MAX_SIZE 256
#define HASHMAP_INITIAL_CAP 64 // Keep it a power of 2

// Hashmap. Use init_hashmap() to allocate its initial memory. Use
// set_on_hashmap() and get_from_hashmap() to set and get values, and len and
// cap for length and capacity, but don't change these. The indexes property is
// a list of the occupied indexes on the values/keys array, use it if you need
// to iterate through the hashmap. After you're done, use clear_hashmap() to
// free the allocated memory.
//
// struct hashmap {
//     void** values;
//     char** keys;
//     list indexes;
//     size_t cap;
// };
typedef struct {
    void** values;
    char** keys;
    list indexes;
    size_t len;
    size_t cap;
} hashmap;

// Allocates the initial memory for the hashmap.
void init_hashmap(hashmap* map);

// Sets a key/value pair. Returns the value set, or the previous value if
// there was any that got replaced.
void* set_on_hashmap(void* value, char* key, hashmap* map);

// Gets the value with the given key, or NULL if there is no such key.
void* get_from_hashmap(char* key, hashmap* map);

// Frees the memory from the hashmap. Call after you're done with it.
void clear_hashmap(hashmap* map);
