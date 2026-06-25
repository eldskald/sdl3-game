#include <data-structs/hashmap.h>
#include <stdio.h>
#include <tests.h>
#include <tests/hashmap.h>

void test_hashmap(void) {
    printf(YELLOW "\nTesting hashmaps...\n" RESET);

    hashmap map = (hashmap){0};
    init_hashmap(&map);
    expect("Initialized hashmap, checking initial length, cap, indexes and "
           "pointers...",
           5,
           (bool[]){map.len == 0,
                    map.cap == HASHMAP_INITIAL_CAP,
                    map.keys != NULL,
                    map.values != NULL,
                    map.indexes.head == NULL});

    int val1 = 0;
    int val2 = 0;
    set_on_hashmap(&val1, "test1", &map);
    set_on_hashmap(&val2, "test2", &map);
    expect("Inserted two values on the hashmap, checking them...",
           2,
           (bool[]){get_from_hashmap("test1", &map) == &val1,
                    get_from_hashmap("test2", &map) == &val2});

    size_t i1 = (size_t)map.indexes.head->next->val;
    size_t i2 = (size_t)map.indexes.head->val;
    expect("Checking indexes and len...",
           4,
           (bool[]){map.values[i1] == &val1,
                    map.values[i2] == &val2,
                    strcmp(map.keys[i1], "test1") == 0,
                    strcmp(map.keys[i2], "test2") == 0,
                    map.len == 2});

    set_on_hashmap(&val2, "test1", &map);
    expect("Inserted new value on previous key, checking it...",
           3,
           (bool[]){get_from_hashmap("test1", &map) == &val2,
                    map.values[i1] == &val2,
                    strcmp(map.keys[i1], "test1") == 0,
                    map.len == 2});

    int db[HASHMAP_INITIAL_CAP] = {0};
    for (int i = 0; i <= HASHMAP_INITIAL_CAP >> 1; i++) {
        char c[2] = {'a' + i, '\0'};
        set_on_hashmap(&db[i], c, &map);
    }
    expect("Inserted values until reaching half cap, checking if cap was "
           "doubled...",
           1,
           (bool[]){map.cap == HASHMAP_INITIAL_CAP << 1});

    bool check = true;
    if (get_from_hashmap("test1", &map) != &val2) check = false;
    if (get_from_hashmap("test2", &map) != &val2) check = false;
    for (int i = 0; i <= HASHMAP_INITIAL_CAP >> 1; i++) {
        char c[2] = {'a' + i, '\0'};
        if (get_from_hashmap(c, &map) != &db[i]) check = false;
    }
    expect("Cheking values after doubling...", 1, (bool[]){check});

    clear_hashmap(&map);
    expect("Cleared hashmap, checking length, cap, indexes and pointers...",
           5,
           (bool[]){map.len == 0,
                    map.cap == 0,
                    map.keys == NULL,
                    map.values == NULL,
                    map.indexes.head == NULL});
}
