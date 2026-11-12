#pragma once
#include "data-structs/hashmap.h"
#include "defs.h"
#include <SDL3/SDL.h>

// Ways an entity can move around the tilemap, as flags, so an entity's `moves`
// can be something like `move_run | move_jump | move_fall | move_drop`. All but
// `move_crawl` are implemented for now. `move_run` goes sideways to the next
// tile on the same floor, `move_jump` jumps from the floor, or out of a fluid
// from its surface, `move_fall` runs off a ledge, `move_drop` drops down from a
// platform, or lets go of a wall or ceiling it's climbing, and `move_swim`
// swims in fluids deep enough to swim in. `move_climb_wall` climbs up and down
// walls and `move_climb_ceiling` along ceilings, both with its run class'
// values, without falling, and around the corners between them and floors.
// `move_fly` flies anywhere in the air, in any direction, like swimming but
// with its fly class' values, staying where it is when it isn't going
// anywhere. Entities that can move otherwise, running, jumping or climbing,
// only fly when they're told to with `ENTITY_fly_to()`, until they land with
// `ENTITY_land_on()`.
//
// ```
// enum entity_move {
//     move_run = 1 << 0,
//     move_jump = 1 << 1,
//     move_fall = 1 << 2,
//     move_drop = 1 << 3,
//     move_swim = 1 << 4,
//     move_crawl = 1 << 5,
//     move_climb_wall = 1 << 6,
//     move_climb_ceiling = 1 << 7,
//     move_fly = 1 << 8,
// };
// ```
typedef enum {
    move_run = 1 << 0,
    move_jump = 1 << 1,
    move_fall = 1 << 2,
    move_drop = 1 << 3,
    move_swim = 1 << 4,
    move_crawl = 1 << 5,
    move_climb_wall = 1 << 6,
    move_climb_ceiling = 1 << 7,
    move_fly = 1 << 8,
} entity_move;

// What a single move of an entity does, while its `entity_move` flags are
// what it's allowed to do: running to the next tile, jumping, falling off a
// ledge, dropping down from a platform, letting go of a wall or ceiling it
// clings to, swimming, climbing a wall or along a ceiling, flying, or landing
// with `ENTITY_land_on()`.
//
// ```
// enum entity_motion {
//     motion_run,
//     motion_jump,
//     motion_fall,
//     motion_drop,
//     motion_let_go,
//     motion_swim,
//     motion_climb_wall,
//     motion_climb_ceiling,
//     motion_fly,
//     motion_land,
// };
// ```
typedef enum {
    motion_run,
    motion_jump,
    motion_fall,
    motion_drop,
    motion_let_go,
    motion_swim,
    motion_climb_wall,
    motion_climb_ceiling,
    motion_fly,
    motion_land,
} entity_motion;

// What an entity that climbs is clinging to: nothing, a wall on its left or on
// its right, or a ceiling.
//
// ```
// enum entity_surface {
//     surface_none,
//     surface_wall_left,
//     surface_wall_right,
//     surface_ceiling,
// };
// ```
typedef enum {
    surface_none,
    surface_wall_left,
    surface_wall_right,
    surface_ceiling,
} entity_surface;

