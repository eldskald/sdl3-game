#include "tests.h"
#include "data-structs/list.test.h"
#include "data-structs/dynarr.test.h"
#include <stdio.h>
#include <SDL3/SDL.h>

void expect(int *errors, bool expectation) {
    if (expectation) {
        printf(GREEN " Passed\n" RESET);
    } else {
        printf(RED " Failed\n" RESET);
        (*errors)++;
    }
}

int main() {
    printf(BLUE "RUNNING TESTS\n" RESET);

    int errors = 0;

    list_test(&errors);
    dynarr_test(&errors);

    if (errors) printf(RED "\nFAILED\n" RESET);
    else printf(GREEN "\nPASSED\n" RESET);

    return errors > 0;
}
