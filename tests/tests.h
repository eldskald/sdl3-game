#pragma once
#include <SDL3/SDL.h>

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define RESET "\x1b[0m"

#define MAX_EXPECTATIONS 32
#define FAIL_MESSAGE_MAX 256
#define NUM_TO_STR_MAX 16
#define SUB_CALLS_MAX 256

void expect(const char* desc, size_t n, bool expectations[n]);

bool get_tests_result(void);