// Classes of values entities move with, from `ENTITY_CLASSES_CSV_PATH`, so
// entities in the same classes share their trajectories. Jump classes go from
// the weakest to the strongest, with a `jump_speed`. Fall classes have a
// `gravity` and a `max_fall_speed`. Run classes have a `max_speed`, an
// `acceleration` and a `friction`, the same as the player's movement on the
// floor. Swim classes have a `max_speed`, an `acceleration` and a `friction`,
// used instead of the others while the entity swims, which it does in any
// direction without sinking, and a `jump_speed` to jump out of a fluid's
// surface with. Entities that can't swim move in fluids deep enough to swim
// in with their other values multiplied by the `ENTITY_UNDERFLUID_` factors.
// Fly classes have a `max_speed`, an `acceleration` and a `friction`, used
// instead of the others while the entity flies.
// Size classes are the `width` and `height` of the entity's body.
//
// ```
// enum jump_class { ENTITY_JUMP_CLASSES };
// enum fall_class { ENTITY_FALL_CLASSES };
// enum run_class { ENTITY_RUN_CLASSES };
// enum swim_class { ENTITY_SWIM_CLASSES };
// enum fly_class { ENTITY_FLY_CLASSES };
// enum size_class { ENTITY_SIZE_CLASSES };
// ```
typedef enum { ENTITY_JUMP_CLASSES } jump_class;
typedef enum { ENTITY_FALL_CLASSES } fall_class;
typedef enum { ENTITY_RUN_CLASSES } run_class;
typedef enum { ENTITY_SWIM_CLASSES } swim_class;
typedef enum { ENTITY_FLY_CLASSES } fly_class;
typedef enum { ENTITY_SIZE_CLASSES } size_class;

// How a move ended: `move_completed` when it got to the node it was going to,
// `move_missed` when it ended up on another one or nowhere it can stand, like
// when it landed somewhere else than where it was predicted to, and
// `move_interrupted` when it took longer than `ENTITY_MOVE_MAX_TIME` or
// `ENTITY_interrupt_movement()` interrupted it.
//
// ```
// enum entity_move_end {
//     move_completed,
//     move_missed,
//     move_interrupted,
// };
// ```
typedef enum {
    move_completed,
    move_missed,
    move_interrupted,
} entity_move_end;

// What a move did, given to an entity's `on_move_finished_func`. `move` is
// what the move did and `end` how it ended. `start_x,start_y` is where the
// body was and `start_vx,start_vy` its velocity when it started, from the node
// `from_x,from_y`, going to the node `to_x,to_y`. When it ended on a node,
// `on_node` is set and `end_x,end_y` is that node. `reaches_goal` is whether
// the path it was found on reaches where `ENTITY_move_towards()` was told to
// go, otherwise it only gets closer.
//
// ```
// struct entity_move_result {
//     entity_motion move;
//     entity_move_end end;
//     double start_x, start_y, start_vx, start_vy;
//     Uint16 from_x, from_y;
//     Uint16 to_x, to_y;
//     bool on_node;
//     Uint16 end_x, end_y;
//     bool reaches_goal;
// };
// ```
typedef struct {
    entity_motion move;
    entity_move_end end;
    double start_x, start_y, start_vx, start_vy;
    Uint16 from_x, from_y;
    Uint16 to_x, to_y;
    bool on_node;
    Uint16 end_x, end_y;
    bool reaches_goal;
} entity_move_result;

// An entity moves its body around the tilemap by itself, one move at a time,
// each found with `ENTITY_move_towards()`, so whatever owns it decides what it
// does after each one. It moves its body at
// `body_id` with the values of its classes, jumping with its `jump` class or
// any weaker one. Its body must be the size of its `size` class. `moves` are
// its `entity_move` flags of how it's allowed to move. The body becomes the
// entity's, deleted along with it.
//
// With `move_swim`, it swims when its body is in a fluid deep enough, like the
// player, whatever the fluid, moving in any direction and staying where it is
// when it isn't going anywhere. Otherwise, it sinks in them, moving on their
// bottom slower. `safe_fluids` has the bit `1 << type` set for each
// `fluid_type` it can go in, and it never goes where there are any of the
// others on purpose.
//
// `on_create_func` is called with its ID right after it's created,
// `on_destroy_func` right before it's destroyed, while it still exists,
// `update_func` each tick before it moves, but only while its body is within
// the simulation space, and `on_move_finished_func` when a move ends, with what
// it did. Moves end during updates, so `on_move_finished_func` can start the
// next one on the same tick the last one ended, like jumping again the tick it
// lands, or when interrupted. Any of them can be `NULL`. They're free to
// create, edit and delete entities, and `update_func` and
// `on_move_finished_func` can delete their own, but `on_destroy_func` can't,
// since it's already being deleted. `state` is for
// whatever created it to keep its own data on, like memory allocated on
// creation and freed on destruction.
//
// ```
// struct entity {
//     size_t body_id;
//     Uint32 moves;
//     Uint32 safe_fluids;
//     jump_class jump;
//     fall_class fall;
//     run_class run;
//     swim_class swim;
//     fly_class fly;
//     size_class size;
//     void (*on_create_func)(size_t entity_id);
//     void (*on_destroy_func)(size_t entity_id);
//     void (*update_func)(size_t entity_id);
//     void (*on_move_finished_func)(size_t entity_id,
//                                   entity_move_result result);
//     void* state;
// };
// ```
typedef struct {
    size_t body_id;
    Uint32 moves;
    Uint32 safe_fluids;
    jump_class jump;
    fall_class fall;
    run_class run;
    swim_class swim;
    fly_class fly;
    size_class size;
    void (*on_create_func)(size_t entity_id);
    void (*on_destroy_func)(size_t entity_id);
    void (*update_func)(size_t entity_id);
    void (*on_move_finished_func)(size_t entity_id, entity_move_result result);
    void* state;
} entity;



