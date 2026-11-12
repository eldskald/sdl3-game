#include "data-structs/dynarr.h"
#include "tests.h"
#include "tests/dynarr.h"
#include <SDL3/SDL.h>
#include <stdio.h>

static int int_at(size_t n, dynarr* arr) {
    return *(int*)dynarr_at(n, arr);
}

void test_dynarr(void) {
    printf(YELLOW "\nTesting dynarrs...\n" RESET);

    dynarr arr = (dynarr){0};
    init_dynarr(&arr, sizeof(int));
    expect("Initialized dynarr, checking initial length, cap and data size...",
           3,
           (bool[]){arr.len == 0,
                    arr.cap == DYNARR_INITIAL_CAP,
                    arr.data_size == sizeof(int)});

    push_to_dynarr(&(int){7}, &arr);
    push_to_dynarr(&(int){9}, &arr);
    expect("Pushed two values to dynarr, checking values, length and cap...",
           4,
           (bool[]){int_at(0, &arr) == 7,
                    int_at(1, &arr) == 9,
                    arr.len == 2,
                    arr.cap == DYNARR_INITIAL_CAP});

    remove_val_from_dynarr(&(int){9}, &arr);
    expect("Removed value from dynarr, checking values, length and cap...",
           3,
           (bool[]){int_at(0, &arr) == 7,
                    arr.len == 1,
                    arr.cap == DYNARR_INITIAL_CAP});

    push_to_dynarr(&(int){9}, &arr);
    remove_at_from_dynarr(0, &arr);
    expect("Reapplied removed value and then removed first element from "
           "dynarr, checking values, length and cap...",
           3,
           (bool[]){int_at(0, &arr) == 9,
                    arr.len == 1,
                    arr.cap == DYNARR_INITIAL_CAP});

    for (int i = 0; i < DYNARR_INITIAL_CAP; i++)
        push_to_dynarr(&i, &arr);
    expect("Pushed values until reaching cap, checking if cap was doubled...",
           1,
           (bool[]){arr.cap == DYNARR_INITIAL_CAP << 1});

    bool check = int_at(0, &arr) == 9;
    for (int i = 0; i < DYNARR_INITIAL_CAP; i++)
        if (int_at(i + 1, &arr) != i) check = false;
    expect("Checking values after doubling...", 1, (bool[]){check});

    int* grown = grow_dynarr(3, &arr);
    grown[0] = 1;
    grown[1] = 2;
    grown[2] = 3;
    expect("Grew dynarr by three values, checking values and length...",
           4,
           (bool[]){arr.len == DYNARR_INITIAL_CAP + 4,
                    int_at(DYNARR_INITIAL_CAP + 1, &arr) == 1,
                    int_at(DYNARR_INITIAL_CAP + 2, &arr) == 2,
                    int_at(DYNARR_INITIAL_CAP + 3, &arr) == 3});

    clear_dynarr(&arr);
    expect("Cleared dynarr, checking length and cap...",
           2,
           (bool[]){arr.len == 0, arr.cap == DYNARR_INITIAL_CAP << 1});

    deinit_dynarr(&arr);
    expect("Deinitialized dynarr, checking length, cap and data pointer...",
           3,
           (bool[]){arr.len == 0, arr.cap == 0, arr.data == NULL});

    init_dynarr(&arr, sizeof(int));
    for (int i = 0; i < DYNARR_INITIAL_CAP + 1; i++)
        push_to_dynarr(&i, &arr);
    shrink_dynarr(&arr);
    bool kept = true;
    for (int i = 0; i < DYNARR_INITIAL_CAP + 1; i++)
        if (int_at(i, &arr) != i) kept = false;
    expect("Pushed one more value than the initial cap to a new dynarr and "
           "shrank it, checking if its cap became its length and it kept its "
           "values...",
           3,
           (bool[]){arr.len == DYNARR_INITIAL_CAP + 1,
                    arr.cap == DYNARR_INITIAL_CAP + 1,
                    kept});

    push_to_dynarr(&(int){7}, &arr);
    expect("Pushed to it after, checking if it grew again...",
           3,
           (bool[]){arr.len == DYNARR_INITIAL_CAP + 2,
                    arr.cap > DYNARR_INITIAL_CAP + 2,
                    int_at(DYNARR_INITIAL_CAP + 1, &arr) == 7});

    clear_dynarr(&arr);
    shrink_dynarr(&arr);
    expect("Cleared and shrank it, checking if it freed everything...",
           2,
           (bool[]){arr.cap == 0, arr.data == NULL});

    push_to_dynarr(&(int){5}, &arr);
    expect("Pushed to it once empty, checking if it grew back...",
           3,
           (bool[]){arr.len == 1,
                    arr.cap == DYNARR_INITIAL_CAP,
                    int_at(0, &arr) == 5});
    deinit_dynarr(&arr);
}
