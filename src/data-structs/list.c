#include "list.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

void push_to_list(int val, list* l) {
    node* n = SDL_malloc(sizeof(node));
    n->val = val;
    n->next = l->head;
    l->head = n;
}

void remove_from_list(int val, list* l) {
    if (!l->head) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to remove %d from empty list.",
                    val);
        return;
    }

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
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Tried to remove %d from list that didn't had it.",
                val);
}

void clear_list(list* l) {
    while (l->head) {
        node* n = l->head;
        l->head = n->next;
        SDL_free(n);
    }
}
