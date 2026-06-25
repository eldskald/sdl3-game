#pragma once
#include <SDL3/SDL.h>

// A node of the list.
//
// ```
// struct node {
//     void* val;
//     node* next;
// };
// ```
typedef struct node node;
struct node {
    void* val;
    node* next;
};

// List of int types. Use push_to_list(), remove_from_list() and clear_list() to
// manipulate it. Always call clear_list() when done, or else memory leaks.
//
// struct list {
//     node* head;
// };
typedef struct {
    node* head;
} list;

// Pushes val to be the new head.
void push_to_list(void* val, list* l);

// Removes the list's head and returns its value.
void* pop_from_list(list* l);

// Iterates from head until it finds val and remove it.
void remove_from_list(void* val, list* l);

// Returns the length of the list.
size_t get_list_len(list* l);

// Clear all nodes, freeing all memory.
void clear_list(list* l);
