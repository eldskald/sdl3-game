#include "engine/entity.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/physics.h"
#include "engine/tilemap.h"
#include "tests.h"
#include "tests/entity.h"
#include <SDL3/SDL.h>
#include <stdio.h>

#define MAX_TICKS 900
#define MAX_MOVES 256

static size_t created_id = 0;
static size_t create_calls = 0;
static size_t destroy_calls = 0;
static size_t update_calls = 0;

static void on_create(size_t id) {
    created_id = id;
    create_calls++;
}

static void on_destroy(size_t _id) { // NOLINT
    destroy_calls++;
}

static void on_update(size_t _id) { // NOLINT
    update_calls++;
}

// How entities moved with `go_to()` are told to move there.
typedef enum {
    order_move,
    order_move_ignoring_unsafe,
    order_land,
    order_fly,
} owner_order;

// Where entities moved with `go_to()` are going, in pixels, how they're told
// to go there, whether they keep moving towards it after each move, what the
// order returned the first and the last time and the last move that finished.
static double goal_px = 0.0;
static double goal_py = 0.0;
static owner_order order = order_move;
static bool keep_moving = false;
static int first_moved = 0;
static int last_moved = 0;
static size_t finished_calls = 0;
static entity_move_result last_result = (entity_move_result){0};

// Moves the entity `id` towards where `go_to()` set, with `order`.
static int move_towards(size_t id) {
    switch (order) {
    case order_move_ignoring_unsafe:
        return ENTITY_move_towards_ignoring_unsafe_fluids(id, goal_px, goal_py);
    case order_land:
        return ENTITY_land_on(id, goal_px, goal_py);
    case order_fly:
        return ENTITY_fly_to(id, goal_px, goal_py);
    case order_move:
        break;
    }
    return ENTITY_move_towards(id, goal_px, goal_py);
}

static void on_move_finished(size_t id, entity_move_result result) {
    finished_calls++;
    last_result = result;
    if (keep_moving) last_moved = move_towards(id);
}

// Makes the entity `id` move towards the tile `(gx,gy)`, which is where the
// bottom center of its body should be, and keep moving towards it after each
// move, like its owner would. Returns what `ENTITY_move_towards()` did.
static int go_to(size_t id, int gx, int gy) {
    goal_px = gx * SPRITESHEET_CELL_X + SPRITESHEET_CELL_X / 2.0;
    goal_py = (gy + 1) * SPRITESHEET_CELL_Y;
    keep_moving = true;
    last_moved = move_towards(id);
    first_moved = last_moved;
    return last_moved;
}

// Ticks the physics and entities once. An entity moved with `go_to()` that
// couldn't move from where it was, like in the air, tries again once it
// stopped moving.
static void tick(size_t id) {
    PHYSICS_update();
    ENTITY_update();
    bool moving = true;
    ENTITY_is_moving(id, &moving);
    if (keep_moving && !moving && last_moved == 5)
        last_moved = move_towards(id);
}

// Makes the entity `id` go to the tile `(gx,gy)` with `how`, like its owner
// would, ticking until its last order didn't start a move and it stopped
// moving, other than when it couldn't move from where it was, which it tries
// again, or until `max_ticks` went by. After each tick, `each_tick` is called
// with `id` and `arg`, if it isn't `NULL`. Returns how many ticks it took.
static int run_order(size_t id,
                     int gx,
                     int gy,
                     owner_order how,
                     int max_ticks,
                     void (*each_tick)(size_t id, void* arg),
                     void* arg) {
    CAMERA_center_at(gx * SPRITESHEET_CELL_X, gy * SPRITESHEET_CELL_Y);
    order = how;
    go_to(id, gx, gy);
    int t = 0;
    for (; t < max_ticks; t++) {
        tick(id);
        if (each_tick) each_tick(id, arg);
        bool moving = true;
        ENTITY_is_moving(id, &moving);
        if (!moving && last_moved != 0 && last_moved != 5) break;
    }
    keep_moving = false;
    order = order_move;
    return t;
}

// Ticks until the body of the entity `id` stops moving sideways, up to a
// second, like when it slides to a stop after its last move.
static void settle(size_t id) {
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    for (int t = 0; t < FIXED_T_TARGET_FPS; t++) {
        body b = (body){0};
        PHYSICS_get_body(e.body_id, &b);
        if (SDL_fabs(b.vel_x) <= EPSILON) return;
        tick(id);
    }
}

// Fills the tiles from `(x,y)` to `(x + w - 1, y + h - 1)` with `type`.
static void fill(int x, int y, int w, int h, tile_type type, bool platform) {
    for (int i = 0; i < w; i++)
        for (int j = 0; j < h; j++) {
            tile t[1][1] = {{(tile){.type = type, .platform = platform}}};
            TILEMAP_set_at(x + i, y + j, 1, 1, &t);
        }
}

// A body of `size` standing on the floor of tile `(x,y)`.
static size_t new_body_at(int x, int y, size_class size) {
    Uint8 w = 0;
    Uint8 h = 0;
    ENTITY_get_size(size, &w, &h);
    size_t body_id = 0;
    PHYSICS_new_body(
        (body){
            .type = physics_actor,
            .x = x * SPRITESHEET_CELL_X + (SPRITESHEET_CELL_X - w) / 2.0,
            .y = (y + 1) * SPRITESHEET_CELL_Y - h,
            .w = w,
            .h = h,
        },
        &body_id);
    return body_id;
}

// An entity in `classes` with its body standing on the floor of tile `(x,y)`,
// with the moves of `classes`, or every one but swimming when it has none,
// calling `on_move_finished()` when its moves finish.
static size_t new_entity_with(int x, int y, entity classes) {
    classes.body_id = new_body_at(x, y, classes.size);
    classes.on_move_finished_func = on_move_finished;
    if (!classes.moves)
        classes.moves = move_run | move_jump | move_fall | move_drop;
    size_t id = 0;
    ENTITY_new_entity(classes, &id);
    return id;
}

// Classes moving like the player.
static const entity player_like = (entity){
    .jump = jump_class_high,
    .fall = fall_class_normal,
    .run = run_class_normal,
    .size = size_class_normal,
};

// An entity moving like the player, with its body standing on the floor of
// tile `(x,y)`.
static size_t new_entity_at(int x, int y) {
    return new_entity_with(x, y, player_like);
}

// Tile the bottom center of the entity's body is on.
static void get_tile(size_t id, int* x, int* y, body* b) {
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    PHYSICS_get_body(e.body_id, b);
    *x = (int)SDL_floor((b->x + b->w / 2.0) / SPRITESHEET_CELL_X);
    *y = (int)SDL_floor((b->y + b->h - 1.0) / SPRITESHEET_CELL_Y);
}

