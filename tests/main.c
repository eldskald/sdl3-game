#include "engine/tilemap.h"
#include "tests.h"
#include "tests/chunks.h"
#include "tests/csv.h"
#include "tests/data-integrity.h"
#include "tests/drawing.h"
#include "tests/dynarr.h"
#include "tests/fluids.h"
#include "tests/hashmap.h"
#include "tests/inputs.h"
#include "tests/json.h"
#include "tests/physics.h"
#include "tests/pool.h"
#include "tests/reactions.h"
#include "tests/tilemap.h"
#include <SDL3/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_AUDIO, SDL_LOG_PRIORITY_COUNT);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_COUNT);

    printf(BLUE "\nRUNNING TESTS\n" RESET);

    TILEMAP_start();

    if (argc == 1 || SDL_strcmp(argv[1], "dynarr") == 0) test_dynarr();
    if (argc == 1 || SDL_strcmp(argv[1], "pool") == 0) test_pool();
    if (argc == 1 || SDL_strcmp(argv[1], "hashmap") == 0) test_hashmap();

    if (argc == 1 || SDL_strcmp(argv[1], "json") == 0) test_json();
    if (argc == 1 || SDL_strcmp(argv[1], "csv") == 0) test_csv();

    if (argc == 1 || SDL_strcmp(argv[1], "tilemap") == 0) test_tilemap();
    if (argc == 1 || SDL_strcmp(argv[1], "physics") == 0) test_physics();
    if (argc == 1 || SDL_strcmp(argv[1], "fluids") == 0) test_fluids();
    if (argc == 1 || SDL_strcmp(argv[1], "reactions") == 0) test_reactions();
    if (argc == 1 || SDL_strcmp(argv[1], "chunks") == 0) test_chunks();
    if (argc == 1 || SDL_strcmp(argv[1], "drawing") == 0) test_drawing();
    if (argc == 1 || SDL_strcmp(argv[1], "inputs") == 0) test_inputs();
    if (argc == 1 || SDL_strcmp(argv[1], "data-integrity") == 0)
        test_data_integrity();

    TILEMAP_stop();

    bool failed = get_tests_result();

    if (failed)
        printf(RED "\nFAILED\n" RESET);
    else
        printf(GREEN "\nPASSED\n" RESET);
    return failed;
}
