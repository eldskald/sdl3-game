#include "hashmap.h"
#include "list.h"
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

static void* insert_in_lists(void* val,
                             char* key,
                             void** values,
                             char** keys,
                             list* indexes,
                             size_t cap) {
    // Only works when cap is a power of 2
    size_t index = (size_t)(hash_key(key) & (Uint64)(cap - 1));
    SDL_Log("%lu - %s", index, key);

    // Linear probing
    while (values[index] != NULL) {

        // If key already exists, update it
        if (SDL_strcmp(key, keys[index]) == 0) {
            void* old = values[index];
            values[index] = val;
            return old;
        }

        index++;
        if (index >= cap) {
            index = 0;
        }
    }

    values[index] = val;
    keys[index] = SDL_calloc(KEY_MAX_SIZE + 1, sizeof(char));
    SDL_strlcpy(keys[index], key, KEY_MAX_SIZE);
    push_to_list(index, indexes);
    SDL_Log("%p - %s", keys[index], keys[index]);
    return val;
}



void init_hashmap(hashmap* map) {
    if (!map) return;
    map->values = SDL_calloc(HASHMAP_INITIAL_CAP, sizeof(void*));
    map->keys = SDL_calloc(HASHMAP_INITIAL_CAP, sizeof(char*));
    map->indexes = (list){0};
    map->cap = HASHMAP_INITIAL_CAP;
    map->len = 0;
}

void* set_on_hashmap(void* value, char* key, hashmap* map) {
    if (!map) return NULL;

    // Doubles the cap if current len reaches to half cap
    if (map->len >= map->cap >> 1) {
        size_t new_cap = map->cap << 1;
        void** new_values = SDL_calloc(new_cap, sizeof(void*));
        char** new_keys = SDL_calloc(new_cap, sizeof(char*));
        list new_indexes = (list){0};
        for (node* i = map->indexes.head; i; i = i->next) {
            insert_in_lists(map->values[i->val],
                            map->keys[i->val],
                            new_values,
                            new_keys,
                            &new_indexes,
                            new_cap);
            SDL_free(map->keys[i->val]);
        }
        SDL_free(map->values);
        SDL_free(map->keys);
        clear_list(&map->indexes);
        map->values = new_values;
        map->keys = new_keys;
        map->indexes = new_indexes;
        map->cap = map->cap << 1;
    }

    // Inserts the new value
    void* ptr = insert_in_lists(
        value, key, map->values, map->keys, &map->indexes, map->cap);
    if (ptr == value) map->len++;
    return ptr;
}

void* get_from_hashmap(char* key, hashmap* map) {
    if (!map) return NULL;
    size_t index = (size_t)(hash_key(key) & (Uint64)(map->cap - 1));
    while (SDL_strcmp(key, map->keys[index]) != 0) {
        index++;
        if (index >= map->cap) index = 0;
        if (!map->keys[index]) return NULL;
    }
    return map->values[index];
}

void clear_hashmap(hashmap* map) {
    for (node* i = map->indexes.head; i; i = i->next)
        SDL_free(map->keys[i->val]);
    SDL_free(map->values);
    SDL_free(map->keys);
    clear_list(&map->indexes);
    map->cap = 0;
    map->len = 0;
}
