typedef struct node node;

// A node of the list.
// struct node {
//     int val;
//     node* next;
// };
struct node {
    int val;
    node* next;
};

// List of int types. Use push_to_list(), remove_from_list() and clear_list() to
// manipulate it. Always call clear_list() when done, or else memory leaks.
// struct list {
//     node* head;
// };
typedef struct {
    node* head;
} list;

// Pushes val to be the new head.
void push_to_list(int val, list* l);

// Iterates from head until it finds val and remove it.
void remove_from_list(int val, list* l);

// Clear all nodes, freeing all memory.
void clear_list(list* l);
