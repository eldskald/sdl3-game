#include "data-structs/dynarr.test.h"
#include "data-structs/hashmap.test.h"
#include "data-structs/list.test.h"
#include "graphics/drawing.test.h"
#include "services/json.test.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stdio.h>

int main() {
    printf(BLUE "\nRUNNING TESTS\n" RESET);

    test_list();
    test_dynarr();
    test_hashmap();

    test_json();

    test_drawing();

    bool failed = get_tests_result();

    if (failed)
        printf(RED "\nFAILED\n" RESET);
    else
        printf(GREEN "\nPASSED\n" RESET);

    return failed;
}