// Moves an entity in `classes` from the tile `(sx,sy)` to `(gx,gy)` on
// whatever level was built, ticking until it gets there and stops, checking it
// does.
static void expect_reach_with(const char* desc,
                              entity classes,
                              int sx,
                              int sy,
                              int gx,
                              int gy,
                              int max_ticks) {
    size_t id = new_entity_with(sx, sy, classes);
    int ticks = run_order(id, gx, gy, order_move, max_ticks, NULL, NULL);
    settle(id);
    int x = sx;
    int y = sy;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    bool on_floor = false;
    PHYSICS_is_body_on_floor(e.body_id, &on_floor);
    ENTITY_del_entity(id);
    report r = (report){0};
    check(&r,
          x == gx && y == gy && on_floor,
          "Ended on tile (%d,%d) after %d ticks, expected (%d,%d)",
          x,
          y,
          ticks,
          gx,
          gy);
    finish_report(desc, &r);
}

static void
expect_reach(const char* desc, int sx, int sy, int gx, int gy, int max_ticks) {
    expect_reach_with(desc, player_like, sx, sy, gx, gy, max_ticks);
}

// A room with walls from tile 0 to 60 and a floor on row 30.
static void build_room(void) {
    TILEMAP_clear();
    fill(0, 0, 61, 1, tile_stone, false);
    fill(0, 0, 1, 31, tile_stone, false);
    fill(60, 0, 1, 31, tile_stone, false);
    fill(0, 30, 61, 1, tile_stone, false);
}

// Makes an entity that only jumps go from the tile `(sx,sy)` to `(gx,gy)`,
// checking it gets there without walking, which is speeding up on the floor
// for more than a couple of ticks in a row, other than jumping off it. It
// starts each jump from wherever the one before landed, going however fast it
// was, so it only ever speeds up on the floor for the few ticks it takes to
// get to where a running jump starts, when it landed a little behind it.
// How many ticks in a row an entity sped up on the floor, other than jumping
// off it, the most it did on `walked`, see `track_walking()`.
typedef struct {
    bool on_floor;
    double last_vx;
    int walking, walked;
} walking_track;

static void track_walking(size_t id, void* arg) {
    walking_track* w = arg;
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    bool was_on_floor = w->on_floor;
    PHYSICS_is_body_on_floor(e.body_id, &w->on_floor);
    if (w->on_floor && was_on_floor && b.vel_y >= 0.0 &&
        SDL_fabs(b.vel_x) > SDL_fabs(w->last_vx) + EPSILON) {
        w->walking++;
        w->walked = SDL_max(w->walked, w->walking);
    } else
        w->walking = 0;
    w->last_vx = b.vel_x;
}

static void
expect_hops(const char* desc, int sx, int sy, int gx, int gy, int max_ticks) {
    entity classes = player_like;
    classes.moves = move_jump;
    size_t id = new_entity_with(sx, sy, classes);
    walking_track w = (walking_track){0};
    int ticks = run_order(id, gx, gy, order_move, max_ticks, track_walking, &w);
    settle(id);
    int x = sx;
    int y = sy;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    ENTITY_del_entity(id);
    report r = (report){0};
    check(&r,
          x == gx && y == gy && w.on_floor,
          "Ended on tile (%d,%d) after %d ticks, expected (%d,%d)",
          x,
          y,
          ticks,
          gx,
          gy);
    check(&r, w.walked <= 2, "Walked for %d ticks in a row", w.walked);
    finish_report(desc, &r);
}

static void test_moves(void) {
    build_room();
    expect_reach(
        "Made an entity go to a tile on the same floor, checking if it "
        "ran there...",
        5,
        29,
        20,
        29,
        MAX_TICKS);

    build_room();
    fill(20, 29, 10, 1, tile_stone, false);
    expect_reach("Made an entity go on top of a step a tile tall, checking if "
                 "it jumped up there...",
                 5,
                 29,
                 25,
                 28,
                 MAX_TICKS);

    build_room();
    fill(20, 28, 10, 2, tile_stone, false);
    expect_reach("Made an entity go on top of a step two tiles tall, checking "
                 "if it jumped up there...",
                 5,
                 29,
                 25,
                 27,
                 MAX_TICKS);

    // The floor gets a pit 2 tiles wide and 15 deep, which it can't get out of
    build_room();
    fill(0, 30, 61, 16, tile_stone, false);
    fill(18, 30, 2, 15, 0, false);
    expect_reach("Made an entity go across a pit it can't climb out of, "
                 "checking if it jumped over it...",
                 5,
                 29,
                 30,
                 29,
                 MAX_TICKS);

    build_room();
    fill(1, 24, 20, 6, tile_stone, false);
    expect_reach("Made an entity go down from a ledge, checking if it fell "
                 "down from it...",
                 5,
                 23,
                 40,
                 29,
                 MAX_TICKS);

    // The platform goes from wall to wall, so dropping is the only way down
    build_room();
    fill(1, 24, 59, 1, tile_wood, true);
    expect_reach("Made an entity go below the platform it's on, checking if it "
                 "dropped down from it...",
                 5,
                 23,
                 30,
                 29,
                 MAX_TICKS);

    // Back up there, now jumping up through it
    build_room();
    fill(1, 24, 59, 1, tile_wood, true);
    fill(1, 26, 59, 1, tile_wood, true);
    fill(1, 28, 59, 1, tile_wood, true);
    expect_reach("Made an entity go up through stacked platforms, checking if "
                 "it jumped up through them...",
                 5,
                 29,
                 30,
                 23,
                 MAX_TICKS);

    build_room();
    expect_hops("Made an entity that only jumps go far on the same floor, "
                "checking if it hopped there without walking...",
                5,
                29,
                40,
                29,
                MAX_TICKS);

    build_room();
    fill(14, 29, 3, 1, tile_stone, false);
    fill(24, 28, 3, 2, tile_stone, false);
    fill(34, 29, 6, 1, tile_stone, false);
    expect_hops("Made it hop over steps going up and down, checking if it got "
                "there without walking...",
                5,
                29,
                45,
                29,
                MAX_TICKS);

    // A tower too tall to jump on, so it goes as close as it can and stops
    build_room();
    fill(30, 24, 3, 6, tile_stone, false);
    size_t id = new_entity_at(5, 29);
    run_order(id, 31, 23, order_move, MAX_TICKS, NULL, NULL);
    settle(id);
    int x = 0;
    int y = 0;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    ENTITY_del_entity(id);
    expect("Made an entity go on top of a tower too tall to jump on, checking "
           "if it got as close as it could, stopped and was out of reach...",
           4,
           (bool[]){
               x == 29,
               y == 29,
               SDL_fabs(b.vel_x) <= EPSILON,
               last_moved == 7,
           });
}

