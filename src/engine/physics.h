#pragma once
#include "data-structs/dynarr.h"
#include <SDL3/SDL.h>

// Body types, for which type of physics interactions it will have.
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
} body_type;

// Body information. Position is `x` and `y`, velocity is `vel_x` and `vel_y`,
// dimensions are `w` and `h`, while `type` is which type of physics it should
// have. `w` and `h` must not be zero. The `x,y,w,h` values must also fit inside
// the tilemap's space.
//
// ```
// struct body {
//     body_type type;
//     double x, y;
//     double vel_x, vel_y;
//     Uint8 w, h;
// };
// ```
typedef struct {
    body_type type;
    double x, y;
    double vel_x, vel_y;
    Uint8 w, h;
} body;

// Prop point, for making soft body simulated props like vines, cloth, chains,
// ropes, etc. You make these by creating several `prop_point`s and connecting
// them with `PHYSICS_connect_prop_points()`, for example, point A connected to
// B connected to C connected to D inelastic connections would create something
// that behaves like a rope. Pinned points don't move, so if point A on the
// previous example is pinned, the rope would hang from it. The `ignore_bodies`
// prop means it won't collide with bodies, and `acc_x/y` is a constant
// acceleration. Its velocity is multiplied by `1.0 - damping` each step on top
// of the global damping, so `0.0` means no extra damping and `1.0` stops it.
// Position must also be within the tilemap space.
//
// When the point gets deleted for any reason, `on_delete_func` is called with
// the deleted point's ID and `on_delete_arg`, so whichever prop created it can
// deal with it right away. It's called after the point and its connections are
// gone and can be `NULL`. It runs on the thread that deleted the point, which
// might be in the middle of `PHYSICS_update()`, so it must not create prop
// points or connections.
//
// ```
// struct prop_point {
//     double x, y;
//     double acc_x, acc_y;
//     double damping;
//     bool pinned;
//     bool ignore_bodies;
//     void (*on_delete_func)(size_t point_id, size_t arg);
//     size_t on_delete_arg;
// };
// ```
typedef struct {
    double x, y;
    double acc_x, acc_y;
    double damping;
    bool pinned;
    bool ignore_bodies;
    void (*on_delete_func)(size_t point_id, size_t arg);
    size_t on_delete_arg;
} prop_point;



// Creates a new body with `data` for its values, returning its index on
// `body_id`. In `data`, `type` can't be `body_null` and both `w` and `h` can't
// be zero and it must fit inside the tilemap space. Returns `2` when `data` is
// invalid, `0` when everything went right.
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
int PHYSICS_set_body_velocity(size_t body_id, double vel_x, double vel_y);

// Adds velocity '(vel_x,vel_y)' to the body at `body_id`. Returns `3` when
// `body_id` is outside the array's range, `1` when there is no body at that
// index, `0` when everything went right.
int PHYSICS_add_body_velocity(size_t body_id, double vel_x, double vel_y);

// Causes the body to drop down from platforms. Returns `3` when `body_id` is
// outside the array's range, `1` when there is no body at that index, `0` when
// everything went right.
int PHYSICS_drop_down_body(size_t body_id);

// Sets the body at `body_id` to position `(x,y)` without taking collisions
// into account. Returns `3` when `body_id` is outside the array's range, `1`
// when there is no body at that index, `0` when everything went right.
int PHYSICS_set_body_position(size_t body_id, double x, double y);

// Edits the body at `body_id` with `data`. Returns `3` when `body_id` is
// outside the array's range, `2` when `data` is invalid, `1` when there is no
// body at that index, `0` when everything went right.
int PHYSICS_edit_body(size_t body_id, body data);

// Deletes all values in `arr`, a `size_t` dynarr, and puts in all body indexes
// intersecting `rect`. Returns `0` when everything went right, `1` if `rect` is
// out of bounds and `2` if `arr` is not initialized.
int PHYSICS_get_intersecting_bodies(SDL_Rect rect, dynarr* arr);

// Checks if the body at `body_id` is touching tiles below, setting `check` to
// true if that's the case. Platform tiles don't count while the body is
// dropping down from them. Returns `3` when `body_id` is outside the array's
// range, `1` when there is no body at that index, `0` when everything went
// right.
int PHYSICS_is_body_on_floor(size_t body_id, bool* check);

// Checks if the body at `body_id` is only touching platform tiles below it,
// setting `check` to true if that's the case. Always false while the body is
// dropping down from them. Returns `3` when `body_id` is
// outside the array's range, `1` when there is no body at that index, `0` when
// everything went right.
int PHYSICS_is_body_on_platform(size_t body_id, bool* check);

// Checks if the body at `body_id` is touching tiles above, setting `check` to
// true if that's the case. Returns `3` when `body_id` is outside the array's
// range, `1` when there is no body at that index, `0` when everything went
// right.
int PHYSICS_is_body_on_ceiling(size_t body_id, bool* check);

// Checks if the body at `body_id` is being simulated, setting `check` to true
// if that's the case, which is when any part of it is within the simulation
// space from `CAMERA_get_simulation_rect()` as of the last `PHYSICS_update()`.
// Bodies outside it stay frozen. Returns `3` when `body_id` is outside the
// array's range, `1` when there is no body at that index, `0` when everything
// went right.
int PHYSICS_is_body_within_the_simulation(size_t body_id, bool* check);



// Creates a new point with `data` for its values, returning its index on
// `point_id`. Returns `0` when everything went right, `2` when the point is
// outside the tilemap space.
int PHYSICS_new_prop_point(prop_point data, size_t* point_id);

