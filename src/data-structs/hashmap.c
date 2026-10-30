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

static bool insert_in_lists(void* val,
                            char* key,
                            void** values,
                            char** keys,
                            size_t* indexes,
                            size_t len,
                            size_t cap,
                            void** old) {
    // Only works when cap is a power of 2
    size_t index = (size_t)(hash_key(key) & (Uint64)(cap - 1));

    // Linear probing
    while (keys[index] != NULL) {

        // If key already exists, update it
        if (SDL_strcmp(key, keys[index]) == 0) {
            *old = values[index];
            values[index] = val;
            return false;
        }

        index++;
        if (index >= cap) {
            index = 0;
        }
    }

    values[index] = val;
    keys[index] = SDL_calloc(KEY_MAX_SIZE + 1, sizeof(char));
    indexes[len] = index;
    SDL_strlcpy(keys[index], key, KEY_MAX_SIZE);
    *old = val;
    return true;
}



void init_hashmap(hashmap* map) {
    if (!map) return;
    map->values = SDL_calloc(HASHMAP_INITIAL_CAP, sizeof(void*));
    map->keys = SDL_calloc(HASHMAP_INITIAL_CAP, sizeof(char*));
    map->indexes = SDL_calloc(HASHMAP_INITIAL_CAP, sizeof(size_t));
    map->cap = HASHMAP_INITIAL_CAP;
    map->len = 0;
}

void* set_on_hashmap(void* value, char* key, hashmap* map) {
    if (!map) return NULL;
    void* old = NULL;

    // Doubles the cap if current len reaches to half cap
    if (map->len >= map->cap >> 1) {
        size_t new_cap = map->cap << 1;
        void** new_values = SDL_calloc(new_cap, sizeof(void*));
        char** new_keys = SDL_calloc(new_cap, sizeof(char*));
        size_t* new_indexes = SDL_calloc(new_cap, sizeof(size_t));
        for (size_t i = 0; i < map->len; i++) {
            insert_in_lists(map->values[map->indexes[i]],
                            map->keys[map->indexes[i]],
                            new_values,
                            new_keys,
                            new_indexes,
                            i,
                            new_cap,
                            &old);
            SDL_free(map->keys[map->indexes[i]]);
        }
        SDL_free(map->values);
        SDL_free(map->keys);
        SDL_free(map->indexes);
        map->values = new_values;
        map->keys = new_keys;
        map->indexes = new_indexes;
        map->cap = map->cap << 1;
    }

    // Inserts the new value
    bool new = insert_in_lists(value,
                               key,
                               map->values,
                               map->keys,
                               map->indexes,
                               map->len,
                               map->cap,
                               &old);
    if (new) map->len++;
    return old;
}

void* get_from_hashmap(char* key, hashmap* map) {
    if (!map) return NULL;
    size_t index = (size_t)(hash_key(key) & (Uint64)(map->cap - 1));
    while (map->keys[index] && SDL_strcmp(key, map->keys[index]) != 0) {
        index++;
        if (index >= map->cap) index = 0;
    }
    return map->keys[index] ? map->values[index] : NULL;
}

void clear_hashmap(hashmap* map) {
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