// A room like `build_room()`, with ledges on both sides of a pool of `type`
// from tile `(20,25)` to `(39,29)`, full to the brim, which is the ledges'
// floor. It's too wide to jump over.
static void build_pool(fluid_type type) {
    build_room();
    fill(1, 25, 19, 5, tile_stone, false);
    fill(40, 25, 20, 5, tile_stone, false);
    CAMERA_center_at(30 * SPRITESHEET_CELL_X, 25 * SPRITESHEET_CELL_Y);
    for (fluid_type t = 0; t < FLUIDS_TOTAL; t++)
        FLUIDS_clear_at(0, 0, 62 * FLUIDS_SPLIT_X, 32 * FLUIDS_SPLIT_Y, t);
    FLUIDS_set_at(20 * FLUIDS_SPLIT_X,
                  25 * FLUIDS_SPLIT_Y,
                  20 * FLUIDS_SPLIT_X,
                  5 * FLUIDS_SPLIT_Y,
                  type,
                  1.0);
    FLUIDS_update();
}

// Makes an entity in `classes` go from the left ledge of `build_pool()` to the
// right one, checking it ends up as close as it gets on the left one, at its
// edge, without ever swimming.
// Sets the `bool` at `arg` if the entity `id` is deep enough in a fluid to
// swim.
static void track_swimming(size_t id, void* arg) {
    bool swimming = false;
    bool unsafe = false;
    ENTITY_get_fluids(id, &swimming, &unsafe);
    *(bool*)arg = *(bool*)arg || swimming;
}

static void expect_stay_out(const char* desc, entity classes) {
    size_t id = new_entity_with(5, 24, classes);
    bool swam = false;
    run_order(id, 50, 24, order_move, MAX_TICKS, track_swimming, &swam);
    settle(id);
    int x = 0;
    int y = 0;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    ENTITY_del_entity(id);
    report r = (report){0};
    check(&r,
          x == 19 && y == 24 && SDL_fabs(b.vel_x) <= EPSILON,
          "Ended on tile (%d,%d) going at %f, expected to stop at (19,24)",
          x,
          y,
          b.vel_x);
    check(&r, !swam, "It swam");
    finish_report(desc, &r);
}

// The fastest an entity went sideways underwater, once it had been for half a
// second, see `track_underwater()`.
typedef struct {
    int ticks;
    double fastest;
} underwater_track;

static void track_underwater(size_t id, void* arg) {
    underwater_track* u = arg;
    bool swimming = false;
    bool unsafe = false;
    ENTITY_get_fluids(id, &swimming, &unsafe);
    u->ticks = swimming ? u->ticks + 1 : 0;
    if (u->ticks <= FIXED_T_TARGET_FPS / 2) return;
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    u->fastest = SDL_max(u->fastest, SDL_fabs(b.vel_x));
}

static void test_swimming(void) {
    entity swimmer = player_like;
    swimmer.moves = move_run | move_jump | move_fall | move_drop | move_swim;
    swimmer.safe_fluids = 1 << fluid_water;

    build_pool(fluid_water);
    expect_reach_with("Made a swimming entity go across a pool of water too "
                      "wide to jump over, checking if it swam across and "
                      "jumped out of it...",
                      swimmer,
                      5,
                      24,
                      50,
                      24,
                      MAX_TICKS);

    build_pool(fluid_water);
    expect_reach_with("Made a swimming entity go to the bottom of a pool of "
                      "water, checking if it swam down there...",
                      swimmer,
                      5,
                      24,
                      30,
                      29,
                      MAX_TICKS);

    build_pool(fluid_water);
    expect_reach_with("Made a swimming entity at the bottom of a pool of water "
                      "go out of it, checking if it swam up and jumped out...",
                      swimmer,
                      30,
                      29,
                      50,
                      24,
                      MAX_TICKS);

    build_pool(fluid_water);
    expect_stay_out("Made an entity that can't swim go across a pool of water, "
                    "checking if it stayed out of it...",
                    player_like);

    build_pool(fluid_lava);
    expect_stay_out("Made a swimming entity that only goes in water go across "
                    "a pool of lava, checking if it stayed out of it...",
                    swimmer);

    // Left alone in the middle of the water, after a few ticks to settle
    build_pool(fluid_water);
    size_t id = new_entity_with(30, 27, swimmer);
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    for (int t = 0; t < 10; t++)
        tick(id);
    body before = (body){0};
    PHYSICS_get_body(e.body_id, &before);
    for (int t = 0; t < 120; t++)
        tick(id);
    body after = (body){0};
    PHYSICS_get_body(e.body_id, &after);
    bool swimming = false;
    bool unsafe = false;
    ENTITY_get_fluids(id, &swimming, &unsafe);
    ENTITY_del_entity(id);
    expect("Left a swimming entity alone in the middle of a pool of water, "
           "checking if it stayed where it was instead of sinking...",
           3,
           (bool[]){
               swimming,
               SDL_fabs(after.y - before.y) <= 1.0,
               SDL_fabs(after.vel_y) <= EPSILON,
           });

    // Walking into the water and along its bottom, slower than on land once
    // it lost the speed it went in with, and sliding further, with less
    // friction
    entity walker = player_like;
    walker.safe_fluids = 1 << fluid_water;
    build_pool(fluid_water);
    id = new_entity_with(5, 24, walker);
    underwater_track under = (underwater_track){0};
    run_order(id, 35, 29, order_move, 2 * MAX_TICKS, track_underwater, &under);
    settle(id);
    int x = 5;
    int y = 24;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    double fastest_under = under.fastest;
    ENTITY_del_entity(id);
    report r = (report){0};
    check(&r,
          SDL_abs(x - 35) <= 1 && y == 29,
          "Ended on tile (%d,%d), expected (35,29) or beside it",
          x,
          y);
    check(&r,
          fastest_under > 0.0 &&
              fastest_under <=
                  60.0 * ENTITY_UNDERFLUID_MAX_SPEED_FACTOR + EPSILON,
          "Went at most %f underwater, once it had been for a while",
          fastest_under);
    finish_report("Made an entity that can't swim, but can go in water, go to "
                  "the bottom of a pool of it, checking if it walked there, "
                  "slower underwater, sliding at most a tile past it...",
                  &r);

    for (fluid_type t = 0; t < FLUIDS_TOTAL; t++)
        FLUIDS_clear_at(0, 0, 62 * FLUIDS_SPLIT_X, 32 * FLUIDS_SPLIT_Y, t);
    FLUIDS_update();
}