// Creates a new entity with `data` for its values, returning its index on
// `entity_id`, then calls its `on_create_func`. Its `body_id` must be of an
// existing body the size of its `size` class, and its classes must exist.
// Returns `2` when `data` is invalid, `0` when everything went right.
int ENTITY_new_entity(entity data, size_t* entity_id);

// Calls the `on_destroy_func` of the entity at `entity_id`, then deletes it
// along with its body. Returns `3` when `entity_id` is outside the array's
// range, `1` when there is no entity at that index, `0` when everything went
// right.
int ENTITY_del_entity(size_t entity_id);

// Gets the entity at `entity_id` and returns it on `data`. Returns `3` when
// `entity_id` is outside the array's range, `1` when there is no entity at that
// index, `0` when everything went right.
int ENTITY_get_entity(size_t entity_id, entity* data);

// Edits the entity at `entity_id` with `data`, which must be valid like on
// `ENTITY_new_entity()`. A move it's in the middle of goes on, but the next one
// is found again with its new values. Returns `3` when `entity_id` is outside
// the array's range, `2` when `data` is invalid, `1` when there is no entity at
// that index, `0` when everything went right.
int ENTITY_edit_entity(size_t entity_id, entity data);

// Makes the entity at `entity_id` start the first move of a path towards
// `(x,y)`, in pixels, which is where the bottom center of its body should end
// up, or the closest it can find to it. Its `on_move_finished_func` is called
// once the move ends. It looks for the path from wherever its body is, even in
// a fluid it avoids, but only moves that get out of it. Paths are kept, so
// moving towards the same place again after a move completed only checks the
// next move is still the same. Returns `3` when `entity_id` is outside the
// array's range, `1` when there is no entity at that index, `4` when it's in
// the middle of a move, `5` when it isn't on a node it can find a path from,
// like when it's in the air or outside the simulation space, `6` when it's
// already there, `7` when it's out of reach, since it can't get any closer,
// and `0` when it started a move. Entities that can only fly fly there, even
// after landing, but entities that can also move otherwise only move
// otherwise, so they can't while they're flying, until they land.
int ENTITY_move_towards(size_t entity_id, double x, double y);

// Same as `ENTITY_move_towards()`, but going through fluids the entity avoids
// like it does through any other, so it can swim, land and go on in them.
int ENTITY_move_towards_ignoring_unsafe_fluids(size_t entity_id,
                                               double x,
                                               double y);

// Same as `ENTITY_move_towards()`, but flying, which the entity at `entity_id`
// keeps doing until it lands with `ENTITY_land_on()`. It starts flying right
// away, even when it's already there or can't get any closer. Returns `2`
// when it can't fly.
int ENTITY_fly_to(size_t entity_id, double x, double y);

