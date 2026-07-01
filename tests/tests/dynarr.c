#include "data-structs/dynarr.h"
#include "tests.h"
#include "tests/dynarr.h"
#include <SDL3/SDL.h>
#include <stdio.h>

void test_dynarr(void) {
    printf(YELLOW "\nTesting dynarrs...\n" RESET);

    dynarr arr = (dynarr){0};
    init_dynarr(&arr);
    expect("Initialized dynarr, checking initial length and cap...",
           2,
           (bool[]){arr.len == 0, arr.cap == DYNARR_INITIAL_CAP});

    int test = 0;
    push_to_dynarr(&test, &arr);
    push_to_dynarr(&arr, &arr);
    expect("Pushed two values to dynarr, checking values, length and cap...",
           4,
           (bool[]){arr.at[0] == &test,
                    arr.at[1] == &arr,
                    arr.len == 2,
                    arr.cap == DYNARR_INITIAL_CAP});

    remove_at_from_dynarr(0, &arr);
    expect("Removed first element from dynarr, checking values, length and "
           "cap...",
           3,
           (bool[]){
               arr.at[0] == &arr, arr.len == 1, arr.cap == DYNARR_INITIAL_CAP});

    int db[DYNARR_INITIAL_CAP] = {0};
    for (int i = 0; i < DYNARR_INITIAL_CAP; i++)
        push_to_dynarr(&db[i], &arr);
    expect("Pushed values until reaching cap, checking if cap was doubled...",
           1,
           (bool[]){arr.cap == DYNARR_INITIAL_CAP << 1});

    bool check = true;
    if (arr.at[0] != &arr)
        for (int i = 0; i < DYNARR_INITIAL_CAP; i++)
            if (arr.at[i + 1] != &db[i]) check = false;
    expect("Checking values after doubling...", 1, (bool[]){check});

    clear_dynarr(&arr);
    expect("Cleared dynarr, checking length, cap and at pointer...",
           3,
           (bool[]){arr.len == 0, arr.cap == 0, arr.at == NULL});
}