#ifdef TEST
// Fills the room from tile `(1,1)` to `(58,29)` randomly with solid tiles and
// platforms, seeded with `seed`.
static void build_random_room(Uint64 seed) {
    build_room();
    Uint64 state = seed;
    for (int x = 1; x < 60; x++) {
        for (int y = 1; y < 30; y++) {
            Sint32 r = SDL_rand_r(&state, 100);
            if (r < 18)
                fill(x, y, 1, 1, tile_stone, false);
            else if (r < 26)
                fill(x, y, 1, 1, tile_wood, true);
        }
    }
}

// Whether a body no bigger than a tile can stand on tile `(x,y)`, where it's
// not solid and has floor under it.
static bool is_standable(int x, int y) {
    tile at = (tile){0};
    tile under = (tile){0};
    TILEMAP_get_at(x, y, &at);
    TILEMAP_get_at(x, y + 1, &under);
    return (!at.type || at.platform) && under.type;
}

static bool is_wall(int x, int y) {
    tile t = (tile){0};
    TILEMAP_get_at(x, y, &t);
    return t.type && !t.platform;
}

// Whether a body no bigger than a tile can cling on tile `(x,y)` to a wall
// beside it or a ceiling over it, without floor under it.
static bool is_clingable(int x, int y) {
    tile at = (tile){0};
    TILEMAP_get_at(x, y, &at);
    if (at.type || is_standable(x, y)) return false;
    return is_wall(x - 1, y) || is_wall(x + 1, y) || is_wall(x, y - 1);
}

// Compares the moves an entity in `classes` finds following its trajectories
// with the ones it finds simulating them whole, from every tile it could stand
// on of random rooms. They can't always be the same: trajectories are made far
// from where they're followed, and the physics round positions to pixels, so
// the tiny differences adding up positions far apart makes can round a
// position that's exactly half a pixel the other way, landing a tick earlier
// or later, or rarely somewhere else. Moving entities are never exactly where
// they were simulated anyway. So it checks that nearly all of them land on the
// same tile at most a tick apart. Each tile is started from its center or,
// picked at random, from somewhere else on it going some speed, so moves start
// from every start offset and speed.
// Fills the room of `build_random_room()` with water from row `top` down.
static void flood(int top) {
    for (fluid_type t = 0; t < FLUIDS_TOTAL; t++)
        FLUIDS_clear_at(0, 0, 62 * FLUIDS_SPLIT_X, 32 * FLUIDS_SPLIT_Y, t);
    FLUIDS_set_at(1 * FLUIDS_SPLIT_X,
                  top * FLUIDS_SPLIT_Y,
                  59 * FLUIDS_SPLIT_X,
                  (30 - top) * FLUIDS_SPLIT_Y,
                  fluid_water,
                  1.0);
    FLUIDS_update();
}

static void expect_same_moves_in(const char* desc,
                                 entity classes,
                                 Uint64 seeds,
                                 int flooded_from) {
    bool clings = classes.moves & (move_climb_wall | move_climb_ceiling);
    static const double speeds[] = {
        0.0, 0.0, 20.0, -20.0, 60.0, -60.0, 90.0, -90.0};
    report r = (report){0};
    size_t compared = 0;
    size_t same = 0;
    for (Uint64 seed = 1; seed <= seeds; seed++) {
        build_random_room(seed);
        if (flooded_from < 30) flood(flooded_from);
        CAMERA_center_at(30 * SPRITESHEET_CELL_X, 15 * SPRITESHEET_CELL_Y);
        size_t id = new_entity_with(1, 29, classes);
        Uint64 state = seed;
        static Uint16 fx[MAX_MOVES], fy[MAX_MOVES], wx[MAX_MOVES],
            wy[MAX_MOVES];
        static double fc[MAX_MOVES], wc[MAX_MOVES];
        for (int x = 1; x < 60; x++) {
            for (int y = 1; y < 30; y++) {
                if (!is_standable(x, y) && !(clings && is_clingable(x, y)))
                    continue;
                bool known = SDL_rand_r(&state, 4) > 0;
                double offset = SDL_rand_r(&state, 88) / 10.0 - 4.4;
                double vx = speeds[SDL_rand_r(&state, SDL_arraysize(speeds))];
                size_t nf = ENTITY_test_get_moves(
                    id, x, y, known, offset, vx, false, fx, fy, fc, MAX_MOVES);
                size_t nw = ENTITY_test_get_moves(
                    id, x, y, known, offset, vx, true, wx, wy, wc, MAX_MOVES);
                for (size_t i = 0; i < nw; i++) {
                    for (size_t j = 0; j < nf; j++) {
                        if (fx[j] == wx[i] && fy[j] == wy[i] &&
                            SDL_fabs(fc[j] - wc[i]) <=
                                1.0 / FIXED_T_TARGET_FPS + EPSILON) {
                            same++;
                            break;
                        }
                    }
                }
                compared += nw;
            }
        }
        ENTITY_del_entity(id);
    }
    check(&r, compared > 1000, "Only %lu moves were compared", compared);
    check(&r,
          same >= compared * 0.98,
          "Only %lu of %lu moves (%.2f%%) landed on the same tile at most a "
          "tick apart",
          same,
          compared,
          compared ? 100.0 * same / compared : 0.0);
    finish_report(desc, &r);
    if (flooded_from < 30) flood(30);
}

static void expect_same_moves(const char* desc, entity classes) {
    expect_same_moves_in(desc, classes, 6, 30);
}

static void test_trajectories(void) {
    size_t checked = 0;
    size_t different = ENTITY_test_check_replays(player_like, &checked);
    report r = (report){0};
    check(&r, checked > 10000, "Only %lu steps were compared", checked);
    check(&r,
          different == 0,
          "%lu of %lu steps weren't exactly the same",
          different,
          checked);
    finish_report("Generated the trajectories of an entity moving like the "
                  "player keeping every step and keeping checkpoints, checking "
                  "if every step simulated again from the checkpoints is "
                  "exactly the same...",
                  &r);

    expect_same_moves(
        "Found the moves of an entity moving like the player from every tile "
        "of random rooms, checking if following trajectories finds nearly the "
        "same ones as simulating them whole...",
        (entity){
            .jump = jump_class_high,
            .fall = fall_class_normal,
            .run = run_class_normal,
            .size = size_class_normal,
        });
    expect_same_moves("Did the same for a small, fast and floaty entity...",
                      (entity){
                          .jump = jump_class_medium,
                          .fall = fall_class_floaty,
                          .run = run_class_fast,
                          .size = size_class_small,
                      });
    expect_same_moves("Did the same for a wide, slow and heavy entity...",
                      (entity){
                          .jump = jump_class_high,
                          .fall = fall_class_heavy,
                          .run = run_class_slow,
                          .size = size_class_wide,
                      });

    entity wader = player_like;
    wader.safe_fluids = 1 << fluid_water;
    expect_same_moves_in("Did the same for an entity that can't swim in rooms "
                         "full of water...",
                         wader,
                         3,
                         1);
    expect_same_moves_in(
        "Did the same with them half full of water...", wader, 3, 15);

    entity climber = player_like;
    climber.size = size_class_small;
    climber.moves =
        move_run | move_fall | move_drop | move_climb_wall | move_climb_ceiling;
    expect_same_moves_in("Did the same for an entity that climbs, from where "
                         "it clings too, letting go of walls and ceilings...",
                         climber,
                         3,
                         30);
}
#endif

