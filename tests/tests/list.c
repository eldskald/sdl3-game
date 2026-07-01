#include "data-structs/list.h"
#include "tests.h"
#include "tests/list.h"
#include <SDL3/SDL.h>
#include <stdio.h>

void test_list(void) {
    printf(YELLOW "\nTesting lists...\n" RESET);

    list l = (list){0};
    expect("Initialized list, checking length...",
           1,
           (bool[]){get_list_len(&l) == 0});

    int ptrs[] = {0, 0, 0, 0};
    push_to_list(&ptrs[0], &l);
    push_to_list(&ptrs[1], &l);
    push_to_list(&ptrs[2], &l);
    push_to_list(&ptrs[3], &l);
    expect("Pushed four pointers, checking values and length...",
           5,
           (bool[]){l.head->val == &ptrs[3],
                    l.head->next->val == &ptrs[2],
                    l.head->next->next->val == &ptrs[1],
                    l.head->next->next->next->val == &ptrs[0],
                    get_list_len(&l) == 4});

    void* top = pop_from_list(&l);
    expect("Popped from list, checking the values and length...",
           5,
           (bool[]){top == &ptrs[3],
                    l.head->next->next->val == &ptrs[0],
                    l.head->next->val == &ptrs[1],
                    l.head->val == &ptrs[2],
                    get_list_len(&l) == 3});

    remove_from_list(&ptrs[1], &l);
    expect("Removed a pointer from list, checking values and length...",
           3,
           (bool[]){l.head->next->val == &ptrs[0],
                    l.head->val == &ptrs[2],
                    get_list_len(&l) == 2});

    clear_list(&l);
    expect("Cleared list, checking values and length...",
           2,
           (bool[]){l.head == NULL, get_list_len(&l) == 0});
}
