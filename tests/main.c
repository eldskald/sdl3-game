#include "engine/tilemap.h"
#include "tests.h"
#include "tests/audio.h"
#include "tests/bgs.h"
#include "tests/chunks.h"
#include "tests/csv.h"
#include "tests/data-integrity.h"
#include "tests/drawing.h"
#include "tests/dynarr.h"
#include "tests/entity.h"
#include "tests/events.h"
#include "tests/fluids.h"
#include "tests/graphics.h"
#include "tests/hashmap.h"
#include "tests/inputs.h"
#include "tests/json.h"
#include "tests/physics.h"
#include "tests/pool.h"
#include "tests/reactions.h"
#include "tests/sfx.h"
#include "tests/shader-buffers.h"
#include "tests/text.h"
#include "tests/tilemap.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// LeakSanitizer calls these on startup for its default suppressions and
// options, so they apply however the tests are run. They need these names and
// external linkage for it, hence the `NOLINT`s.
//
// NVidia's Vulkan driver leaks memory once when the graphics tests create the
// GPU device, through the D-Bus connection it opens and never releases. It's
// harmless, but it shows up in the report, so we're suppressing it. The driver
// is unloaded by the time the report runs, so it can only be matched by D-Bus.
const char* __lsan_default_suppressions(void) { // NOLINT
    return "leak:libdbus-1.so\n";
}

const char* __lsan_default_options(void) { // NOLINT
    return "print_suppressions=0";
}

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
    if (argc == 1 || SDL_strcmp(argv[1], "entity") == 0) test_entity();
    if (argc == 1 || SDL_strcmp(argv[1], "fluids") == 0) test_fluids();
    if (argc == 1 || SDL_strcmp(argv[1], "reactions") == 0) test_reactions();
    if (argc == 1 || SDL_strcmp(argv[1], "chunks") == 0) test_chunks();
    if (argc == 1 || SDL_strcmp(argv[1], "graphics") == 0) test_graphics();
    if (argc == 1 || SDL_strcmp(argv[1], "drawing") == 0) test_drawing();
    if (argc == 1 || SDL_strcmp(argv[1], "text") == 0) test_text();
    if (argc == 1 || SDL_strcmp(argv[1], "inputs") == 0) test_inputs();
    if (argc == 1 || SDL_strcmp(argv[1], "events") == 0) test_events();
    if (argc == 1 || SDL_strcmp(argv[1], "audio") == 0) test_audio();
    if (argc == 1 || SDL_strcmp(argv[1], "sfx") == 0) test_sfx();
    if (argc == 1 || SDL_strcmp(argv[1], "bgs") == 0) test_bgs();
    if (argc == 1 || SDL_strcmp(argv[1], "data-integrity") == 0)
        test_data_integrity();
    if (argc == 1 || SDL_strcmp(argv[1], "shader-buffers") == 0)
        test_shader_buffers();

    TILEMAP_stop();

    bool failed = get_tests_result();

    if (failed)
        printf(RED "\nFAILED\n" RESET);
    else
        printf(GREEN "\nPASSED\n" RESET);
    return failed;
}