// Ticks until the entity `id` isn't moving anymore, up to `MAX_TICKS`.
static void tick_until_stopped_moving(size_t id) {
    bool moving = true;
    for (int t = 0; t < MAX_TICKS && moving; t++) {
        tick(id);
        ENTITY_is_moving(id, &moving);
    }
}

static void test_owning(void) {
    build_room();
    CAMERA_center_at(10 * SPRITESHEET_CELL_X, 25 * SPRITESHEET_CELL_Y);
    entity runner = player_like;
    runner.moves = move_run;
    size_t id = new_entity_with(5, 29, runner);
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    body start = (body){0};
    PHYSICS_get_body(e.body_id, &start);
    finished_calls = 0;
    int moved = ENTITY_move_towards(
        id, 20 * SPRITESHEET_CELL_X + 4.0, 30 * SPRITESHEET_CELL_Y);
    bool moving = false;
    ENTITY_is_moving(id, &moving);
    int again = ENTITY_move_towards(
        id, 20 * SPRITESHEET_CELL_X + 4.0, 30 * SPRITESHEET_CELL_Y);
    tick_until_stopped_moving(id);
    entity_move_result r = last_result;
    expect("Made an entity that only runs move towards a tile on the same "
           "floor, checking if "
           "it started a single move, couldn't start another in the "
           "middle of it, and what its on_move_finished_func got once it ran "
           "to the "
           "next tile...",
           12,
           (bool[]){
               moved == 0,
               moving,
               again == 4,
               finished_calls == 1,
               r.move == motion_run,
               r.end == move_completed,
               r.from_x == 5 && r.from_y == 29,
               r.to_x == 6 && r.to_y == 29,
               r.on_node && r.end_x == 6 && r.end_y == 29,
               r.start_x == start.x && r.start_vx == 0.0,
               r.reaches_goal,
               ENTITY_interrupt_movement(id) == 4,
           });

    Uint16 nx = 0;
    Uint16 ny = 0;
    int on_node = ENTITY_get_node(id, &nx, &ny);
    expect("Made it move towards the tile it's on, checking if it was already "
           "there and its node...",
           3,
           (bool[]){
               ENTITY_move_towards(id,
                                   6 * SPRITESHEET_CELL_X + 4.0,
                                   30 * SPRITESHEET_CELL_Y) == 6,
               on_node == 0,
               nx == 6 && ny == 29,
           });
    ENTITY_del_entity(id);

    // A step it has to jump on, interrupted in the air
    build_room();
    fill(10, 28, 5, 2, tile_stone, false);
    id = new_entity_at(9, 29);
    ENTITY_get_entity(id, &e);
    finished_calls = 0;
    moved = ENTITY_move_towards(
        id, 12 * SPRITESHEET_CELL_X + 4.0, 28 * SPRITESHEET_CELL_Y);
    bool on_floor = true;
    for (int t = 0; t < MAX_TICKS && on_floor; t++) {
        tick(id);
        PHYSICS_is_body_on_floor(e.body_id, &on_floor);
    }
    int interrupted = ENTITY_interrupt_movement(id);
    r = last_result;
    ENTITY_is_moving(id, &moving);
    int in_air = ENTITY_move_towards(
        id, 12 * SPRITESHEET_CELL_X + 4.0, 28 * SPRITESHEET_CELL_Y);
    int no_node = ENTITY_get_node(id, &nx, &ny);
    expect("Made an entity jump on a step and interrupted it in the air, "
           "checking if its on_move_finished_func got it right away and it "
           "can't move until it lands...",
           8,
           (bool[]){
               moved == 0,
               interrupted == 0,
               finished_calls == 1,
               r.move == motion_jump,
               r.end == move_interrupted && !r.on_node,
               !moving,
               in_air == 5,
               no_node == 4,
           });
    ENTITY_del_entity(id);

    // Starting in lava, next to a ledge it can jump on
    entity swimmer = player_like;
    swimmer.moves = move_run | move_jump | move_fall | move_drop | move_swim;
    swimmer.safe_fluids = 1 << fluid_water;
    build_pool(fluid_lava);
    id = new_entity_with(20, 25, swimmer);
    bool swimming = false;
    bool unsafe = false;
    ENTITY_get_fluids(id, &swimming, &unsafe);
    int found = ENTITY_find_nearest_safe_tile(id, &nx, &ny);
    Uint16 safe_x = nx;
    Uint16 safe_y = ny;
    finished_calls = 0;
    moved = ENTITY_move_towards(id,
                                safe_x * SPRITESHEET_CELL_X + 4.0,
                                (safe_y + 1) * SPRITESHEET_CELL_Y);
    tick_until_stopped_moving(id);
    r = last_result;
    bool unsafe_after = true;
    ENTITY_get_fluids(id, &swimming, &unsafe_after);
    expect("Put an entity that avoids lava in a pool of lava by its edge, "
           "checking if it knew, found the ledge and got out on it...",
           6,
           (bool[]){
               unsafe,
               found == 0,
               safe_x <= 19 && safe_y == 24,
               moved == 0,
               r.end == move_completed,
               !unsafe_after,
           });
    ENTITY_del_entity(id);

    // At the bottom of the middle of the pool, only getting out swimming
    // through the lava
    id = new_entity_with(30, 29, swimmer);
    found = ENTITY_find_nearest_safe_tile(id, &nx, &ny);
    int avoiding = ENTITY_move_towards(
        id, 50 * SPRITESHEET_CELL_X + 4.0, 25 * SPRITESHEET_CELL_Y);
    run_order(
        id, 50, 24, order_move_ignoring_unsafe, 2 * MAX_TICKS, NULL, NULL);
    int ignoring = first_moved;
    int reached_x = 0;
    int reached_y = 0;
    body rb = (body){0};
    get_tile(id, &reached_x, &reached_y, &rb);
    ENTITY_del_entity(id);
    expect("Put it at the bottom of the middle of the pool instead, checking "
           "if it found a ledge swimming through the lava, couldn't move "
           "avoiding it, but could ignoring it, and got out to the other "
           "side...",
           5,
           (bool[]){
               found == 0,
               ny == 24 && (nx == 19 || nx == 40),
               avoiding == 7,
               ignoring == 0,
               reached_x == 50 && reached_y == 24,
           });

    // Walls too tall to jump on around the pool
    build_pool(fluid_lava);
    fill(1, 10, 19, 15, tile_stone, false);
    fill(40, 10, 20, 15, tile_stone, false);
    id = new_entity_with(30, 29, swimmer);
    found = ENTITY_find_nearest_safe_tile(id, &nx, &ny);
    ENTITY_del_entity(id);
    id = new_entity_with(30, 25, swimmer);
    Uint16 sx = 0;
    Uint16 sy = 0;
    ENTITY_get_node(id, &sx, &sy);
    finished_calls = 0;
    int jumped = ENTITY_jump(id,
                             jump_class_high,
                             sx * SPRITESHEET_CELL_X + 4.0,
                             (sy + 1) * SPRITESHEET_CELL_Y);
    bool left_lava = false;
    moving = true;
    for (int t = 0; t < MAX_TICKS && moving; t++) {
        tick(id);
        ENTITY_get_fluids(id, &swimming, &unsafe);
        left_lava = left_lava || !unsafe;
        ENTITY_is_moving(id, &moving);
    }
    r = last_result;
    ENTITY_del_entity(id);
    expect("Walled the pool in too high to jump out of, checking if it found "
           "no safe tile but the one above the lava from its bottom, and if it "
           "could jump in place from its surface, out and back in...",
           6,
           (bool[]){
               found == 6,
               nx == 30 && ny == 24,
               jumped == 0,
               left_lava,
               finished_calls == 1,
               r.end == move_completed,
           });

    // A ceiling on it, so there's no air
    fill(20, 24, 20, 1, tile_stone, false);
    id = new_entity_with(30, 29, swimmer);
    found = ENTITY_find_nearest_safe_tile(id, &nx, &ny);
    ENTITY_del_entity(id);
    expect("Put a ceiling on the pool, checking if it found no tile above it "
           "without lava either...",
           1,
           (bool[]){found == 7});

    for (fluid_type t = 0; t < FLUIDS_TOTAL; t++)
        FLUIDS_clear_at(0, 0, 62 * FLUIDS_SPLIT_X, 32 * FLUIDS_SPLIT_Y, t);
    FLUIDS_update();
}

