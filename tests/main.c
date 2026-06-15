#include <SDL3/SDL.h>
#include <stdio.h>
#include <tests.h>
#include <tests/data-structs/dynarr.test.h>
#include <tests/data-structs/hashmap.test.h>
#include <tests/data-structs/list.test.h>
#include <tests/graphics/drawing.test.h>
#include <tests/services/json.test.h>

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
