#include "list.h"
#include <SDL3/SDL.h>

void push_to_list(int val, list* l) {
    node* n = SDL_malloc(sizeof(node));
    n->val = val;
    n->next = l->head;
    l->head = n;
}

int pop_from_list(list* l) {
    node* top = l->head;
    int val = top->val;
    l->head = top->next;
    SDL_free(top);
    return val;
}

void remove_from_list(int val, list* l) {
    if (!l->head) return;

    if (l->head->val == val) {
        node* n = l->head;
        l->head = n->next;
        SDL_free(n);
        return;
    }

    node* curr = l->head;
    while (curr->next) {
        node* n = curr->next;
        if (n->val == val) {
            curr->next = n->next;
            SDL_free(n);
            return;
        }
        curr = n;
    }
}

size_t get_list_len(list* l) {
    size_t len = 0;
    for (node* i = l->head; i; i = i->next) len++;
    return len;
}

void clear_list(list* l) {
    if (!l) return;
    while (l->head) {
        node* n = l->head;
        l->head = n->next;
        SDL_free(n);
    }
}
