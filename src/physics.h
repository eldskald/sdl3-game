#pragma once
#include <SDL3/SDL.h>
#include <data-structs/dynarr.h>

// Physics types, for which type of physics interactions it will have.
// `physics_actor` is for bodies that only collide with the tilemap, while
// `physics_intangible` is for no collisions at all, useful for moving with
// velocity alone.
//
// ```
// enum { physics_actor, physics_intangible }
// ```
typedef enum {
    physics_actor,
    physics_intangible,
} physics_type;

// Body information. Position is `x` and `y`, velocity is `vel_x` and `vel_y`,
// dimensions are `w` and `h`. `sleeping` turns off physics for it, while `type`
// is which type of physics it should have. `w` and `h` must not be zero.
//
// ```
// struct body {
//     physics_type type;
//     double x;
//     double y;
//     double vel_x;
//     double vel_y;
//     Uint8 w;
//     Uint8 h;
//     bool sleeping;
// };
// ```
typedef struct {
    physics_type type;
    double x;
    double y;
    double vel_x;
    double vel_y;
    Uint8 w;
    Uint8 h;
    bool sleeping;
} body;



// Creates a new body with `data` for its values, returning its index on
// `body_id`. In `data`, `type` can't be `body_null` and both `w` and `h` can't
// be zero. Returns `2` when `data` is invalid, `0` when everything went right.
int PHYSICS_new_body(body data, size_t* body_id);

// Deletes the body at index `body_id`. Returns `3` when `body_id` is outside
// the array's range, `1` when there is no body at that index, `0` when
// everything went right.
int PHYSICS_del_body(size_t body_id);

// Get body at `body_id` index and returns it on `data`. Returns `3` when
// `body_id` is outside the array's range, `1` when there is no body at that
// index, `0` when everything went right.
int PHYSICS_get_body(size_t body_id, body* data);

// Sets the body's velocity at `body_id` to `(vel_x,vel_y)`. Returns `3`
// when `body_id` is outside the array's range, `1` when there is no body at
// that index, `0` when everything went right.
int PHYSICS_set_velocity(size_t body_id, double vel_x, double vel_y);

// Adds velocity '(vel_x,vel_y)' to the body at `body_id`. Returns `3` when
// `body_id` is outside the array's range, `1` when there is no body at that
// index, `0` when everything went right.
int PHYSICS_add_velocity(size_t body_id, double vel_x, double vel_y);

// Sets the body at `body_id` to position `(x,y)` without taking collisions
// into account. Returns `3` when `body_id` is outside the array's range, `1`
// when there is no body at that index, `0` when everything went right.
int PHYSICS_set_position(size_t body_id, double x, double y);

// Updates the body at `body_id` with `data`. Returns `3` when `body_id` is
// outside the array's range, `2` when `data` is invalid, `1` when there is no
// body at that index, `0` when everything went right.
int PHYSICS_update_body(size_t body_id, body data);

// Get all bodies intersecting `rect`.
dynarr PHYSICS_get_intersecting_bodies(SDL_Rect rect);



// Initiates the body array. Call at the start of the game.
void PHYSICS_start(void);

// Frees all allocated memory. Call at the end of the game.
void PHYSICS_stop(void);

// Moves each body by its velocity, taking collisions into account.
void PHYSICS_update(void);