// An entity in `classes` with its body's top left at `(x,y)`, in pixels.
static size_t new_entity_at_pos(double x, double y, entity classes) {
    Uint8 w = 0;
    Uint8 h = 0;
    ENTITY_get_size(classes.size, &w, &h);
    PHYSICS_new_body(
        (body){.type = physics_actor, .x = x, .y = y, .w = w, .h = h},
        &classes.body_id);
    classes.on_move_finished_func = on_move_finished;
    size_t id = 0;
    ENTITY_new_entity(classes, &id);
    return id;
}

// Moves the entity `id` towards the tile `(gx,gy)` until it got there and
// stopped, or `MAX_TICKS` went by, then waits a second, checking it stays
// there, not moving, other than gravity pulling it against the floor when it
// isn't clinging to anything. Puts what it's clinging to after on `surface`,
// returning whether it got there and stayed.
static bool climb_to(size_t id, int gx, int gy, entity_surface* surface) {
    run_order(id, gx, gy, order_move, MAX_TICKS, NULL, NULL);
    for (int t = 0; t < FIXED_T_TARGET_FPS; t++)
        tick(id);
    int x = 0;
    int y = 0;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    ENTITY_get_clinging(id, surface);
    return x == gx && y == gy && SDL_fabs(b.vel_x) <= EPSILON &&
           (*surface == surface_none || SDL_fabs(b.vel_y) <= EPSILON);
}

static void test_climbing(void) {
    entity climber = player_like;
    climber.size = size_class_small;
    climber.moves =
        move_run | move_fall | move_drop | move_climb_wall | move_climb_ceiling;

    // A block 10 tiles tall it can only climb on
    build_room();
    fill(30, 20, 30, 10, tile_stone, false);
    size_t id = new_entity_with(10, 29, climber);
    entity_surface surface = surface_none;
    bool on_top = climb_to(id, 40, 19, &surface);
    ENTITY_del_entity(id);
    expect("Made an entity that climbs walls go on top of a block too tall to "
           "jump on, checking if it climbed up its side and over it...",
           2,
           (bool[]){on_top, surface == surface_none});

    id = new_entity_with(10, 29, climber);
    bool on_wall = climb_to(id, 29, 24, &surface);
    ENTITY_del_entity(id);
    expect("Made it go halfway up the block's side, checking if it stayed "
           "there clinging to it...",
           2,
           (bool[]){on_wall, surface == surface_wall_right});

    // A pit too wide to cross, under a slab only reachable through a pillar
    build_room();
    fill(0, 30, 61, 16, tile_stone, false);
    fill(20, 30, 20, 15, 0, false);
    fill(17, 26, 26, 1, tile_stone, false);
    fill(17, 27, 1, 3, tile_stone, false);
    id = new_entity_with(18, 29, climber);
    bool hanging = climb_to(id, 30, 27, &surface);
    ENTITY_del_entity(id);
    expect("Made it go under a slab over a pit, checking if it climbed up a "
           "pillar onto the slab's underside and along it, staying there...",
           2,
           (bool[]){hanging, surface == surface_ceiling});

    // Clinging under a floating slab, with the floor right under it
    build_room();
    fill(20, 20, 21, 1, tile_stone, false);
    Uint8 w = 0;
    Uint8 h = 0;
    ENTITY_get_size(climber.size, &w, &h);
    id = new_entity_at_pos(30 * SPRITESHEET_CELL_X +
                               (SPRITESHEET_CELL_X - w) / 2.0,
                           21 * SPRITESHEET_CELL_Y,
                           climber);
    entity_surface before = surface_none;
    ENTITY_get_clinging(id, &before);
    finished_calls = 0;
    bool landed = climb_to(id, 30, 29, &surface);
    entity_move_result r = last_result;
    ENTITY_del_entity(id);
    expect("Put it under a floating slab, checking if it clung to it, then "
           "made it go to the floor under it, checking if it let go and "
           "landed there...",
           4,
           (bool[]){before == surface_ceiling,
                    landed,
                    r.move == motion_let_go,
                    surface == surface_none});
}