// Deletes the point at index `point_id` and its connections, then calls its
// `on_delete_func`. Returns `3` when `point_id` is outside the array's range,
// `1` when there is no point at that index, `0` when everything went right.
int PHYSICS_del_prop_point(size_t point_id);

// Gets the point data on `point_id` and returns it on `data`. Returns `3` when
// `point_id` is outside the array's range, `1` when there is no point at
// `point_id`, `0` when everything went right.
int PHYSICS_get_prop_point(size_t point_id, prop_point* data);

// Edits the point at `point_id` with `data`. This also zeroes out the point's
// velocity, use `PHYSICS_set_prop_point_acceleration()`,
// `PHYSICS_set_prop_point_velocity()` or `PHYSICS_set_prop_point_on_delete()`
// to change those without stopping it. Returns `3` when `point_id` is outside
// the array's range, `2` when `data` is invalid, `1` when there is no point at
// that index, `0` when everything went right.
int PHYSICS_edit_prop_point(size_t point_id, prop_point data);

// Sets the constant acceleration of the point at `point_id` to `(acc_x,acc_y)`
// while keeping its velocity. Returns `3` when `point_id` is outside the
// array's range, `1` when there is no point at that index, `0` when everything
// went right.
int PHYSICS_set_prop_point_acceleration(size_t point_id,
                                        double acc_x,
                                        double acc_y);

// Sets the damping of the point at `point_id` to `damping` while keeping its
// velocity. Returns `3` when `point_id` is outside the array's range, `1` when
// there is no point at that index, `0` when everything went right.
int PHYSICS_set_prop_point_damping(size_t point_id, double damping);

// Sets the velocity of the point at `point_id` to `(vel_x,vel_y)`, in pixels
// per second, before damping. Returns `3` when `point_id` is outside the
// array's range, `1` when there is no point at that index, `0` when everything
// went right.
int PHYSICS_set_prop_point_velocity(size_t point_id,
                                    double vel_x,
                                    double vel_y);

// Gets the velocity of the point at `point_id` on `(vel_x,vel_y)`, in pixels
// per second, from how much it moved on its last step, before damping. Returns
// `3` when `point_id` is outside the array's range, `1` when there is no point
// at that index, `0` when everything went right.
int PHYSICS_get_prop_point_velocity(size_t point_id,
                                    double* vel_x,
                                    double* vel_y);

// Connects the points with IDs `id_a` and `id_b` with a connection with
// `length`, `elasticity` and `two_ways` data. `elasticity` is the behaviour
// type, as `0` it will stop the points from going farther than `length` from
// one another, if `two_ways` is `true` it will also stop them from being closer
// than `length`, so an inelastic one-way connection behaves like a rope while
// an inelastic two-ways connection behaves like a stick. An elastic connection
// behaves like a spring with coefficient `1 / elasticity`, and one-way means it
// will only pull them together when stretched while a two-ways one will also
// push them apart when compressed. The IDs must be different, `length` must be
// greater than zero and `elasticity` must be greater than or equal to zero, it
// returns `2` for invalid data otherwise. Returns `3` when either `id_a/b` is
// outside the array's range, `1` when there is no point in either index. Also
// returns `4` when trying to connecto two points that are already connected,
// and `0` when everything went right.
int PHYSICS_connect_prop_points(
    size_t id_a, size_t id_b, double elasticity, double length, bool two_ways);

// Deletes all values in `arr`, a `size_t` dynarr, and puts in the IDs of all
// points connected to the point at `point_id`. Returns `3` when `point_id` is
// outside the array's range, `2` if `arr` is not initialized, `1` when there is
// no point at that index, `0` when everything went right.
int PHYSICS_get_connected_points(size_t point_id, dynarr* arr);

// Sets the `on_delete_func` and `on_delete_arg` of the point at `point_id`
// without touching anything else, unlike `PHYSICS_edit_prop_point()` which
// also resets its velocity. Returns `3` when `point_id` is outside the array's
// range, `1` when there is no point at that index, `0` when everything went
// right.
int PHYSICS_set_prop_point_on_delete(size_t point_id,
                                     void (*func)(size_t point_id, size_t arg),
                                     size_t arg);

// Checks if the point at `point_id` is being simulated, setting `check` to true
// if that's the case, which is when it's within the simulation space from
// `CAMERA_get_simulation_rect()` as of the last `PHYSICS_update()`. Points
// outside it stay frozen and act as pinned for the points they're connected
// to. Returns `3` when `point_id` is outside the array's range, `1` when there
// is no point at that index, `0` when everything went right.
int PHYSICS_is_prop_point_within_the_simulation(size_t point_id, bool* check);



// Initiates the body array. Call at the start of the game.
void PHYSICS_start(void);

// Frees all allocated memory. Call at the end of the game.
void PHYSICS_stop(void);

// Moves each body by its velocity and each prop with its acceleration and
// connections. Bodies collide with the tilemap and interact with fluids and
// props as they move. Only bodies and points within the simulation space from
// `CAMERA_get_simulation_rect()` are simulated, the rest stay frozen.
void PHYSICS_update(void);



#ifdef TEST
// Gets the connection at index `connection_id` and returns its points on
// `point_a` and `point_b` and its values on `elasticity`, `length` and
// `two_ways`, all zeroes if there's none.
void PHYSICS_test_get_connection(size_t connection_id,
                                 size_t* point_a,
                                 size_t* point_b,
                                 double* elasticity,
                                 double* length,
                                 bool* two_ways);
#endif
