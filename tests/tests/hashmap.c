#include "data-structs/hashmap.h"
#include "tests.h"
#include "tests/hashmap.h"
#include <SDL3/SDL.h>
#include <stdio.h>

static int int_of(const char* key, hashmap* map) {
    int* val = get_from_hashmap(key, map);
    return val ? *val : -1;
}

void test_hashmap(void) {
    printf(YELLOW "\nTesting hashmaps...\n" RESET);

    hashmap map = (hashmap){0};
    init_hashmap(&map, sizeof(int));
    expect("Initialized hashmap, checking initial length, cap, data size, "
           "indexes and pointers...",
           6,
           (bool[]){map.len == 0,
                    map.cap == HASHMAP_INITIAL_CAP,
                    map.data_size == sizeof(int),
                    map.keys != NULL,
                    map.values != NULL,
                    map.indexes != NULL});

    set_on_hashmap(&(int){0}, "test1", &map);
    set_on_hashmap(&(int){2}, "test2", &map);
    expect("Inserted two values on the hashmap, checking them...",
           3,
           (bool[]){int_of("test1", &map) == 0,
                    int_of("test2", &map) == 2,
                    get_from_hashmap("missing", &map) == NULL});

    expect("Checking iteration and len...",
           5,
           (bool[]){*(int*)hashmap_value_at(0, &map) == 0,
                    *(int*)hashmap_value_at(1, &map) == 2,
                    SDL_strcmp(hashmap_key_at(0, &map), "test1") == 0,
                    SDL_strcmp(hashmap_key_at(1, &map), "test2") == 0,
                    map.len == 2});

    set_on_hashmap(&(int){3}, "test1", &map);
    expect("Inserted new value on previous key, checking it...",
           3,
           (bool[]){int_of("test1", &map) == 3,
                    SDL_strcmp(hashmap_key_at(0, &map), "test1") == 0,
                    map.len == 2});

    for (int i = 0; i <= HASHMAP_INITIAL_CAP >> 1; i++) {
        char c[2] = {'a' + i, '\0'};
        set_on_hashmap(&i, c, &map);
    }
    expect("Inserted values until reaching half cap, checking if cap was "
           "doubled...",
           1,
           (bool[]){map.cap == HASHMAP_INITIAL_CAP << 1});

    bool check = int_of("test1", &map) == 3 && int_of("test2", &map) == 2;
    for (int i = 0; i <= HASHMAP_INITIAL_CAP >> 1; i++) {
        char c[2] = {'a' + i, '\0'};
        if (int_of(c, &map) != i) check = false;
    }
    expect("Cheking values after doubling...", 1, (bool[]){check});

    deinit_hashmap(&map);
    expect("Deinitialized hashmap, checking length, cap, indexes and "
           "pointers...",
           5,
           (bool[]){map.len == 0,
                    map.cap == 0,
                    map.keys == NULL,
                    map.values == NULL,
                    map.indexes == NULL});
}
