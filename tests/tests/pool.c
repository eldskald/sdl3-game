#include "data-structs/pool.h"
#include "tests.h"
#include "tests/pool.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// Value at `id` on `p`, or `-1` if there's none.
static int int_at(size_t id, pool* p) {
    int* val = pool_at(id, p);
    return val ? *val : -1;
}

void test_pool(void) {
    printf(YELLOW "\nTesting pools...\n" RESET);

    pool p = (pool){0};
    init_pool(&p, sizeof(int));
    expect("Initialized pool, checking initial cap and data size...",
           2,
           (bool[]){p.cap == POOL_INITIAL_CAP, p.data_size == sizeof(int)});

    size_t id0 = add_to_pool(&(int){7}, &p);
    size_t id1 = add_to_pool(&(int){9}, &p);
    expect("Added two values to pool, checking values at their ids...",
           3,
           (bool[]){id0 != id1, int_at(id0, &p) == 7, int_at(id1, &p) == 9});

    remove_from_pool(id0, &p);
    expect("Removed first value from pool, checking both ids...",
           2,
           (bool[]){pool_at(id0, &p) == NULL, int_at(id1, &p) == 9});

    size_t id2 = add_to_pool(&(int){11}, &p);
    expect("Added another value to pool, checking if it reused the removed "
           "value's id...",
           3,
           (bool[]){id2 == id0, int_at(id2, &p) == 11, int_at(id1, &p) == 9});

    // Fills the free slots left, so the next one added doubles the cap
    for (int i = 2; i < POOL_INITIAL_CAP; i++)
        add_to_pool(&i, &p);
    size_t id3 = add_to_pool(&(int){13}, &p);
    bool kept = int_at(id2, &p) == 11 && int_at(id1, &p) == 9;
    for (int i = 2; i < POOL_INITIAL_CAP; i++)
        if (int_at(i, &p) != i) kept = false;
    expect("Added values until reaching cap, checking if cap was doubled and "
           "values were kept...",
           5,
           (bool[]){p.cap == POOL_INITIAL_CAP << 1,
                    id3 == POOL_INITIAL_CAP,
                    int_at(id3, &p) == 13,
                    pool_at(POOL_INITIAL_CAP + 1, &p) == NULL,
                    kept});

    deinit_pool(&p);
    expect("Deinitialized pool, checking cap and pointers...",
           4,
           (bool[]){p.cap == 0,
                    p.items == NULL,
                    p.used == NULL,
                    pool_at(id1, &p) == NULL});
}
