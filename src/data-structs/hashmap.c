#include "data-structs/hashmap.h"
#include <SDL3/SDL.h>

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL
static Uint64 hash_key(const char* key) {
    Uint64 hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (Uint64)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

static void* get_slot(size_t index, const hashmap* map) {
    return (Uint8*)map->values + index * map->data_size;
}

// Index on `keys` where `key` is, or the empty one it would be inserted at.
static size_t find_index(const char* key, const hashmap* map) {
    // Only works when cap is a power of 2
    size_t index = (size_t)(hash_key(key) & (Uint64)(map->cap - 1));

    // Linear probing
    while (map->keys[index] && SDL_strcmp(key, map->keys[index]) != 0) {
        index++;
        if (index >= map->cap) index = 0;
    }
    return index;
}

// Doubles the cap, moving every key/value pair to its index on the new cap.
static void double_cap(hashmap* map) {
    hashmap old = *map;
    map->cap = old.cap << 1;
    map->values = SDL_calloc(map->cap, map->data_size);
    map->keys = SDL_calloc(map->cap, sizeof(char*));
    map->indexes = SDL_calloc(map->cap, sizeof(size_t));
    for (size_t i = 0; i < old.len; i++) {
        size_t old_index = old.indexes[i];
        size_t index = find_index(old.keys[old_index], map);
        map->keys[index] = old.keys[old_index];
        SDL_memcpy(
            get_slot(index, map), get_slot(old_index, &old), map->data_size);
        map->indexes[i] = index;
    }
    SDL_free(old.values);
    SDL_free(old.keys);
    SDL_free(old.indexes);
}



void init_hashmap(hashmap* map, size_t data_size) {
    if (!map) return;
    map->values = SDL_calloc(HASHMAP_INITIAL_CAP, data_size);
    map->keys = SDL_calloc(HASHMAP_INITIAL_CAP, sizeof(char*));
    map->indexes = SDL_calloc(HASHMAP_INITIAL_CAP, sizeof(size_t));
    map->data_size = data_size;
    map->cap = HASHMAP_INITIAL_CAP;
    map->len = 0;
}

void* set_on_hashmap(const void* value, const char* key, hashmap* map) {
    if (!map) return NULL;

    // Doubles the cap if current len reaches to half cap
    if (map->len >= map->cap >> 1) double_cap(map);

    size_t index = find_index(key, map);
    if (!map->keys[index]) {
        map->keys[index] = SDL_calloc(KEY_MAX_SIZE + 1, sizeof(char));
        SDL_strlcpy(map->keys[index], key, KEY_MAX_SIZE);
        map->indexes[map->len] = index;
        map->len++;
    }
    void* slot = get_slot(index, map);
    SDL_memcpy(slot, value, map->data_size);
    return slot;
}

void* get_from_hashmap(const char* key, const hashmap* map) {
    if (!map) return NULL;
    size_t index = find_index(key, map);
    return map->keys[index] ? get_slot(index, map) : NULL;
}

void* hashmap_value_at(size_t i, const hashmap* map) {
    return get_slot(map->indexes[i], map);
}

const char* hashmap_key_at(size_t i, const hashmap* map) {
    return map->keys[map->indexes[i]];
}

void deinit_hashmap(hashmap* map) {
    for (size_t i = 0; i < map->len; i++) {
        SDL_free(map->keys[map->indexes[i]]);
    }
    SDL_free(map->values);
    SDL_free(map->keys);
    SDL_free(map->indexes);
    map->keys = NULL;
    map->values = NULL;
    map->indexes = NULL;
    map->cap = 0;
    map->len = 0;
}
