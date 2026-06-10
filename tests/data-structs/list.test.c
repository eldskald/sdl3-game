#include "list.test.h"
#include "../../src/data-structs/list.h"
#include "../tests.h"
#include <stdio.h>

void list_test(int* errors) {
    printf(YELLOW "\nTesting lists...\n" RESET);

    list l = (list){0};
    printf("Initialized list, should have length %d, has %lu",
           0,
           get_list_len(&l));
    expect(errors, get_list_len(&l) == 0);

    push_to_list(13, &l);
    push_to_list(17, &l);
    push_to_list(31, &l);
    push_to_list(39, &l);
    printf("Pushed 13, 17, 31 and 39, checking the values and length...");
    expect(errors,
           l.head->val == 39 && l.head->next->val == 31 &&
               l.head->next->next->val == 17 &&
               l.head->next->next->next->val == 13 && get_list_len(&l) == 4);

    int top = pop_from_list(&l);
    printf("Popped from list, checking the values and length...");
    expect(errors,
           top == 39 && l.head->next->next->val == 13 &&
               l.head->next->val == 17 && l.head->val == 31 &&
               get_list_len(&l) == 3);

    remove_from_list(17, &l);
    printf("Removed 17 from list, checking values and length...");
    expect(errors,
           l.head->next->val == 13 && l.head->val == 31 &&
               get_list_len(&l) == 2);

    clear_list(&l);
    printf("Cleared list, checking values and length...");
    expect(errors, l.head == NULL && get_list_len(&l) == 0);
}
