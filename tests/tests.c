#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <tests.h>

static int errors = 0;

void expect(const char* desc, size_t n, bool expectations[n]) {
    printf("%s", desc);

    size_t misses_n = 0;
    size_t misses[MAX_EXPECTATIONS] = {0};
    for (size_t i = 0; i < n; i++) {
        if (!expectations[i]) {
            misses[misses_n] = i;
            misses_n++;
            errors++;
        }
    }
    if (misses_n > 0) {
        char msg[FAIL_MESSAGE_MAX] = "";
        if (misses_n > 1) {
            sprintf(msg, "At checks %lu", misses[0]);
            for (size_t i = 1; i < misses_n - 1; i++) {
                char str[NUM_TO_STR_MAX] = "";
                sprintf(str, ", %lu", misses[i]);
                strcat(msg, str);
            }
            char str[NUM_TO_STR_MAX] = "";
            sprintf(str, " and %lu", misses[misses_n - 1]);
            strcat(msg, str);
        } else {
            sprintf(msg, "At check %lu", misses[0]);
        }
        printf(RED " Failed - %s" RESET "\n", msg);
    } else {
        printf(GREEN " Passed" RESET "\n");
    }
}

bool get_tests_result(void) {
    return errors > 0;
}