// Makes the entity at `entity_id` jump right away with the class `jump`,
// from the floor or swimming, to land on the tile at `(x,y)`, in pixels, which
// is where the bottom center of its body should end up. It holds the direction
// towards it for as long as it takes to land as close as it can to it, ignoring
// fluids it avoids, since it's up to whoever calls it where it lands. It's a
// move like any other, so its `on_move_finished_func` is called once it lands,
// as completed if it landed on that tile, otherwise missed. Returns `3` when
// `entity_id` is outside the array's range, `1` when there is no entity at
// that index, `2` when `jump` isn't a jump class, `4` when it's in the middle
// of a move, `5` when it's neither on the floor nor swimming, `7` when it
// can't land on that tile from where it is, and `0` when it jumped.
int ENTITY_jump(size_t entity_id, jump_class jump, double x, double y);

// Makes the entity at `entity_id` land on the tile at `(x,y)`, in pixels, like
// the bottom center of where its body should end up on `ENTITY_move_towards()`.
// It lands on its floor or, when it has none, on a wall beside it, preferring
// its right, if it climbs walls, or on its ceiling, if it climbs ceilings.
// Like `ENTITY_move_towards()`, it starts a single move, of the path towards it
// until it's there, flying if it's flying, then of `motion_land`, getting to
// where it lands. Once it landed, it isn't flying anymore, so it stays on the
// floor, or clings to the wall or ceiling, until its next move, which only
// flies again if it can't move otherwise, or if it's `ENTITY_fly_to()`.
// Returns the same as
// `ENTITY_move_towards()`, with `6` meaning it already landed there, and `2`
// when there's nothing it can land on that tile.
int ENTITY_land_on(size_t entity_id, double x, double y);

// Interrupts the move the entity at `entity_id` is in the middle of, even in
// the air, letting go of whatever it was doing so only the physics move it,
// and calls its `on_move_finished_func`. Returns `3` when `entity_id` is
// outside the array's range, `1` when there is no entity at that index, `4`
// when it isn't moving, `0` when everything went right.
int ENTITY_interrupt_movement(size_t entity_id);

// Sets `moving` to whether the entity at `entity_id` is in the middle of a
// move. Returns `3` when `entity_id` is outside the array's range, `1` when
// there is no entity at that index, `0` when everything went right.
int ENTITY_is_moving(size_t entity_id, bool* moving);

// Sets `flying` to whether the entity at `entity_id` is flying. Returns `3`
// when `entity_id` is outside the array's range, `1` when there is no entity
// at that index, `0` when everything went right.
int ENTITY_is_flying(size_t entity_id, bool* flying);

// Gets the node, a tile, the body of the entity at `entity_id` is standing on
// or swimming in on `x` and `y`, even one with fluids it avoids. Returns `3`
// when `entity_id` is outside the array's range, `1` when there is no entity
// at that index, `4` when it isn't on any, like in the air, `0` when
// everything went right.
int ENTITY_get_node(size_t entity_id, Uint16* x, Uint16* y);

// Sets `surface` to what the entity at `entity_id` is clinging to, which is
// `surface_none` unless it climbs and is on a wall or ceiling. Returns `3` when
// `entity_id` is outside the array's range, `1` when there is no entity at
// that index, `0` when everything went right.
int ENTITY_get_clinging(size_t entity_id, entity_surface* surface);

// Sets `swimming` to whether the body of the entity at `entity_id` is deep
// enough in a fluid to swim, and `unsafe` to whether it touches any fluid it
// avoids. Returns `3` when `entity_id` is outside the array's range, `1` when
// there is no entity at that index, `0` when everything went right.
int ENTITY_get_fluids(size_t entity_id, bool* swimming, bool* unsafe);

