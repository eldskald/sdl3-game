#include "tests.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>

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

void check(report* r, bool ok, const char* fmt, ...) {
    if (r->len < MAX_EXPECTATIONS)
        r->checks[r->len++] = ok;
    else if (!ok)
        r->checks[MAX_EXPECTATIONS - 1] = false;
    if (ok || r->errors_len >= MAX_ERRORS) return;
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(r->errors[r->errors_len++], FAIL_MESSAGE_MAX, fmt, args);
    va_end(args);
}

void finish_report(const char* desc, report* r) {
    expect(desc, r->len, r->checks);
    for (size_t i = 0; i < r->errors_len; i++)
        printf(RED "    %s" RESET "\n", r->errors[i]);
}

bool get_tests_result(void) {
    return errors > 0;
}

void clear_tiles_and_fluids(void) {
    TILEMAP_clear();
    for (Uint8 type = 0; type < FLUIDS_TOTAL; type++)
        FLUIDS_clear_at(0,
                        0,
                        TILEMAP_SIZE_X * FLUIDS_SPLIT_X,
                        TILEMAP_SIZE_Y * FLUIDS_SPLIT_Y,
                        type);
    FLUIDS_update();
}
