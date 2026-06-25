#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <stdio.h>
#include <tests.h>
#include <tests/chunks.h>
#include <tests/drawing.h>
#include <tests/dynarr.h>
#include <tests/hashmap.h>
#include <tests/json.h>
#include <tests/list.h>
#include <tests/physics.h>

int main() {
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_AUDIO, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_COUNT);

    printf(BLUE "\nRUNNING TESTS\n" RESET);

    test_list();
    test_dynarr();
    test_hashmap();

    test_json();

    test_physics();
    test_chunks();
    test_drawing();

    bool failed = get_tests_result();

    if (failed)
        printf(RED "\nFAILED\n" RESET);
    else
        printf(GREEN "\nPASSED\n" RESET);

    return failed;
}