// Finds the nearest node, a tile, the entity at `entity_id` can get to that
// has no fluids it avoids, where it can stand or swim, and gets it on `x` and
// `y`. That's the node it's on if it's safe already. Otherwise, it finds the
// path to it with every move the entity has, going through fluids it avoids,
// like `ENTITY_move_towards_ignoring_unsafe_fluids()`. It doesn't move there,
// it's up to whoever calls it, but the path is kept, so moving there with
// `ENTITY_move_towards_ignoring_unsafe_fluids()` right after doesn't find it
// again. When it can't get to any, it gets the first tile right above it
// without any fluid instead, like above a fluid's surface. Returns `3` when
// `entity_id` is outside the array's range, `1` when there is no entity at
// that index, `4` when it's in the middle of a move and isn't safe, `5` when
// it isn't on a node it can find a path from, `6` when it can't get to any
// safe one and got the tile above it, `7` when there's no tile above it
// without fluids it fits in either, like when it's walled in, and `0` when it
// found one.
int ENTITY_find_nearest_safe_tile(size_t entity_id, Uint16* x, Uint16* y);

// Gets the `width` and `height` of the size class `size` on `w` and `h`, the
// size the bodies of entities in it must have.
void ENTITY_get_size(size_class size, Uint8* w, Uint8* h);

// Gets on `px,py` the bottom center of the tile `(x,y)`, in pixels, which is
// what moving entities to it takes, like with `ENTITY_move_towards()`, for
// tiles like the ones `ENTITY_get_node()` and
// `ENTITY_find_nearest_safe_tile()` get.
void ENTITY_get_tile_target(Uint16 x, Uint16 y, double* px, double* py);

// Registers that entities in the classes of `classes`, moving with its
// `moves`, will be created, so `ENTITY_generate_trajectories()` generates
// their trajectories, which only jumps, falls, drops and letting go have.
// Each kind of entity should call it once when it starts, after
// `ENTITY_start()`.
void ENTITY_register_classes(entity classes);

// Generates the trajectories of every move of every combination of classes
// registered with `ENTITY_register_classes()`. Otherwise, each combination's
// trajectories are generated the first time an entity with it looks for a
// path, which makes the game start faster on development but stutter the first
// time each combination moves, which is also what happens to combinations
// that weren't registered. Call once, after every kind of entity registered
// its classes.
void ENTITY_generate_trajectories(void);

// Maps of the names of each class, like on `ENTITY_CLASSES_CSV_PATH` without
// their prefix, to the class. Deinit them when done.
hashmap ENTITY_get_string_jump_classes_map(void);
hashmap ENTITY_get_string_fall_classes_map(void);
hashmap ENTITY_get_string_run_classes_map(void);
hashmap ENTITY_get_string_swim_classes_map(void);
hashmap ENTITY_get_string_fly_classes_map(void);
hashmap ENTITY_get_string_size_classes_map(void);



// Loads the classes and allocates memory. Call at the start of the game, after
// `PHYSICS_start()`.
void ENTITY_start(void);

// Destroys every entity left and frees all allocated memory. Call when
// quitting the game, before `PHYSICS_stop()` and before stopping anything the
// entities' `on_destroy_func` use.
void ENTITY_stop(void);

// Calls the `update_func` of each entity whose body is within the simulation
// space from `CAMERA_get_simulation_rect()`, then moves them along their paths.
// Call on the fixed thread, after `PHYSICS_update()`.
void ENTITY_update(void);



#ifdef TEST
// Generates the trajectories of every start of the entities in `classes`'
// classes, dry and submerged, with their jump class, and of letting go of
// every surface, once keeping every step
// and once only checkpoints, then compares every step, getting them from the
// checkpoints like when following them. Puts how many it compared on
// `checked` and returns how many weren't exactly the same. `classes` can't
// swim.
size_t ENTITY_test_check_replays(entity classes, size_t* checked);

// Puts on `to_x`, `to_y` and `costs` up to `max` moves the entity at
// `entity_id` can do from the tile `(x,y)`, returning how many. If `known`,
// its body got there `offset_x` pixels off from its center going `vx`,
// otherwise it starts from its center. With `whole`, they're simulated whole
// instead of following their trajectories, so both can be compared.
size_t ENTITY_test_get_moves(size_t entity_id,
                             Uint16 x,
                             Uint16 y,
                             bool known,
                             double offset_x,
                             double vx,
                             bool whole,
                             Uint16* to_x,
                             Uint16* to_y,
                             double* costs,
                             size_t max);
#endif
