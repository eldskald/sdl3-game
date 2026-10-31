#pragma once
#include <SDL3/SDL.h>

#define KEY_MAX_SIZE 256
#define HASHMAP_INITIAL_CAP 64 // Keep it a power of 2

// Hashmap of values of `data_size` bytes each. Use `init_hashmap()` to allocate
// its initial memory. Use `set_on_hashmap()` and `get_from_hashmap()` to set
// and get values, and `data_size`, `len` and `cap` for the values' size, length
// and capacity, but don't change these. The `values` and `keys` are the arrays
// containing values and keys indexed at the hashes, and these hashes are stored
// at `indexes`, ordered normally in case you need to iterate through key/value
// pairs, do something like from `i = 0` to `len`, you have
// `hashmap_value_at(i)` and `hashmap_key_at(i)` as the pairs. After you're
// done, use `deinit_hashmap()` to free the allocated memory.
//
// ```
// struct hashmap {
//     void* values;
//     char** keys;
//     size_t* indexes;
//     size_t data_size;
//     size_t len;
//     size_t cap;
// };
// ```
typedef struct {
    void* values;
    char** keys;
    size_t* indexes;
    size_t data_size;
    size_t len;
    size_t cap;
} hashmap;

// Allocates the initial memory for the hashmap, of values of `data_size`
// bytes.
void init_hashmap(hashmap* map, size_t data_size);

// Copies `value` to `key`, replacing the previous value if there was any.
// Returns the pointer to it on the hashmap.
void* set_on_hashmap(const void* value, const char* key, hashmap* map);

// Pointer to the value with the given key, or `NULL` if there is no such key.
void* get_from_hashmap(const char* key, const hashmap* map);

// Pointer to the value of the `i`th key set on the hashmap.
void* hashmap_value_at(size_t i, const hashmap* map);

// The `i`th key set on the hashmap.
const char* hashmap_key_at(size_t i, const hashmap* map);

// Frees the memory from the hashmap. Call after you're done with it.
void deinit_hashmap(hashmap* map);
