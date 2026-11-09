#pragma once
#include <SDL3/SDL.h>

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define RESET "\x1b[0m"

#define MAX_EXPECTATIONS 256
#define FAIL_MESSAGE_MAX 256
#define NUM_TO_STR_MAX 16
#define SUB_CALLS_MAX 256

#define MAX_ERRORS 16

void expect(const char* desc, size_t n, bool expectations[n]);

// Checks of a single subject, like a file, passed to a single `expect()`,
// along with messages of what failed to be printed after it.
typedef struct {
    bool checks[MAX_EXPECTATIONS];
    size_t len;
    char errors[MAX_ERRORS][FAIL_MESSAGE_MAX];
    size_t errors_len;
} report;

// Adds a check to `r`, with a `printf()` style message of what failed when it
// isn't `ok`. Past `MAX_EXPECTATIONS` checks, failures go on the last one, and
// past `MAX_ERRORS` messages, they're dropped, so it still fails.
void check(report* r, bool ok, const char* fmt, ...);

// Passes the checks of `r` to `expect()` with `desc`, then prints the messages
// of what failed.
void finish_report(const char* desc, report* r);

bool get_tests_result(void);

// Clears the whole tilemap and every fluid on the whole fluids grid. Steps the
// fluids once to apply the clears, since tests don't run on the fluids thread.
void clear_tiles_and_fluids(void);