// Moves the entity `id` towards the tile `(gx,gy)` until it stopped moving or
// `MAX_TICKS` went by, checking if it's there, then waits a second, checking
// it stays there, not moving at all.
static bool fly_to(size_t id, int gx, int gy) {
    run_order(id, gx, gy, order_move, 2 * MAX_TICKS, NULL, NULL);
    int x0 = 0;
    int y0 = 0;
    body b0 = (body){0};
    get_tile(id, &x0, &y0, &b0);
    for (int t = 0; t < FIXED_T_TARGET_FPS; t++)
        tick(id);
    int x = 0;
    int y = 0;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    return x0 == gx && y0 == gy && x == gx && y == gy &&
           SDL_fabs(b.y - b0.y) <= 1.0 && SDL_fabs(b.vel_x) <= EPSILON &&
           SDL_fabs(b.vel_y) <= EPSILON;
}

static void test_flying(void) {
    entity flyer = player_like;
    flyer.moves = move_fly;

    build_room();
    size_t id = new_entity_with(10, 29, flyer);
    bool hovering = fly_to(id, 30, 10);
    ENTITY_del_entity(id);
    expect("Made an entity that flies go high up in the air, checking if it "
           "flew there and stayed there...",
           1,
           (bool[]){hovering});

    build_pool(fluid_lava);
    id = new_entity_with(5, 24, flyer);
    bool across = fly_to(id, 50, 24);
    bool swimming = false;
    bool unsafe = false;
    ENTITY_get_fluids(id, &swimming, &unsafe);
    ENTITY_del_entity(id);
    expect("Made it go across a pool of lava, checking if it flew over it...",
           2,
           (bool[]){across, !unsafe});
    for (fluid_type t = 0; t < FLUIDS_TOTAL; t++)
        FLUIDS_clear_at(0, 0, 62 * FLUIDS_SPLIT_X, 32 * FLUIDS_SPLIT_Y, t);
    FLUIDS_update();

    // A wall from the floor up to two tiles under the ceiling
    build_room();
    fill(30, 3, 2, 27, tile_stone, false);
    id = new_entity_with(20, 29, flyer);
    bool over = fly_to(id, 40, 29);
    ENTITY_del_entity(id);
    expect("Made it go past a wall with a gap only at its top, checking if it "
           "flew over it...",
           1,
           (bool[]){over});
}

// Makes the entity `id` land on the tile `(gx,gy)`, ticking until it's done
// or `MAX_TICKS` went by, then waits a second. Puts what it's clinging to on
// `surface` and whether it's on the floor on `on_floor`, returning whether it
// landed there and stayed without moving sideways, or at all while clinging.
// Sets the `bool` at `arg` if the entity `id` is flying.
static void track_flying(size_t id, void* arg) {
    bool flying = false;
    ENTITY_is_flying(id, &flying);
    *(bool*)arg = *(bool*)arg || flying;
}

static bool
land_at(size_t id, int gx, int gy, entity_surface* surface, bool* on_floor) {
    run_order(id, gx, gy, order_land, 2 * MAX_TICKS, NULL, NULL);
    int x0 = 0;
    int y0 = 0;
    body b0 = (body){0};
    get_tile(id, &x0, &y0, &b0);
    for (int t = 0; t < FIXED_T_TARGET_FPS; t++)
        tick(id);
    int x = 0;
    int y = 0;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    PHYSICS_is_body_on_floor(e.body_id, on_floor);
    ENTITY_get_clinging(id, surface);
    return last_moved == 6 && x0 == gx && y0 == gy && x == gx && y == gy &&
           SDL_fabs(b.vel_x) <= EPSILON &&
           (*surface == surface_none || SDL_fabs(b.y - b0.y) <= EPSILON);
}

static void test_landing(void) {
    entity flyer = player_like;
    flyer.moves = move_fly;
    Uint8 w = 0;
    Uint8 h = 0;
    ENTITY_get_size(flyer.size, &w, &h);

    build_room();
    size_t id = new_entity_at_pos(
        20 * SPRITESHEET_CELL_X + 2.0, 15 * SPRITESHEET_CELL_Y, flyer);
    entity_surface surface = surface_none;
    bool on_floor = false;
    bool landed = land_at(id, 30, 29, &surface, &on_floor);
    entity_move_result r = last_result;
    int midair = ENTITY_land_on(
        id, 30 * SPRITESHEET_CELL_X + 4.0, 16 * SPRITESHEET_CELL_Y);
    bool flying_after_landing = true;
    ENTITY_is_flying(id, &flying_after_landing);
    int took_off = ENTITY_move_towards(
        id, 30 * SPRITESHEET_CELL_X + 4.0, 21 * SPRITESHEET_CELL_Y);
    bool flying = false;
    ENTITY_is_flying(id, &flying);
    ENTITY_del_entity(id);
    expect("Made an entity that only flies land on the floor from the air, "
           "checking if it landed there and stayed on it, couldn't land in "
           "the middle of the air, and flew again moving towards somewhere "
           "else, since it can't move otherwise...",
           8,
           (bool[]){
               landed,
               on_floor,
               surface == surface_none,
               r.move == motion_land && r.end == move_completed,
               !flying_after_landing,
               midair == 2,
               took_off == 0,
               flying,
           });

    // Flying and climbing, by a block, in the air until it's told to fly
    entity climbing_flyer = flyer;
    climbing_flyer.moves =
        move_fly | move_run | move_climb_wall | move_climb_ceiling | move_drop;
    build_room();
    fill(30, 20, 30, 10, tile_stone, false);
    id = new_entity_at_pos(
        20 * SPRITESHEET_CELL_X + 2.0, 15 * SPRITESHEET_CELL_Y, climbing_flyer);
    bool flying_at_first = true;
    ENTITY_is_flying(id, &flying_at_first);
    int flew = ENTITY_fly_to(
        id, 20 * SPRITESHEET_CELL_X + 4.0, 16 * SPRITESHEET_CELL_Y);
    bool hovering = fly_to(id, 20, 15);
    int moved_flying = ENTITY_move_towards(
        id, 25 * SPRITESHEET_CELL_X + 4.0, 30 * SPRITESHEET_CELL_Y);
    bool on_wall = land_at(id, 29, 24, &surface, &on_floor);
    entity_surface wall = surface;
    bool flying_on_wall = true;
    ENTITY_is_flying(id, &flying_on_wall);
    expect("Made an entity that flies and climbs fly in the air, checking if "
           "it didn't fly until then, hovered there and couldn't move other "
           "than flying while flying, then land on a block's side, checking "
           "if it clung to it and stopped flying...",
           7,
           (bool[]){
               !flying_at_first,
               flew == 0 || flew == 6,
               hovering,
               moved_flying == 5,
               on_wall,
               wall == surface_wall_right,
               !flying_on_wall,
           });

    // Climbing to the block's top, never flying on the way
    bool ever_flew = false;
    run_order(id, 35, 19, order_move, MAX_TICKS, track_flying, &ever_flew);
    int x = 0;
    int y = 0;
    body b = (body){0};
    get_tile(id, &x, &y, &b);
    bool climbed_up = x == 35 && y == 19;

    // Then flying again, to land on the ceiling
    run_order(id, 20, 2, order_fly, 2 * MAX_TICKS, NULL, NULL);
    bool on_ceiling = land_at(id, 20, 1, &surface, &on_floor);
    ENTITY_del_entity(id);
    expect("Made it go on top of the block, checking if it climbed there "
           "without flying, then made it fly and land on the ceiling, checking "
           "if it clung to it...",
           4,
           (bool[]){
               climbed_up,
               !ever_flew,
               on_ceiling,
               surface == surface_ceiling,
           });
}

