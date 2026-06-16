#include <data-structs/list.h>
#include <stdio.h>
#include <tests.h>
#include <tests/list.h>

void test_list(void) {
    printf(YELLOW "\nTesting lists...\n" RESET);

    list l = (list){0};
    expect("Initialized list, checking length...",
           1,
           (bool[]){get_list_len(&l) == 0});

    push_to_list(13, &l);
    push_to_list(17, &l);
    push_to_list(31, &l);
    push_to_list(39, &l);
    expect("Pushed 13, 17, 31 and 39, checking the values and length...",
           5,
           (bool[]){l.head->val == 39,
                    l.head->next->val == 31,
                    l.head->next->next->val == 17,
                    l.head->next->next->next->val == 13,
                    get_list_len(&l) == 4});

    int top = pop_from_list(&l);
    expect("Popped from list, checking the values and length...",
           5,
           (bool[]){top == 39,
                    l.head->next->next->val == 13,
                    l.head->next->val == 17,
                    l.head->val == 31,
                    get_list_len(&l) == 3});

    remove_from_list(17, &l);
    expect("Removed 17 from list, checking values and length...",
           3,
           (bool[]){l.head->next->val == 13,
                    l.head->val == 31,
                    get_list_len(&l) == 2});

    clear_list(&l);
    expect("Cleared list, checking values and length...",
           2,
           (bool[]){l.head == NULL, get_list_len(&l) == 0});
}
