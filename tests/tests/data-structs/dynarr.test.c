#include <data-structs/dynarr.h>
#include <stdio.h>
#include <tests.h>
#include <tests/data-structs/dynarr.test.h>

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

    for (int i = 0; i < DYNARR_INITIAL_CAP; i++)
        push_to_dynarr(&test, &arr);
    expect("Pushed values until reaching cap, checking if cap was doubled...",
           1,
           (bool[]){arr.cap == DYNARR_INITIAL_CAP << 1});

    expect("Checking previous values before doubling...",
           1,
           (bool[]){arr.at[0] == &arr});

    clear_dynarr(&arr);
    expect("Cleared dynarr, checking length, cap and at pointer...",
           3,
           (bool[]){arr.len == 0, arr.cap == 0, arr.at == NULL});
}