static void test_jumping(void) {
    build_room();
    fill(14, 28, 3, 2, tile_stone, false);
    CAMERA_center_at(10 * SPRITESHEET_CELL_X, 25 * SPRITESHEET_CELL_Y);
    size_t id = new_entity_at(12, 29);
    entity e = (entity){0};
    ENTITY_get_entity(id, &e);
    finished_calls = 0;
    // As far on top of the step as it gets
    double tx = 14 * SPRITESHEET_CELL_X + 1.0;
    int jumped = ENTITY_jump(id, jump_class_high, tx, 28 * SPRITESHEET_CELL_Y);
    tick_until_stopped_moving(id);
    entity_move_result r = last_result;
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    double off = SDL_fabs(b.x + b.w / 2.0 - tx);
    int too_far = ENTITY_jump(id,
                              jump_class_high,
                              40 * SPRITESHEET_CELL_X + 4.0,
                              30 * SPRITESHEET_CELL_Y);
    int too_weak = ENTITY_jump(id,
                               jump_class_tiny,
                               15 * SPRITESHEET_CELL_X + 4.0,
                               20 * SPRITESHEET_CELL_Y);
    int no_class = ENTITY_jump(id, ENTITY_JUMP_CLASSES_TOTAL, tx, 0.0);
    expect("Made an entity jump on top of a step, checking if it landed where "
           "it was told, close to the spot, and the errors of jumping too far, "
           "too high for its class and with a class that doesn't exist...",
           8,
           (bool[]){
               jumped == 0,
               finished_calls == 1,
               r.move == motion_jump && r.end == move_completed,
               r.end_x == 14 && r.end_y == 27,
               off <= 1.0,
               too_far == 7,
               too_weak == 7,
               no_class == 2,
           });

    jumped = ENTITY_jump(id, jump_class_high, tx, 28 * SPRITESHEET_CELL_Y);
    int again = ENTITY_jump(id, jump_class_high, tx, 28 * SPRITESHEET_CELL_Y);
    bool on_floor = true;
    for (int t = 0; t < MAX_TICKS && on_floor; t++) {
        tick(id);
        PHYSICS_is_body_on_floor(e.body_id, &on_floor);
    }
    ENTITY_interrupt_movement(id);
    int in_air = ENTITY_jump(id, jump_class_high, tx, 28 * SPRITESHEET_CELL_Y);
    ENTITY_del_entity(id);
    expect("Made it jump in place, checking if it couldn't jump again in the "
           "middle of it, nor in the air once interrupted...",
           3,
           (bool[]){jumped == 0, again == 4, in_air == 5});
}

static void test_storage(void) {
    TILEMAP_clear();
    CAMERA_center_at(0.0f, 0.0f);
    size_t body_id = new_body_at(2, 2, size_class_normal);
    size_t id = 0;
    int r = ENTITY_new_entity(
        (entity){
            .body_id = body_id,
            .run = run_class_fast,
            .size = size_class_normal,
            .on_create_func = on_create,
            .on_destroy_func = on_destroy,
            .update_func = on_update,
        },
        &id);
    entity data = (entity){0};
    ENTITY_get_entity(id, &data);
    expect("Created an entity, checking it and if its on_create_func was "
           "called with its ID...",
           5,
           (bool[]){
               r == 0,
               data.body_id == body_id,
               data.run == run_class_fast,
               create_calls == 1,
               created_id == id,
           });

    PHYSICS_update();
    ENTITY_update();
    ENTITY_update();
    CAMERA_center_at(TILEMAP_SIZE_X * SPRITESHEET_CELL_X,
                     TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y);
    PHYSICS_update();
    ENTITY_update();
    expect("Updated it twice within the simulation space and once outside it, "
           "checking if its update_func was only called while within it...",
           1,
           (bool[]){update_calls == 2});
    CAMERA_center_at(0.0f, 0.0f);
    PHYSICS_update();

    data.run = run_class_slow;
    ENTITY_edit_entity(id, data);
    ENTITY_get_entity(id, &data);
    size_t missing_body = body_id + 1;
    entity wrong_size = data;
    wrong_size.size = size_class_wide;
    entity wrong_class = data;
    wrong_class.jump = ENTITY_JUMP_CLASSES_TOTAL;
    expect("Edited it, as well as created and edited one with a body that "
           "doesn't exist, a body of another size than its size class and a "
           "class that doesn't exist, checking it and the errors...",
           7,
           (bool[]){
               data.run == run_class_slow,
               ENTITY_new_entity((entity){.body_id = missing_body}, &id) == 2,
               ENTITY_edit_entity(id, (entity){.body_id = missing_body}) == 2,
               ENTITY_new_entity(wrong_size, &id) == 2,
               ENTITY_edit_entity(id, wrong_size) == 2,
               ENTITY_new_entity(wrong_class, &id) == 2,
               ENTITY_edit_entity(id, wrong_class) == 2,
           });

    ENTITY_del_entity(id);
    body b = (body){0};
    expect("Deleted it, checking if its on_destroy_func was called, its body "
           "is gone and the errors of using it and an index out of range...",
           6,
           (bool[]){
               destroy_calls == 1,
               PHYSICS_get_body(body_id, &b) == 1,
               ENTITY_get_entity(id, &data) == 1,
               ENTITY_move_towards(id, 0.0, 0.0) == 1,
               ENTITY_interrupt_movement(id) == 1,
               ENTITY_del_entity(ENTITY_STARTING_POOL_CAP << 4) == 3,
           });
}

void test_entity(void) {
    printf(YELLOW "\nTesting entities...\n" RESET);

    PHYSICS_start();
    FLUIDS_start();
    ENTITY_start();

    test_storage();
    test_moves();
    test_swimming();
    test_owning();
    test_jumping();
    test_climbing();
    test_flying();
    test_landing();
#ifdef TEST
    test_trajectories();
#endif

    ENTITY_stop();
    FLUIDS_stop();
    PHYSICS_stop();
    TILEMAP_clear();
    CAMERA_center_at(0.0f, 0.0f);
}
