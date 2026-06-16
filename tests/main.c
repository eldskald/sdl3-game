#include <SDL3/SDL.h>
#include <stdio.h>
#include <tests.h>
#include <tests/chunks.h>
#include <tests/drawing.h>
#include <tests/dynarr.h>
#include <tests/hashmap.h>
#include <tests/json.h>
#include <tests/list.h>

int main() {
    printf(BLUE "\nRUNNING TESTS\n" RESET);

    test_list();
    test_dynarr();
    test_hashmap();

    test_json();

    test_chunks();
    test_drawing();

    bool failed = get_tests_result();

    if (failed)
        printf(RED "\nFAILED\n" RESET);
    else
        printf(GREEN "\nPASSED\n" RESET);

    return failed;
}
