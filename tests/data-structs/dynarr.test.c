#include "dynarr.test.h"
#include "../../src/data-structs/dynarr.h"
#include "../tests.h"
#include <stdio.h>

void dynarr_test(int* errors) {
    printf(YELLOW "\nTesting dynarrs...\n" RESET);

    dynarr arr = (dynarr){0};
    init_dynarr(&arr);
    printf("Initialized dynarr, should have length %d and cap %d, has %lu and "
           "%lu...",
           0,
           DYNARR_INITIAL_CAP,
           arr.len,
           arr.cap);
    expect(errors, arr.len == 0 && arr.cap == DYNARR_INITIAL_CAP);

    push_to_dynarr(errors, &arr);
    push_to_dynarr(&arr, &arr);
    printf("Pushed two values to dynarr, checking values, length and cap...");
    expect(errors,
           arr.at[0] == errors && arr.at[1] == &arr && arr.len == 2 &&
               arr.cap == DYNARR_INITIAL_CAP);

    remove_at_from_dynarr(0, &arr);
    printf("Removed first element from dynarr, checking values, length and "
           "cap...");
    expect(errors,
           arr.at[0] == &arr && arr.len == 1 && arr.cap == DYNARR_INITIAL_CAP);

    for (int i = 0; i < DYNARR_INITIAL_CAP; i++)
        push_to_dynarr(errors, &arr);
    printf("Pushed values until reaching cap, checking if cap was doubled...");
    expect(errors, arr.cap == DYNARR_INITIAL_CAP << 1);

    clear_dynarr(&arr);
    printf("Cleared dynarr, should have length %d and cap %d, and at NULL...",
           0,
           0);
    expect(errors, arr.len == 0 && arr.cap == 0 && arr.at == NULL);
}
