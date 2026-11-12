#include "engine/entity.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "data-structs/pool.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/physics.h"
#include "engine/tilemap.h"
#include "paths.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

// Entities find their paths with A* on the tilemap, where each node is a tile
// the entity's body can stand on, swim in, cling to a wall or ceiling on, or
// fly in, and each edge is a move to another node, like running to the tile
// beside it or jumping. Entities only do one move of a path at a time, when
// their owner tells them to, which can be right as the one before ends, so
// owners decide what they do after each one. Each node keeps where the body
// was and how fast it was going when the search first got there, which is
// where its moves start from.
//
// Running, swimming, climbing, flying and landing are steered, going to the
// node next to it. Jumping, falling, dropping and letting go replay their
// inputs, and are found by following trajectories: each combination of
// classes has the trajectory of each of them simulated once with
// `PHYSICS_simulate()`, on made up tiles with nothing but the floor under the
// starting tile, or the wall or ceiling it lets go of, from each pixel of a
// tile the body might start from going each of `ENTITY_START_SPEEDS`, so
// they're the same from any tile, just moved by whole tiles. A move starts
// from the closest ones to where the body is and how fast it's going, so it
// sometimes ends up somewhere else than where it was simulated to, which its
// owner finds out when it ends. Each trajectory keeps every tile the physics
// might look at along the way, in the order they're first needed, so a move
// from a real tile only looks them up. If they're all the same as the made up
// ones, it does what its trajectory did. Otherwise, it does what its
// trajectory did up to the step that first needed a different tile, and the
// physics take it from there until it lands. That way, a move costs a few
// tile lookups and the steps near whatever it collides with, which is often
// only the floor it lands on. Trajectories only keep checkpoints of their
// steps, simulating the ones in between again when they're needed, exactly
// the same.
//
// Fluids change what the physics do, so trajectories are made on made up
// tiles without them, or, for entities that don't swim, full of them, which
// they move in slower. A real tile with fluids is the same as a made up one
// only if the made up ones have the same, full of fluids it doesn't avoid or
// with none. Entities that swim swim to the nodes around them in fluids,
// steering towards them in any direction, since swimming is slow enough to
// steer, and jump out of the fluid from its surface. Nodes with fluids it
// avoids aren't nodes, and moves that would touch them are left out, unless
// it's told to ignore them, or it's in them, so it can get out.
//
// Entities that fly only fly when they can't move otherwise, or when they're
// told to, until they land, so whatever finds and does their moves sees them
// with no moves but flying while they fly, and with every move but it
// otherwise, see `moving_as()`.



// Where an entity's body is when it gets to a node, if it's `known`: its
// position `x` and how fast it's going, `vx`.
typedef struct {
    bool known;
    double x, vx;
} node_state;

// How a move that isn't steered starts: right away, like the tick
// the move before it lands, or once a running body gets to where it starts,
// once the entity comes to rest by itself, or once it stops at its node's
// center, for when it doesn't know where it'll be.
typedef enum {
    start_now,
    start_at_rest,
    start_aligned,
} start_mode;

// What a body is in: no fluids, only fluids it can swim in, like at the bottom
// of a pool, or some of each, like at its surface. Trajectories are generated
// for the first two, on made up tiles without fluids, or full of them, where
// entities that don't swim move with the `ENTITY_UNDERFLUID_` factors.
typedef enum {
    medium_dry,
    medium_submerged,
    medium_mixed,
} medium;
#define MEDIUMS 2

// How a move that isn't steered starts from its node: with
// `mode`, from the start offset `offset` and speed `speed` of its
// trajectories, which is the body at `x,y` going `vx`, in `medium`, after
// waiting `wait` seconds for it to get there.
typedef struct {
    start_mode mode;
    medium medium;
    int offset, speed;
    double x, y, vx;
    double wait;
} move_start;

// A move from the node at `(from_x,from_y)` to the one at `(to_x,to_y)`,
// taking `cost` seconds and getting there the way `to_state` is. Moves that
// replay their inputs hold `dir` for their first `dir_time` seconds and, on
// jumps, jump with the class `jump`, starting with `start` from where the body
// is at `start_x` going `start_vx`, or from swimming if `from_swim`. Steered
// moves go to a node next to it: running ones in `dir`, and the rest to
// `target_x,target_y`, which is also where ones starting from where they stop
// start from.
typedef struct {
    entity_motion move;
    Sint8 dir;
    double dir_time;
    jump_class jump;
    start_mode start;
    double start_x, start_vx;
    Uint16 from_x, from_y;
    Uint16 to_x, to_y;
    double cost;
    node_state to_state;
    bool from_swim;
    double target_x, target_y;
} path_move;

// What a move does the same from anywhere, which is all trajectories keep of
// it: `move`, holding `dir` for its first `dir_time` seconds and, on jumps,
// jumping with the class `jump`.
typedef struct {
    entity_motion move;
    Sint8 dir;
    jump_class jump;
    double dir_time;
} move_inputs;

static move_inputs inputs_of(const path_move* m) {
    return (move_inputs){.move = m->move,
                         .dir = m->dir,
                         .jump = m->jump,
                         .dir_time = m->dir_time};
}

static path_move inputs_move(move_inputs in) {
    return (path_move){.move = in.move,
                       .dir = in.dir,
                       .jump = in.jump,
                       .dir_time = in.dir_time};
}

// Values the classes of an entity move it with, its run and fall classes'
// with `jump_factor` multiplying its jump classes' `jump_speed`. While it
// swims, they're its swim class' instead, and it `swims`: it moves vertically
// the same as horizontally, without gravity, jumping with `jump_speed`.
typedef struct {
    double max_speed, acceleration, friction;
    double gravity, max_fall_speed;
    double jump_factor;
    bool swims;
    double jump_speed;
} movement;

typedef struct {
    double jump_speed;
} jump_values;

typedef struct {
    double gravity, max_fall_speed;
} fall_values;

typedef struct {
    double max_speed, acceleration, friction;
} run_values;

typedef struct {
    double max_speed, acceleration, friction, jump_speed;
} swim_values;

typedef struct {
    double max_speed, acceleration, friction;
} fly_values;

typedef struct {
    Uint8 w, h;
} size_values;

// What the physics see of a tile.
typedef enum {
    kind_empty,
    kind_platform,
    kind_solid,
} tile_kind;

// What a node is to an entity: a tile it can stand on, one it would swim in,
// one it can cling to a wall or ceiling on, or one in the air it flies in.
typedef enum {
    node_none,
    node_ground,
    node_swim,
    node_climb,
    node_fly,
} node_kind;

// The fluids on a tile's fluid cells, the cell `(i,j)` of it being the bit
// `i + FLUIDS_SPLIT_X * j`. `full` has the cells with enough fluid to swim in,
// and `fluids` the types with enough volume on each cell for entities that
// avoid them to avoid it, with the bit `1 << type` for each `fluid_type`.
typedef struct {
    Uint8 full;
    Uint8 fluids[FLUIDS_SPLIT_X * FLUIDS_SPLIT_Y];
} tile_fluids;
static_assert(FLUIDS_SPLIT_X * FLUIDS_SPLIT_Y <= 8 && FLUIDS_TOTAL <= 8,
              "tile_fluids has a bit per fluid cell and per fluid type");

// What fluids do to a body: whether it swims and whether it's on any fluid the
// entity avoids.
typedef struct {
    bool swimming, unsafe;
} body_fluids;

// How a move is going, to know when it ends. `airborne` is whether it left the
// floor, `wet` whether it started swimming and `left_water` whether it stopped
// swimming since.
typedef struct {
    bool airborne, wet, left_water;
} move_progress;

typedef enum {
    progress_going,
    progress_landed,
    progress_stopped,
} progress_end;

// A state of a trajectory, before a step, where `x,y` is the body's position
// on the made up tiles and `vx,vy` its velocity.
typedef struct {
    double x, y, vx, vy;
} trajectory_step;

// A state of a trajectory being generated, with whether it left the floor,
// `airborne`, and `step_vx,step_vy`, the velocity it took the step with, after
// the move's inputs, which are only needed to know what tiles it looks at.
typedef struct {
    trajectory_step state;
    bool airborne;
    double step_vx, step_vy;
} generated_step;

// A tile at `(x,y)` relative to the starting tile that the physics might look
// at on the step from the state `step` of a trajectory to the next one, and
// not on any step before.
typedef struct {
    Sint16 x, y;
    Uint16 step;
} trajectory_cell;

typedef enum {
    trajectory_landed,
    trajectory_stopped,
    trajectory_timed_out,
} trajectory_end;

// The trajectory of the move with `inputs` from the made up starting tile, how
// it ended, and
// the tiles the physics might look at along it, a `trajectory_cell` dynarr on
// `cells`. Its states are only kept every `checkpoint_ticks` steps, a
// `trajectory_step` dynarr on `checkpoints`, and the one it ended on, `last`,
// on the step `last_step`. The ones in between are simulated again from the
// checkpoint before them, see `get_step()`. The only made up tiles are the
// ones from `x0,y0` to `x1,y1` relative to the starting tile, platforms when
// `platform`: the floor under the body when it starts standing, or the wall or
// ceiling it lets go of, `cling`. `drop_time` is how long it drops down from
// platforms at the start. It left the floor on the step `airborne_from`, and
// never did if it's past `last_step`. Its made up tiles are full of fluids if
// `submerged`.
typedef struct {
    move_inputs inputs;
    dynarr checkpoints;
    trajectory_step last;
    Uint16 last_step;
    dynarr cells;
    Uint16 airborne_from;
    bool submerged;
    trajectory_end end;
    Sint16 x0, y0, x1, y1;
    bool platform;
    entity_surface cling;
    double drop_time;
} trajectory;

// Trajectories of a combination of classes from a start offset and speed, a
// `trajectory` dynarr, generated the first time they're needed.
typedef struct {
    bool generated;
    dynarr trajectories;
} trajectory_set;

// Entities are wrapped with the state of their moves, so it isn't exposed on
// `entity`. `path` is the last path found, a `path_move` dynarr, to the node
// `goal_x,goal_y`, which it reaches if `reaches_goal`, found ignoring fluids it
// avoids if `ignores_unsafe`, and `path_at` is the move on it it's on, or the
// next one when it's not `moving`. A move that isn't steered starts once the
// entity gets to where it starts from, then `executing` is set and `progress`
// is how it's going, so it knows when it lands. `move_time` counts how long
// it's been waiting for it to start, then how long it's been going since.
// `result` has what the move did so far. `climbing` is whether it's clinging to
// whatever it's climbing, without falling, which it stops doing once it isn't
// touching anything it climbs. `flying` is whether it's flying, which it only
// does when it can't move any other way, or when it's told to with
// `ENTITY_fly_to()`, until it lands.
typedef struct {
    entity entity;
    dynarr path;
    size_t path_at;
    Uint16 goal_x, goal_y;
    bool reaches_goal;
    bool ignores_unsafe;
    bool moving;
    bool executing;
    double move_time;
    move_progress progress;
    entity_move_result result;
    bool climbing;
    bool flying;
} entity_data;

// A node found by the path search. `parent` is the index on `nodes` of the
// node it was reached from with the move `via`, `-1` for the starting one, and
// `state` is how it got there.
typedef struct {
    Uint16 x, y;
    double g;
    Sint32 parent;
    path_move via;
    node_state state;
    bool closed;
} search_node;

typedef struct {
    double f;
    Uint32 node;
} heap_item;

// What a move of the entity `e` simulated with `PHYSICS_simulate()` did, see
// `sim_step()`. It starts `t0` seconds into the move. `started_unsafe` is
// whether it's still in a fluid it avoids it started in.
typedef struct {
    const entity* e;
    const path_move* m;
    double t0;
    bool started_unsafe;
    move_progress progress;
    bool landed;
    bool on_floor;
    double time;
} sim_state;

// What generating a trajectory recorded, see `gen_step()`, with `steps` a
// `generated_step` dynarr.
typedef struct {
    const movement* mv;
    const path_move* m;
    dynarr* steps;
    move_progress progress;
    trajectory_end end;
} gen_state;



// Where the made up tiles trajectories are generated on start, far from the
// tilemap's edges so they don't get in the way.
#define VIRTUAL_X (TILEMAP_SIZE_X / 2)
#define VIRTUAL_Y (TILEMAP_SIZE_Y / 2)

static const char classes_csv[] = {
#embed ENTITY_CLASSES_CSV_PATH suffix(, '\0')
};

static const char* jump_names[ENTITY_JUMP_CLASSES_TOTAL] = {
    [jump_class_tiny] = "tiny",
    [jump_class_low] = "low",
    [jump_class_medium] = "medium",
    [jump_class_high] = "high",
};
static const char* fall_names[ENTITY_FALL_CLASSES_TOTAL] = {
    [fall_class_floaty] = "floaty",
    [fall_class_normal] = "normal",
    [fall_class_heavy] = "heavy",
};
static const char* run_names[ENTITY_RUN_CLASSES_TOTAL] = {
    [run_class_slow] = "slow",
    [run_class_normal] = "normal",
    [run_class_fast] = "fast",
};
static const char* swim_names[ENTITY_SWIM_CLASSES_TOTAL] = {
    [swim_class_normal] = "normal",
};
static const char* fly_names[ENTITY_FLY_CLASSES_TOTAL] = {
    [fly_class_normal] = "normal",
};
static const char* size_names[ENTITY_SIZE_CLASSES_TOTAL] = {
    [size_class_small] = "small",
    [size_class_normal] = "normal",
    [size_class_wide] = "wide",
};

static jump_values jumps[ENTITY_JUMP_CLASSES_TOTAL] = {0};
static fall_values falls[ENTITY_FALL_CLASSES_TOTAL] = {0};
static run_values runs[ENTITY_RUN_CLASSES_TOTAL] = {0};
static swim_values swims[ENTITY_SWIM_CLASSES_TOTAL] = {0};
static fly_values flies[ENTITY_FLY_CLASSES_TOTAL] = {0};
static size_values sizes[ENTITY_SIZE_CLASSES_TOTAL] = {0};

// Each kind of class, its classes on `ENTITY_CLASSES_CSV_PATH` being on the
// rows named `prefix` and each of its `names`, like `jump_high`, with its
// `columns` values in order, read as `type`, each class being `stride` bytes
// on `values`.
typedef struct {
    const char* prefix;
    const char** names;
    int total;
    void* values;
    size_t stride;
    int columns;
    parsing_type type;
} class_kind;

static const class_kind class_kinds[] = {
    {"jump",
     jump_names,
     ENTITY_JUMP_CLASSES_TOTAL,
     jumps,
     sizeof(jump_values),
     1,
     parsing_double},
    {"fall",
     fall_names,
     ENTITY_FALL_CLASSES_TOTAL,
     falls,
     sizeof(fall_values),
     2,
     parsing_double},
    {"run",
     run_names,
     ENTITY_RUN_CLASSES_TOTAL,
     runs,
     sizeof(run_values),
     3,
     parsing_double},
    {"swim",
     swim_names,
     ENTITY_SWIM_CLASSES_TOTAL,
     swims,
     sizeof(swim_values),
     4,
     parsing_double},
    {"fly",
     fly_names,
     ENTITY_FLY_CLASSES_TOTAL,
     flies,
     sizeof(fly_values),
     3,
     parsing_double},
    {"size",
     size_names,
     ENTITY_SIZE_CLASSES_TOTAL,
     sizes,
     sizeof(size_values),
     2,
     parsing_uint8},
};

static const double dir_hold_times[] = {ENTITY_DIR_HOLD_TIMES};

// Steps between the states trajectories keep, `1` keeping all of them, which
// is only for tests to compare with.
static int checkpoint_ticks = ENTITY_TRAJECTORY_CHECKPOINT_TICKS;
static const double start_speeds[] = {ENTITY_START_SPEEDS};

// Moves start from each pixel of a tile, where the body's center is, and from
// rest or going at each of `start_speeds` but the first to either side, which
// is start `2 * i - 1` to the right and `2 * i` to the left.
#define START_OFFSETS SPRITESHEET_CELL_X
#define START_SPEEDS (2 * ENTITY_START_SPEEDS_TOTAL - 1)

// Jumps of each jump class, and falls and drops, which don't depend on it,
// from each start offset and speed.
static trajectory_set
    jump_sets[MEDIUMS][ENTITY_JUMP_CLASSES_TOTAL][ENTITY_FALL_CLASSES_TOTAL]
             [ENTITY_RUN_CLASSES_TOTAL][ENTITY_SIZE_CLASSES_TOTAL]
             [START_OFFSETS][START_SPEEDS] = {0};
static trajectory_set
    ground_sets[MEDIUMS][ENTITY_FALL_CLASSES_TOTAL][ENTITY_RUN_CLASSES_TOTAL]
               [ENTITY_SIZE_CLASSES_TOTAL][START_OFFSETS][START_SPEEDS] = {0};

// Letting go of walls on the left, walls on the right and ceilings, from rest
// where the body clings to them, `surface - 1`.
#define CLING_SURFACES 3
static trajectory_set
    let_go_sets[MEDIUMS][ENTITY_FALL_CLASSES_TOTAL][ENTITY_RUN_CLASSES_TOTAL]
               [ENTITY_SIZE_CLASSES_TOTAL][CLING_SURFACES] = {0};

// Combinations of classes `ENTITY_register_classes()` registered, so only
// their trajectories are generated up front: whether each combination of fall,
// run and size classes is `used`, how many jump classes it jumps with,
// `jumps`, which are the weakest ones up to the strongest registered with it,
// whether it goes in fluids without swimming, `submerged`, and whether it lets
// go of walls or ceilings, `lets_go`.
typedef struct {
    bool used;
    int jumps;
    bool submerged;
    bool lets_go;
} class_use;
static class_use used_classes[ENTITY_FALL_CLASSES_TOTAL]
                             [ENTITY_RUN_CLASSES_TOTAL]
                             [ENTITY_SIZE_CLASSES_TOTAL] = {0};

// Steps of the trajectory being generated, a `generated_step` dynarr.
static dynarr generating = (dynarr){0};

// The trajectory whose made up tiles `get_virtual_tile()` reads.
static const trajectory* virtual_trajectory = NULL;

static pool entities = (pool){0};
static SDL_Rect sim = (SDL_Rect){0};
// Whether it's in the middle of `ENTITY_update()`.
static bool updating = false;

// The path search's nodes, open nodes on a binary heap ordered by `f`, and the
// moves out of the node being expanded. Nodes are only searched within the
// simulation space, so `stamps` and `indexes` map its tiles to their nodes on
// the current search, tiles whose stamp isn't the current `search_id` having
// none yet. That way, nothing has to be cleared between searches.
static dynarr nodes = (dynarr){0};
static dynarr heap = (dynarr){0};
static dynarr moves = (dynarr){0};
static Uint32 stamps[SIMULATION_SPACE_X][SIMULATION_SPACE_Y] = {0};
static Uint32 indexes[SIMULATION_SPACE_X][SIMULATION_SPACE_Y] = {0};
static Uint32 search_id = 0;

// The fluids on the tiles within the simulation space, read the first time
// they're needed on each update, tiles whose stamp isn't `fluids_id` not read
// yet on this one.
static Uint32 fluid_stamps[SIMULATION_SPACE_X][SIMULATION_SPACE_Y] = {0};
static tile_fluids fluid_tiles[SIMULATION_SPACE_X][SIMULATION_SPACE_Y] = {0};
static Uint32 fluids_id = 1;

// Where fluids the entity avoids are like any other while finding moves, so
// nodes with them are nodes and moves can go through and land on them:
// everywhere while `ignoring_unsafe`, or only on the node a search starts
// from, `unsafe_start_x,unsafe_start_y`, so it can get out of them. They're
// set by whatever finds moves, and unset once it's done.
static bool ignoring_unsafe = false;
static int unsafe_start_x = -1;
static int unsafe_start_y = -1;

static bool is_unsafe_ok(int x, int y) {
    return ignoring_unsafe || (x == unsafe_start_x && y == unsafe_start_y);
}

static entity_data* get_entity(size_t id) {
    return pool_at(id, &entities);
}

static search_node* node_at(Uint32 idx) {
    return dynarr_at(idx, &nodes);
}

static path_move* move_at(entity_data* d, size_t n) {
    return n < d->path.len ? dynarr_at(n, &d->path) : NULL;
}

// Whether `e` can move other than flying: running, jumping or climbing.
static bool has_ground_moves(const entity* e) {
    return e->moves &
           (move_run | move_jump | move_climb_wall | move_climb_ceiling);
}

// Whether `e` can only move flying.
static bool only_flies(const entity* e) {
    return e->moves & move_fly && !has_ground_moves(e);
}

// The entity of `d` as it moves now: with no move but `move_fly` while it
// flies, and without it otherwise. Everything that finds and does its moves
// sees it like that, so what it does depends on whether it's flying, not on
// what it can do.
static entity moving_as(const entity_data* d) {
    entity e = d->entity;
    e.moves = d->flying ? (Uint32)move_fly : e.moves & ~(Uint32)move_fly;
    return e;
}


// Whether `e` swims with its body deep enough in a fluid to swim, `under`.
static bool swims_in(const entity* e, bool under) {
    return under && e->moves & move_swim;
}

// How `e` moves with its body deep enough in a fluid to swim if `under`:
// swimming if it can, otherwise with the `ENTITY_UNDERFLUID_` factors. Out of
// them, it flies with its fly class' values, without gravity, if it has
// `move_fly`, which entities only have as they move while flying, see
// `moving_as()`.
static movement get_movement(const entity* e, bool under) {
    if (e->moves & move_fly && !under)
        return (movement){
            .max_speed = flies[e->fly].max_speed,
            .acceleration = flies[e->fly].acceleration,
            .friction = flies[e->fly].friction,
            .jump_factor = 1.0,
        };
    if (swims_in(e, under))
        return (movement){
            .max_speed = swims[e->swim].max_speed,
            .acceleration = swims[e->swim].acceleration,
            .friction = swims[e->swim].friction,
            .swims = true,
            .jump_speed = swims[e->swim].jump_speed,
        };
    movement mv = (movement){
        .max_speed = runs[e->run].max_speed,
        .acceleration = runs[e->run].acceleration,
        .friction = runs[e->run].friction,
        .gravity = falls[e->fall].gravity,
        .max_fall_speed = falls[e->fall].max_fall_speed,
        .jump_factor = 1.0,
    };
    if (under) {
        mv.max_speed *= ENTITY_UNDERFLUID_MAX_SPEED_FACTOR;
        mv.acceleration *= ENTITY_UNDERFLUID_ACCELERATION_FACTOR;
        mv.friction *= ENTITY_UNDERFLUID_FRICTION_FACTOR;
        mv.gravity *= ENTITY_UNDERFLUID_GRAVITY_FACTOR;
        mv.max_fall_speed *= ENTITY_UNDERFLUID_MAX_FALL_SPEED_FACTOR;
        mv.jump_factor = ENTITY_UNDERFLUID_JUMP_SPEED_FACTOR;
    }
    return mv;
}



static tile_kind get_kind(tile t) {
    if (!t.type) return kind_empty;
    return t.platform ? kind_platform : kind_solid;
}

// Tiles outside the tilemap are solid, like they are for bodies.
static tile_kind get_real_kind(int x, int y) {
    if (x < 0 || y < 0 || x >= TILEMAP_SIZE_X || y >= TILEMAP_SIZE_Y)
        return kind_solid;
    tile t = (tile){0};
    TILEMAP_get_at(x, y, &t);
    return get_kind(t);
}

// Kind of the made up tile `(x,y)` relative to the starting tile of `t`.
static tile_kind get_virtual_kind(const trajectory* t, int x, int y) {
    if (x < t->x0 || x > t->x1 || y < t->y0 || y > t->y1) return kind_empty;
    return t->platform ? kind_platform : kind_solid;
}

// Made up tiles for `PHYSICS_simulate()`, the ones of `virtual_trajectory`
// with its starting tile at `(VIRTUAL_X,VIRTUAL_Y)`.
static int get_virtual_tile(Uint16 x, Uint16 y, tile* data) {
    if (x >= TILEMAP_SIZE_X || y >= TILEMAP_SIZE_Y) return 1;
    *data = (tile){0};
    tile_kind kind =
        get_virtual_kind(virtual_trajectory, x - VIRTUAL_X, y - VIRTUAL_Y);
    if (kind != kind_empty)
        *data = (tile){.type = tile_stone, .platform = kind == kind_platform};
    return 0;
}

static bool is_solid(int x, int y) {
    return get_real_kind(x, y) == kind_solid;
}

static bool is_platform(int x, int y) {
    return get_real_kind(x, y) == kind_platform;
}

// Tile columns and rows a body at `(x,y)` covers, snapped to pixels like the
// physics do.
static void get_span(const body* b,
                     double x,
                     double y,
                     int* from_x,
                     int* to_x,
                     int* from_y,
                     int* to_y) {
    double l = SDL_round(x);
    double t = SDL_round(y);
    *from_x = (int)SDL_floor(l / SPRITESHEET_CELL_X);
    *to_x = (int)SDL_floor((l + b->w - 1.0) / SPRITESHEET_CELL_X);
    *from_y = (int)SDL_floor(t / SPRITESHEET_CELL_Y);
    *to_y = (int)SDL_floor((t + b->h - 1.0) / SPRITESHEET_CELL_Y);
}

// Where a body standing on node `(x,y)` is, centered on it.
static double node_pos_x(const body* b, int x) {
    return x * SPRITESHEET_CELL_X + (SPRITESHEET_CELL_X - b->w) / 2.0;
}

static double node_pos_y(const body* b, int y) {
    return (y + 1) * SPRITESHEET_CELL_Y - b->h;
}

// Where a body like `b` is centered vertically on node `y`.
static double node_mid_y(const body* b, int y) {
    return y * SPRITESHEET_CELL_Y + (SPRITESHEET_CELL_Y - b->h) / 2.0;
}

static double node_center_x(int x) {
    return x * SPRITESHEET_CELL_X + SPRITESHEET_CELL_X / 2.0;
}

static bool is_within_sim(int x, int y) {
    return x >= sim.x && x < sim.x + sim.w && y >= sim.y && y < sim.y + sim.h;
}

static tile_fluids read_tile_fluids(int x, int y) {
    tile_fluids f = (tile_fluids){0};
    if (x < 0 || y < 0 || x >= TILEMAP_SIZE_X || y >= TILEMAP_SIZE_Y) return f;
    fluid_cell cells[FLUIDS_SPLIT_X][FLUIDS_SPLIT_Y];
    FLUIDS_get_at(x * FLUIDS_SPLIT_X,
                  y * FLUIDS_SPLIT_Y,
                  FLUIDS_SPLIT_X,
                  FLUIDS_SPLIT_Y,
                  &cells);
    for (int i = 0; i < FLUIDS_SPLIT_X; i++) {
        for (int j = 0; j < FLUIDS_SPLIT_Y; j++) {
            int bit = i + FLUIDS_SPLIT_X * j;
            double total = 0.0;
            for (int type = 0; type < FLUIDS_TOTAL; type++) {
                double volume = cells[i][j].volume[type];
                total += volume;
                if (volume >= ENTITY_UNSAFE_FLUID_VOL_THRESHOLD)
                    f.fluids[bit] |= 1 << type;
            }
            if (total >= FLUIDS_INTERACTION_VOL_THRESHOLD) f.full |= 1 << bit;
        }
    }
    return f;
}

// Fluids on tile `(x,y)`, read once per update for tiles within the simulation
// space, so path searches don't read the same ones over and over.
static tile_fluids get_tile_fluids(int x, int y) {
    if (!is_within_sim(x, y)) return read_tile_fluids(x, y);
    int i = x - sim.x;
    int j = y - sim.y;
    if (fluid_stamps[i][j] != fluids_id) {
        fluid_tiles[i][j] = read_tile_fluids(x, y);
        fluid_stamps[i][j] = fluids_id;
    }
    return fluid_tiles[i][j];
}

static bool is_dry(int x, int y) {
    tile_fluids f = get_tile_fluids(x, y);
    if (f.full) return false;
    for (int i = 0; i < FLUIDS_SPLIT_X * FLUIDS_SPLIT_Y; i++)
        if (f.fluids[i]) return false;
    return true;
}

// What fluids do to a body like `b` at `(x,y)`, for an entity that can go in
// `safe` fluids. Like the player, it swims when every fluid cell it overlaps
// has enough fluid to swim in, walls counting as full.
static body_fluids
get_body_fluids(Uint32 safe, const body* b, double x, double y) {
    double l = SDL_round(x);
    double t = SDL_round(y);
    int from_x = (int)SDL_floor(l / FLUIDS_CELL_W);
    int to_x = (int)SDL_floor((l + b->w - 1.0) / FLUIDS_CELL_W);
    int from_y = (int)SDL_floor(t / FLUIDS_CELL_H);
    int to_y = (int)SDL_floor((t + b->h - 1.0) / FLUIDS_CELL_H);
    body_fluids f = (body_fluids){.swimming = true};
    for (int cx = from_x; cx <= to_x; cx++) {
        for (int cy = from_y; cy <= to_y; cy++) {
            int tx = (int)SDL_floor(cx / (double)FLUIDS_SPLIT_X);
            int ty = (int)SDL_floor(cy / (double)FLUIDS_SPLIT_Y);
            int bit = (cx - tx * FLUIDS_SPLIT_X) +
                      FLUIDS_SPLIT_X * (cy - ty * FLUIDS_SPLIT_Y);
            tile_fluids tf = get_tile_fluids(tx, ty);
            if (tf.fluids[bit] & ~safe) f.unsafe = true;
            if (!(tf.full & 1 << bit) && !is_solid(tx, ty)) f.swimming = false;
        }
    }
    return f;
}

// Whether the body fits at node `(x,y)` without overlapping any wall.
static bool fits_at(const body* b, int x, int y) {
    double px = node_pos_x(b, x);
    double py = node_pos_y(b, y);
    if (px < 0.0 || py < 0.0 ||
        px + b->w > TILEMAP_SIZE_X * SPRITESHEET_CELL_X ||
        py + b->h > TILEMAP_SIZE_Y * SPRITESHEET_CELL_Y)
        return false;
    int from_x, to_x, from_y, to_y;
    get_span(b, px, py, &from_x, &to_x, &from_y, &to_y);
    for (int i = from_x; i <= to_x; i++)
        for (int j = from_y; j <= to_y; j++)
            if (is_solid(i, j)) return false;
    return true;
}

// Whether the body at node `(x,y)` has floor under it.
static bool has_floor(const body* b, int x, int y) {
    int from_x, to_x, from_y, to_y;
    get_span(
        b, node_pos_x(b, x), node_pos_y(b, y), &from_x, &to_x, &from_y, &to_y);
    for (int i = from_x; i <= to_x; i++)
        if (is_solid(i, y + 1) || is_platform(i, y + 1)) return true;
    return false;
}

// Whether `e` climbs anything.
static bool climbs(const entity* e) {
    return e->moves & (move_climb_wall | move_climb_ceiling);
}

// Whether `e` can cling on tile `(x,y)` to a wall beside it or a ceiling over
// it.
static bool can_cling_at(const entity* e, int x, int y) {
    if (e->moves & move_climb_wall &&
        (is_solid(x - 1, y) || is_solid(x + 1, y)))
        return true;
    return e->moves & move_climb_ceiling && is_solid(x, y - 1);
}

// What node `(x,y)` is to the entity `e` with the body `b`. Where it would
// swim, it's a node only if it can swim, and nowhere with fluids it avoids is,
// unless they're ok there, see `is_unsafe_ok()`. Entities that fly fly
// anywhere else, floor or not.
// Without a floor, it's one where it climbs, if it can cling to something
// there.
static node_kind find_node_kind(const entity* e, const body* b, int x, int y) {
    if (!fits_at(b, x, y)) return node_none;
    body_fluids f =
        get_body_fluids(e->safe_fluids, b, node_pos_x(b, x), node_pos_y(b, y));
    if (f.unsafe && !is_unsafe_ok(x, y)) return node_none;
    if (swims_in(e, f.swimming)) return node_swim;
    if (e->moves & move_fly && !f.swimming) return node_fly;
    if (has_floor(b, x, y)) return node_ground;
    return can_cling_at(e, x, y) ? node_climb : node_none;
}

// What each node within the simulation space is on the search being done, the
// ones whose stamp is `kinds_id`, while `caching_kinds`. Searches look at the
// same nodes over and over, but they're all the same for one, the entity, its
// body and where fluids it avoids are ok all being the same.
static Uint32 kind_stamps[SIMULATION_SPACE_X][SIMULATION_SPACE_Y] = {0};
static node_kind kinds[SIMULATION_SPACE_X][SIMULATION_SPACE_Y] = {0};
static Uint32 kinds_id = 0;
static bool caching_kinds = false;

// What node `(x,y)` is to `e` with the body `b`, see `find_node_kind()`, read
// from the cache while searching.
static node_kind get_node_kind(const entity* e, const body* b, int x, int y) {
    if (!caching_kinds || !is_within_sim(x, y))
        return find_node_kind(e, b, x, y);
    int i = x - sim.x;
    int j = y - sim.y;
    if (kind_stamps[i][j] != kinds_id) {
        kinds[i][j] = find_node_kind(e, b, x, y);
        kind_stamps[i][j] = kinds_id;
    }
    return kinds[i][j];
}

// Whether the body at node `(x,y)` only has platforms under it.
static bool is_on_platform(const body* b, int x, int y) {
    int from_x, to_x, from_y, to_y;
    get_span(
        b, node_pos_x(b, x), node_pos_y(b, y), &from_x, &to_x, &from_y, &to_y);
    bool found = false;
    for (int i = from_x; i <= to_x; i++) {
        if (is_solid(i, y + 1)) return false;
        if (is_platform(i, y + 1)) found = true;
    }
    return found;
}

// The tile the center of the body `b` is on.
static void get_center_tile(const body* b, int* x, int* y) {
    *x = (int)SDL_floor((SDL_round(b->x) + b->w / 2.0) / SPRITESHEET_CELL_X);
    *y = (int)SDL_floor((SDL_round(b->y) + b->h / 2.0) / SPRITESHEET_CELL_Y);
}

// What the body `b` of `e` is touching that it climbs, on the tile its center
// is on: a wall right by one of its sides or a ceiling right over it, rounded
// to pixels like the physics do.
static entity_surface get_touching(const entity* e, const body* b) {
    int x, y;
    get_center_tile(b, &x, &y);
    if (e->moves & move_climb_wall) {
        if (SDL_round(b->x + b->w) == (x + 1) * SPRITESHEET_CELL_X &&
            is_solid(x + 1, y))
            return surface_wall_right;
        if (SDL_round(b->x) == x * SPRITESHEET_CELL_X && is_solid(x - 1, y))
            return surface_wall_left;
    }
    if (e->moves & move_climb_ceiling &&
        SDL_round(b->y) == y * SPRITESHEET_CELL_Y && is_solid(x, y - 1))
        return surface_ceiling;
    return surface_none;
}

// The node the body `b` of `e` is on. On the floor, it's the one with its
// center if it can stand there, otherwise the closest one under it it can stand
// on, like when it's on a ledge with its center past it. Otherwise, it's the
// one with its center if it swims there, or the one under it, since it bobs up
// past it at the surface, or the one with its center if it's clinging to
// something or flying there. Returns `node_none` when it's on none.
static node_kind get_body_node(
    const entity* e, const body* b, bool on_floor, Uint16* x, Uint16* y) {
    int from_x, to_x, from_y, to_y;
    get_span(b, b->x, b->y, &from_x, &to_x, &from_y, &to_y);
    int center, middle;
    get_center_tile(b, &center, &middle);
    if (on_floor) {
        int best = -1;
        for (int i = from_x; i <= to_x; i++) {
            if (get_node_kind(e, b, i, to_y) != node_ground) continue;
            if (best < 0 || SDL_abs(i - center) < SDL_abs(best - center))
                best = i;
        }
        if (best >= 0) {
            *x = best;
            *y = to_y;
            return node_ground;
        }
    }
    for (int j = middle; j <= middle + 1; j++) {
        if (get_node_kind(e, b, center, j) != node_swim) continue;
        *x = center;
        *y = j;
        return node_swim;
    }
    if (climbs(e) && get_touching(e, b) != surface_none &&
        get_node_kind(e, b, center, middle) == node_climb) {
        *x = center;
        *y = middle;
        return node_climb;
    }
    if (e->moves & move_fly &&
        get_node_kind(e, b, center, middle) == node_fly) {
        *x = center;
        *y = middle;
        return node_fly;
    }
    return node_none;
}



// Same as `get_body_node()`, but on nodes with fluids the entity avoids too, so
// it finds where it is wherever it is.
static node_kind find_body_node(
    const entity* e, const body* b, bool on_floor, Uint16* x, Uint16* y) {
    bool ignoring = ignoring_unsafe;
    ignoring_unsafe = true;
    node_kind kind = get_body_node(e, b, on_floor, x, y);
    ignoring_unsafe = ignoring;
    return kind;
}



// Horizontal speed after a tick of `dt` holding `dir`, same as the player's
// movement on the floor.
static double run_speed(const movement* mv, double vx, Sint8 dir, double dt) {
    double new_x = vx;
    // When horizontal speed is greater than max (knockback or something)
    if (SDL_fabs(vx) > mv->max_speed) {
        new_x += vx > 0.0f ? -mv->friction * dt : mv->friction * dt;
    } else if (dir) {
        // Holding the direction against its current speed
        if (dir * new_x < 0.0f)
            new_x += new_x > 0.0f ? -(mv->friction + mv->acceleration) * dt
                                  : (mv->friction + mv->acceleration) * dt;
        // Holding the direction of its current speed
        else
            new_x = SDL_clamp(new_x + dir * mv->acceleration * dt,
                              -mv->max_speed,
                              mv->max_speed);
    }
    // Not holding any direction
    else if (new_x != 0.0f) {
        new_x += new_x > 0.0f ? -mv->friction * dt : mv->friction * dt;
        if (new_x * vx < 0.0f) new_x = 0.0f;
    }
    return new_x;
}

// Whether the direction of `m` is still held on the tick `t` seconds into it.
// Hold times are often whole ticks, and times added up in different ways can
// be off by a little from each other, landing on either side of the hold time,
// so it lets go half a tick earlier to always count the same ticks.
static bool is_holding_dir(const path_move* m, double t) {
    return t < m->dir_time - 0.5 / FIXED_T_TARGET_FPS;
}

// Vertical speed after a tick of `dt` holding `dir` vertically, which is only
// held swimming. Otherwise, it falls.
static double
vertical_speed(const movement* mv, double vy, Sint8 dir, double dt) {
    if (mv->swims) return run_speed(mv, vy, dir, dt);
    return SDL_min(vy + mv->gravity * dt, mv->max_fall_speed);
}

// Velocity after a tick of `dt` that's `t` seconds into the move `m`, which
// started at rest. This is what both simulates moves and executes them, so
// they do the same.
static void control_move(const movement* mv,
                         const path_move* m,
                         double t,
                         double dt,
                         double* vx,
                         double* vy) {
    *vx = run_speed(mv, *vx, is_holding_dir(m, t) ? m->dir : 0, dt);
    if (m->move == motion_jump && t <= 0.0)
        *vy = mv->swims ? -mv->jump_speed
                        : -jumps[m->jump].jump_speed * mv->jump_factor;
    *vy = vertical_speed(mv, *vy, 0, dt);
}

// Direction to hold to get from `x` to `to`, both horizontal positions of the
// body's center, going at `vx`. When `brake`ing, it lets go once friction alone
// would stop it there, so it can stop on it instead of passing by it.
static Sint8
steer(const movement* mv, double x, double vx, double to, bool brake) {
    double dx = to - x;
    if (SDL_fabs(dx) <= ENTITY_ALIGN_TOLERANCE) return 0;
    Sint8 dir = dx > 0.0 ? 1 : -1;
    if (brake && dir * vx > 0.0 &&
        vx * vx / (2.0 * mv->friction) >= SDL_fabs(dx))
        return 0;
    return dir;
}



// Whether the move `m` ended on the tick `t` seconds into it, its body being
// on the floor if `on_floor`, swimming if `swimming` and going at `vx`. It
// lands when it gets on the floor after leaving it, or gets deep enough in a
// fluid to swim, unless it started swimming and didn't get out yet. It stops
// when it never left the floor and isn't going anywhere anymore. This is what
// both simulates moves and executes them, so they end the same.
static progress_end advance(move_progress* p,
                            const path_move* m,
                            double t,
                            bool on_floor,
                            bool swimming,
                            double vx) {
    if (swimming) {
        if (!p->wet || p->left_water) return progress_landed;
    } else if (p->wet)
        p->left_water = true;
    if (!on_floor) {
        p->airborne = true;
        return progress_going;
    }
    if (p->airborne) return progress_landed;
    if (!is_holding_dir(m, t) && SDL_fabs(vx) <= EPSILON)
        return progress_stopped;
    return progress_going;
}

// Steps a simulated move on the real tilemap, swimming in fluids. It gives up
// without landing when it touches a fluid the entity avoids, unless it started
// in one and didn't get out of it yet, or it's `ignoring_unsafe`. The first
// step was already checked by whoever started it.
static bool sim_step(body* b, double t, physics_hits hits, void* arg) {
    sim_state* s = arg;
    double at = s->t0 + t;
    body_fluids f = get_body_fluids(s->e->safe_fluids, b, b->x, b->y);
    if (!f.unsafe)
        s->started_unsafe = false;
    else if (!s->started_unsafe && !ignoring_unsafe)
        return false;
    if (t > 0.0) {
        progress_end end = advance(&s->progress,
                                   s->m,
                                   at,
                                   hits.floor,
                                   swims_in(s->e, f.swimming),
                                   b->vel_x);
        if (end == progress_landed) {
            s->landed = true;
            s->on_floor = hits.floor;
            s->time = at;
        }
        if (end != progress_going) return false;
    }
    movement mv = get_movement(s->e, f.swimming);
    control_move(&mv, s->m, at, 1.0 / FIXED_T_TARGET_FPS, &b->vel_x, &b->vel_y);
    return true;
}

// Records each state of a trajectory being generated, with the same rules of
// when it ends as `sim_step()`, on made up tiles without fluids.
static bool gen_step(body* b, double t, physics_hits hits, void* arg) {
    gen_state* g = arg;
    bool ends = false;
    if (t > 0.0) {
        progress_end end =
            advance(&g->progress, g->m, t, hits.floor, false, b->vel_x);
        if (end == progress_landed) g->end = trajectory_landed;
        if (end == progress_stopped) g->end = trajectory_stopped;
        ends = end != progress_going;
    }
    generated_step s = (generated_step){
        .state = {.x = b->x, .y = b->y, .vx = b->vel_x, .vy = b->vel_y},
        .airborne = g->progress.airborne,
    };
    if (!ends)
        control_move(
            g->mv, g->m, t, 1.0 / FIXED_T_TARGET_FPS, &b->vel_x, &b->vel_y);
    s.step_vx = b->vel_x;
    s.step_vy = b->vel_y;
    push_to_dynarr(&s, g->steps);
    return !ends;
}

static int pos_to_tile_x(double x) {
    return (int)SDL_floor(x / SPRITESHEET_CELL_X);
}

static int pos_to_tile_y(double y) {
    return (int)SDL_floor(y / SPRITESHEET_CELL_Y);
}

// Tiles around the made up starting tile a trajectory can reach, in each
// direction, which is as far as it goes at the fastest anything moves for
// `ENTITY_MOVE_MAX_TIME`, more or less. `cell_stamps` has the tiles added to
// the trajectory being generated, the ones whose stamp is `cells_id`, so
// they're only added once without looking through them.
#define CELLS_REACH 64
static Uint32 cell_stamps[2 * CELLS_REACH][2 * CELLS_REACH] = {0};
static Uint32 cells_id = 0;

static void new_cells_generation(void) {
    cells_id++;
}

// Adds the tile `(x,y)` on the made up tiles to `t` if it doesn't have it yet,
// as first needed on its step `step`. Tiles past `CELLS_REACH` are looked for
// one by one.
static void add_cell(trajectory* t, int x, int y, size_t step) {
    x -= VIRTUAL_X;
    y -= VIRTUAL_Y;
    if (SDL_abs(x) < CELLS_REACH && SDL_abs(y) < CELLS_REACH) {
        Uint32* stamp = &cell_stamps[x + CELLS_REACH][y + CELLS_REACH];
        if (*stamp == cells_id) return;
        *stamp = cells_id;
    } else {
        for (size_t c = 0; c < t->cells.len; c++) {
            trajectory_cell* cell = dynarr_at(c, &t->cells);
            if (cell->x == x && cell->y == y) return;
        }
    }
    push_to_dynarr(&(trajectory_cell){.x = x, .y = y, .step = step}, &t->cells);
}

// Adds to `t` the tiles that can change what the physics do on the step from
// the state `i` of `steps`, its `generated_step`s, to the next, with a body
// like `shape`, that it doesn't have yet. These are the ones
// `move_body_horizontally()` and `move_body_vertically()` on `physics.c` would
// collide with: moving sideways, the tiles from the side it's going towards to
// where it ends up, on the rows the body overlaps, then moving up or down, the
// tiles from the top or bottom to where it ends up, on the columns the body
// overlaps after moving sideways. Tiles the body only touches by a side, like
// the floor under it, can't stop it moving any other way, so they're left out.
// Positions are rounded to pixels like the physics do.
static void
add_cells(trajectory* t, const dynarr* steps, size_t i, body shape) {
    const double dt = 1.0 / FIXED_T_TARGET_FPS;
    const generated_step* step = dynarr_at(i, steps);
    const trajectory_step* a = &step->state;
    const trajectory_step* b =
        &((generated_step*)dynarr_at(i + 1, steps))->state;
    double dx = dt * step->step_vx;
    double dy = dt * step->step_vy;
    double top = SDL_round(a->y);
    double bottom = SDL_round(a->y + shape.h);

    // Moving sideways, at the height it starts at
    double l = SDL_round(a->x);
    double r = SDL_round(a->x + shape.w);
    int from_x = dx >= 0.0 ? pos_to_tile_x(r) : pos_to_tile_x(l + dx);
    int to_x = dx >= 0.0 ? pos_to_tile_x(r + dx) : pos_to_tile_x(l);
    for (int x = from_x; x <= to_x; x++) {
        for (int y = pos_to_tile_y(top); y <= pos_to_tile_y(bottom); y++) {
            if (bottom <= SPRITESHEET_CELL_Y * y ||
                top >= SPRITESHEET_CELL_Y * (y + 1))
                continue;
            add_cell(t, x, y, i);
        }
    }

    // Moving up or down, where it ended up sideways
    l = SDL_round(b->x);
    r = SDL_round(b->x + shape.w);
    int from_y = dy >= 0.0 ? pos_to_tile_y(bottom) : pos_to_tile_y(top + dy);
    int to_y = dy >= 0.0 ? pos_to_tile_y(bottom + dy) : pos_to_tile_y(top);
    for (int x = pos_to_tile_x(l); x <= pos_to_tile_x(r); x++) {
        if (r <= SPRITESHEET_CELL_X * x || l >= SPRITESHEET_CELL_X * (x + 1))
            continue;
        for (int y = from_y; y <= to_y; y++)
            add_cell(t, x, y, i);
    }
}

// Horizontal speed of the start speed `i`, for an entity in the run class `r`
// in `md`.
static double start_speed(run_class r, int i, medium md) {
    if (i == 0) return 0.0;
    double s = start_speeds[(i + 1) / 2] * runs[r].max_speed;
    if (md == medium_submerged) s *= ENTITY_UNDERFLUID_MAX_SPEED_FACTOR;
    return i % 2 ? s : -s;
}

// Where a body like `b` starts on node `x` from the start offset `offset`,
// whole pixels away from being centered on it.
static double start_pos_x(const body* b, int x, int offset) {
    return node_pos_x(b, x) + (offset - START_OFFSETS / 2);
}

// Moves are tried without holding a direction and holding each direction for
// each of `dir_hold_times`, the hold `i` of `HOLDS` of them, which this sets
// on `m`, `0` being without one.
#define HOLDS (1 + 2 * ENTITY_DIR_HOLD_TIMES_TOTAL)
static void set_hold(path_move* m, int i) {
    m->dir = i == 0 ? 0 : i % 2 ? 1 : -1;
    m->dir_time = i == 0 ? 0.0 : dir_hold_times[(i - 1) / 2];
}

// Generates the trajectory of `t`, its move and made up tiles, with `mv`, for
// a body the size of `size` starting at `(x,y)` going `vx`, relative to the
// made up starting tile, and pushes it to `set`. Made up tiles full of fluids
// are only full to `mv`, which has to be how the body moves in them.
static void generate(dynarr* set,
                     const movement* mv,
                     size_values size,
                     trajectory t,
                     double x,
                     double y,
                     double vx) {
    body b = (body){
        .type = physics_actor,
        .w = size.w,
        .h = size.h,
        .x = x + VIRTUAL_X * SPRITESHEET_CELL_X,
        .y = y + VIRTUAL_Y * SPRITESHEET_CELL_Y,
        .vel_x = vx,
    };
    init_dynarr(&t.cells, sizeof(trajectory_cell));
    clear_dynarr(&generating);
    virtual_trajectory = &t;
    path_move m = inputs_move(t.inputs);

    gen_state g = (gen_state){
        .mv = mv,
        .m = &m,
        .steps = &generating,
        .end = trajectory_timed_out,
    };
    body shape = b;
    PHYSICS_simulate(&b,
                     ENTITY_MOVE_MAX_TIME,
                     t.drop_time,
                     gen_step,
                     &g,
                     NULL,
                     get_virtual_tile);
    // Timed out ones end after their last step, without recording its state
    if (g.end == trajectory_timed_out)
        push_to_dynarr(
            &(generated_step){
                .state = {.x = b.x, .y = b.y, .vx = b.vel_x, .vy = b.vel_y},
                .airborne = g.progress.airborne,
            },
            &generating);
    t.end = g.end;
    new_cells_generation();
    for (size_t i = 0; i + 1 < generating.len; i++)
        add_cells(&t, &generating, i, shape);
    shrink_dynarr(&t.cells);

    // Only what's needed to follow it is kept, without room to spare
    size_t len = generating.len;
    init_dynarr_with(&t.checkpoints,
                     sizeof(trajectory_step),
                     (len - 1) / checkpoint_ticks + 1);
    t.last = ((generated_step*)dynarr_at(len - 1, &generating))->state;
    t.last_step = len - 1;
    t.airborne_from = len;
    for (size_t i = 0; i < len; i++) {
        const generated_step* s = dynarr_at(i, &generating);
        if (i % checkpoint_ticks == 0)
            push_to_dynarr(&s->state, &t.checkpoints);
        if (s->airborne && t.airborne_from == len) t.airborne_from = i;
    }
    virtual_trajectory = NULL;
    push_to_dynarr(&t, set);
}

// Generates `m`, standing on the made up floor under the starting tile,
// platforms if `platform`, from the start offset `o` going `vx`, in `md`.
static void generate_standing(dynarr* set,
                              const movement* mv,
                              size_values size,
                              path_move m,
                              bool platform,
                              double drop_time,
                              int o,
                              double vx,
                              medium md) {
    body b = (body){.w = size.w, .h = size.h};
    double x = start_pos_x(&b, 0, o);
    double y = node_pos_y(&b, 0);
    int from_x, to_x, from_y, to_y;
    get_span(&b, x, y, &from_x, &to_x, &from_y, &to_y);
    trajectory t = (trajectory){
        .inputs = inputs_of(&m),
        .x0 = from_x,
        .x1 = to_x,
        .y0 = 1,
        .y1 = 1,
        .platform = platform,
        .drop_time = drop_time,
        .submerged = md == medium_submerged,
    };
    generate(set, mv, size, t, x, y, vx);
}

// The movement of the fall and run classes `f` and `r` in `md`.
static movement get_class_movement(fall_class f, run_class r, medium md) {
    return get_movement(&(entity){.fall = f, .run = r}, md == medium_submerged);
}

// Jumps with jump class `j`, with each of the `HOLDS`, from the start offset
// `o` and speed `v`, in `md`.
static void generate_jumps(jump_class j,
                           fall_class f,
                           run_class r,
                           size_class s,
                           int o,
                           int v,
                           medium md) {
    trajectory_set* set = &jump_sets[md][j][f][r][s][o][v];
    init_dynarr(&set->trajectories, sizeof(trajectory));
    movement mv = get_class_movement(f, r, md);
    double vx = start_speed(r, v, md);
    path_move m = (path_move){.move = motion_jump, .jump = j};
    for (int i = 0; i < HOLDS; i++) {
        set_hold(&m, i);
        generate_standing(
            &set->trajectories, &mv, sizes[s], m, false, 0.0, o, vx, md);
    }
    shrink_dynarr(&set->trajectories);
    set->generated = true;
}

// Drops from platforms with each of the `HOLDS`, and falls off ledges with
// each of them but the one without a direction, from the start offset `o` and
// speed `v`, in `md`.
static void generate_ground(
    fall_class f, run_class r, size_class s, int o, int v, medium md) {
    trajectory_set* set = &ground_sets[md][f][r][s][o][v];
    init_dynarr(&set->trajectories, sizeof(trajectory));
    movement mv = get_class_movement(f, r, md);
    double vx = start_speed(r, v, md);
    path_move m = (path_move){0};
    for (int i = 0; i < HOLDS; i++) {
        set_hold(&m, i);
        m.move = motion_drop;
        generate_standing(&set->trajectories,
                          &mv,
                          sizes[s],
                          m,
                          true,
                          PHYSICS_DROP_TIMER,
                          o,
                          vx,
                          md);
        if (i == 0) continue;
        m.move = motion_fall;
        generate_standing(
            &set->trajectories, &mv, sizes[s], m, false, 0.0, o, vx, md);
    }
    shrink_dynarr(&set->trajectories);
    set->generated = true;
}

// Where a body like `b` clings to `surface` on node `(x,y)`: against the wall
// on one of its sides, centered on it vertically, or against its ceiling,
// centered on it horizontally.
static void cling_spot(const body* b,
                       int x,
                       int y,
                       entity_surface surface,
                       double* px,
                       double* py) {
    *px = surface == surface_wall_right  ? (x + 1) * SPRITESHEET_CELL_X - b->w
          : surface == surface_wall_left ? x * SPRITESHEET_CELL_X
                                         : node_pos_x(b, x);
    *py =
        surface == surface_ceiling ? y * SPRITESHEET_CELL_Y : node_mid_y(b, y);
}

// Letting go of the wall or ceiling `surface` with each of the `HOLDS`, from
// rest, in `md`. A made up wall goes all the way down from where it clings, as
// far as trajectories can reach, since walls often do, so falling along it
// stays the same as long as the real one does.
static void generate_let_go(fall_class f,
                            run_class r,
                            size_class s,
                            entity_surface surface,
                            medium md) {
    trajectory_set* set = &let_go_sets[md][f][r][s][surface - 1];
    init_dynarr(&set->trajectories, sizeof(trajectory));
    movement mv = get_class_movement(f, r, md);
    body b = (body){.w = sizes[s].w, .h = sizes[s].h};
    double x, y;
    cling_spot(&b, 0, 0, surface, &x, &y);
    int from_x, to_x, from_y, to_y;
    get_span(&b, x, y, &from_x, &to_x, &from_y, &to_y);
    path_move m = (path_move){.move = motion_let_go};
    trajectory t = (trajectory){
        .x0 = surface == surface_wall_right  ? 1
              : surface == surface_wall_left ? -1
                                             : from_x,
        .x1 = surface == surface_wall_right  ? 1
              : surface == surface_wall_left ? -1
                                             : to_x,
        .y0 = surface == surface_ceiling ? -1 : from_y,
        .y1 = surface == surface_ceiling ? -1 : CELLS_REACH,
        .cling = surface,
        .submerged = md == medium_submerged,
    };
    for (int i = 0; i < HOLDS; i++) {
        set_hold(&m, i);
        t.inputs = inputs_of(&m);
        generate(&set->trajectories, &mv, sizes[s], t, x, y, 0.0);
    }
    shrink_dynarr(&set->trajectories);
    set->generated = true;
}

static dynarr*
get_jumps(jump_class j, const entity* e, int o, int v, medium md) {
    trajectory_set* set = &jump_sets[md][j][e->fall][e->run][e->size][o][v];
    if (!set->generated) generate_jumps(j, e->fall, e->run, e->size, o, v, md);
    return &set->trajectories;
}

static dynarr* get_ground_moves(const entity* e, int o, int v, medium md) {
    trajectory_set* set = &ground_sets[md][e->fall][e->run][e->size][o][v];
    if (!set->generated) generate_ground(e->fall, e->run, e->size, o, v, md);
    return &set->trajectories;
}

static dynarr*
get_let_goes(const entity* e, entity_surface surface, medium md) {
    trajectory_set* set =
        &let_go_sets[md][e->fall][e->run][e->size][surface - 1];
    if (!set->generated) generate_let_go(e->fall, e->run, e->size, surface, md);
    return &set->trajectories;
}

static void free_set(trajectory_set* set) {
    if (!set->generated) return;
    for (size_t i = 0; i < set->trajectories.len; i++) {
        trajectory* t = dynarr_at(i, &set->trajectories);
        deinit_dynarr(&t->checkpoints);
        deinit_dynarr(&t->cells);
    }
    deinit_dynarr(&set->trajectories);
    set->generated = false;
}



// Pushes `m` to `out`, or replaces the move there to the same node with it if
// it's cheaper.
static void push_move(path_move m, dynarr* out) {
    for (size_t i = 0; i < out->len; i++) {
        path_move* other = dynarr_at(i, out);
        if (other->to_x != m.to_x || other->to_y != m.to_y) continue;
        if (m.cost < other->cost) *other = m;
        return;
    }
    push_to_dynarr(&m, out);
}

// Pushes `m` to `out` if `b`, where `e` landed `time` seconds into it, on the
// floor if `on_floor`, is on another node. `m`'s cost is how long it waited
// to start, and the time it took is added to it. It gets to its node the way
// `b` is, which is only known on the floor.
static void push_landing(const entity* e,
                         path_move m,
                         body b,
                         bool on_floor,
                         double time,
                         dynarr* out) {
    node_kind kind = get_body_node(e, &b, on_floor, &m.to_x, &m.to_y);
    if (kind == node_none) return;
    if (m.to_x == m.from_x && m.to_y == m.from_y) return;
    m.cost += time;
    m.to_state = (node_state){
        .known = on_floor && kind == node_ground,
        .x = b.x,
        .vx = b.vel_x,
    };
    push_move(m, out);
}

// How long a body that started dropping down from platforms for `drop_time`
// has left of it after `steps` steps, taking a step's time out of it once each
// step like the physics do, so it's exactly the same.
static double drop_timer_after(double drop_time, int steps) {
    for (int i = 0; i < steps; i++)
        drop_time -= 1.0 / FIXED_T_TARGET_FPS;
    return drop_time;
}

// Simulates `m` of `e` with the physics on the real tiles, from the body `b`,
// `t0` seconds into it, with the step at `t0` already checked and `progress`
// being how it went by then, leaving `b` where it ended. Returns how it went,
// with whether it `landed`, on the floor if `on_floor`, `time` seconds into
// it. Moves starting in a fluid the entity avoids can go through it until they
// get out.
static sim_state simulate_landing(const entity* e,
                                  const path_move* m,
                                  body* b,
                                  double t0,
                                  move_progress progress,
                                  double drop_time) {
    sim_state s = (sim_state){
        .e = e,
        .m = m,
        .t0 = t0,
        .started_unsafe =
            t0 <= 0.0 && get_body_fluids(e->safe_fluids, b, b->x, b->y).unsafe,
        .progress = progress,
    };
    PHYSICS_simulate(
        b,
        ENTITY_MOVE_MAX_TIME - t0,
        drop_timer_after(drop_time, (int)SDL_lround(t0 * FIXED_T_TARGET_FPS)),
        sim_step,
        &s,
        NULL,
        NULL);
    return s;
}

// Simulates `m` of `e` with the physics on the real tiles, from the body `b`,
// `t0` seconds into it, with the step at `t0` already checked and `progress`
// being how it went by then. Moves starting in a fluid the entity avoids can
// go through it until they get out.
static void simulate_rest(const entity* e,
                          path_move m,
                          body b,
                          double t0,
                          move_progress progress,
                          double drop_time,
                          dynarr* out) {
    sim_state s = simulate_landing(e, &m, &b, t0, progress, drop_time);
    if (s.landed) push_landing(e, m, b, s.on_floor, s.time, out);
}

// Whether tile `(x,y)` is like a made up one full of fluids to `e`: every
// fluid cell of it full of fluids it doesn't avoid, unless `ignoring_unsafe`,
// or a wall, which the physics count as full.
static bool is_submerged_tile(const entity* e, int x, int y) {
    if (is_solid(x, y)) return true;
    tile_fluids f = get_tile_fluids(x, y);
    if (f.full != (1 << FLUIDS_SPLIT_X * FLUIDS_SPLIT_Y) - 1) return false;
    if (ignoring_unsafe) return true;
    for (int i = 0; i < FLUIDS_SPLIT_X * FLUIDS_SPLIT_Y; i++)
        if (f.fluids[i] & ~e->safe_fluids) return false;
    return true;
}

// What a body like `shape` of `e` at `(x,y)` is in, by the tiles it covers.
// Entities that swim are never submerged, since they swim instead.
static medium
get_medium(const entity* e, const body* shape, double x, double y) {
    int from_x, to_x, from_y, to_y;
    get_span(shape, x, y, &from_x, &to_x, &from_y, &to_y);
    bool dry = true;
    bool submerged = !(e->moves & move_swim);
    for (int i = from_x; i <= to_x; i++) {
        for (int j = from_y; j <= to_y; j++) {
            dry = dry && is_dry(i, j);
            submerged = submerged && is_submerged_tile(e, i, j);
        }
    }
    return dry ? medium_dry : submerged ? medium_submerged : medium_mixed;
}

// Whether the tile `c` of `t` is the same on the real tilemap to `e`, starting
// from node `(x,y)`, as on the made up tiles. Starting standing, the floor
// under the starting tile only has to be floor, solid or a platform, which the
// physics treat the same from above, but walls and ceilings let go of have to
// be solid. Made up tiles have no fluids, or are full of them if `t` is
// submerged.
static bool is_cell_the_same(const entity* e,
                             const trajectory* t,
                             const trajectory_cell* c,
                             int x,
                             int y) {
    if (t->submerged ? !is_submerged_tile(e, x + c->x, y + c->y)
                     : !is_dry(x + c->x, y + c->y))
        return false;
    tile_kind real = get_real_kind(x + c->x, y + c->y);
    tile_kind made_up = get_virtual_kind(t, c->x, c->y);
    if (made_up == kind_solid && t->cling == surface_none)
        return real != kind_empty;
    return real == made_up;
}

// What replaying a trajectory from a checkpoint needs, see `replay_step()`. It
// does `left` more steps, the next one being the step `at` of the trajectory.
typedef struct {
    const movement* mv;
    const path_move* m;
    int at, left;
} replay_state;

// Does a step of a trajectory being replayed the same as `gen_step()` did
// while generating it, stopping before the one it's replaying up to.
static bool
replay_step(body* b, double _t, physics_hits _hits, void* arg) { // NOLINT
    replay_state* r = arg;
    if (r->left <= 0) return false;
    control_move(r->mv,
                 r->m,
                 r->at * (1.0 / FIXED_T_TARGET_FPS),
                 1.0 / FIXED_T_TARGET_FPS,
                 &b->vel_x,
                 &b->vel_y);
    r->at++;
    r->left--;
    return true;
}

// The state of `t` of `e` on the step `k`, with a body like `shape`. The ones
// that weren't kept are simulated again on the made up tiles from the
// checkpoint before them, the same as when `t` was generated, so they're
// exactly the same.
static trajectory_step
get_step(const entity* e, const trajectory* t, body shape, int k) {
    if (k == t->last_step) return t->last;
    int c = k / checkpoint_ticks;
    const trajectory_step* s = dynarr_at(c, &t->checkpoints);
    if (k == c * checkpoint_ticks) return *s;

    virtual_trajectory = t;
    movement mv = get_movement(e, t->submerged);
    path_move m = inputs_move(t->inputs);
    replay_state r = (replay_state){
        .mv = &mv,
        .m = &m,
        .at = c * checkpoint_ticks,
        .left = k - c * checkpoint_ticks,
    };
    shape.x = s->x;
    shape.y = s->y;
    shape.vel_x = s->vx;
    shape.vel_y = s->vy;
    PHYSICS_simulate(&shape,
                     ENTITY_MOVE_MAX_TIME,
                     drop_timer_after(t->drop_time, r.at),
                     replay_step,
                     &r,
                     NULL,
                     get_virtual_tile);
    virtual_trajectory = NULL;
    return (trajectory_step){
        .x = shape.x, .y = shape.y, .vx = shape.vel_x, .vy = shape.vel_y};
}

// The body `shape` at state `s` of a trajectory, moved from the made up
// starting tile to node `(x,y)`.
static body get_state_body(body shape, const trajectory_step* s, int x, int y) {
    shape.x = s->x + (double)(x - VIRTUAL_X) * SPRITESHEET_CELL_X;
    shape.y = s->y + (double)(y - VIRTUAL_Y) * SPRITESHEET_CELL_Y;
    shape.vel_x = s->vx;
    shape.vel_y = s->vy;
    return shape;
}

// `m` from node `(x,y)`, starting with `start`, its cost being how long it
// waits to start until it's done. Letting go, it gets to `start` first.
static path_move
with_start(path_move m, int x, int y, const move_start* start) {
    m.from_x = x;
    m.from_y = y;
    m.start = start->mode;
    m.start_x = start->x;
    m.start_vx = start->vx;
    m.target_x = start->x;
    m.target_y = start->y;
    m.cost = start->wait;
    return m;
}

// Does `m` of `e` with the physics alone, from the body `shape` at `(x,y)`
// going `vx`, swimming if `wet`.
static void simulate_from(const entity* e,
                          path_move m,
                          body shape,
                          double x,
                          double y,
                          double vx,
                          double drop_time,
                          bool wet,
                          dynarr* out) {
    shape.x = x;
    shape.y = y;
    shape.vel_x = vx;
    shape.vel_y = 0.0;
    simulate_rest(
        e, m, shape, 0.0, (move_progress){.wet = wet}, drop_time, out);
}

// Does the move of `t` from node `(x,y)` with the physics alone, starting with
// `start`, like `follow_trajectory()` does without a trajectory to follow.
static void simulate_whole(const entity* e,
                           const trajectory* t,
                           body shape,
                           int x,
                           int y,
                           const move_start* start,
                           dynarr* out) {
    simulate_from(e,
                  with_start(inputs_move(t->inputs), x, y, start),
                  shape,
                  start->x,
                  start->y,
                  start->vx,
                  t->drop_time,
                  false,
                  out);
}

// Does the move of the trajectory `t` of `e` from node `(x,y)` on the real
// tilemap, with a body like `shape`, starting with `start`, which must be the
// start `t` was generated from, pushing it to `out` if it lands on another
// node. Moves starting in something other than what the trajectory was
// generated in are simulated whole.
static void follow_trajectory(const entity* e,
                              const trajectory* t,
                              body shape,
                              int x,
                              int y,
                              const move_start* start,
                              dynarr* out) {
    if (start->medium != (t->submerged ? medium_submerged : medium_dry)) {
        simulate_whole(e, t, shape, x, y, start, out);
        return;
    }
    path_move m = with_start(inputs_move(t->inputs), x, y, start);
    for (size_t c = 0; c < t->cells.len; c++) {
        const trajectory_cell* cell = dynarr_at(c, &t->cells);
        if (is_cell_the_same(e, t, cell, x, y)) continue;

        // It's the same up to the step that first needs a different tile, so
        // the physics take it from there, with the checks of that step done
        trajectory_step s = get_step(e, t, shape, cell->step);
        simulate_rest(
            e,
            m,
            get_state_body(shape, &s, x, y),
            cell->step / (double)FIXED_T_TARGET_FPS,
            (move_progress){.airborne = cell->step >= t->airborne_from},
            t->drop_time,
            out);
        return;
    }
    if (t->end != trajectory_landed) return;
    push_landing(e,
                 m,
                 get_state_body(shape, &t->last, x, y),
                 true,
                 t->last_step / (double)FIXED_T_TARGET_FPS,
                 out);
}

// Pixels from a node to the one `(dx,dy)` from it, for the ones next to it.
static double step_distance(int dx, int dy) {
    return SDL_sqrt(
        (double)(dx * dx * SPRITESHEET_CELL_X * SPRITESHEET_CELL_X +
                 dy * dy * SPRITESHEET_CELL_Y * SPRITESHEET_CELL_Y));
}

// Pushes to `out` the move of a body like `b` swimming from node `(x,y)` to
// where it stands or swims on the one at `(x + dx, y + dy)`, going `speed`
// pixels per second, starting from swimming if `from_swim`.
static void push_swim(const body* b,
                      int x,
                      int y,
                      int dx,
                      int dy,
                      double speed,
                      bool from_swim,
                      dynarr* out) {
    push_move(
        (path_move){
            .move = motion_swim,
            .from_x = x,
            .from_y = y,
            .to_x = x + dx,
            .to_y = y + dy,
            .cost = step_distance(dx, dy) / speed,
            .from_swim = from_swim,
            .target_x = node_pos_x(b, x + dx),
            .target_y = node_pos_y(b, y + dy),
        },
        out);
}

// Whether a body with its center at `x` going `vx` got to `to` running in
// `dir`, within half of what it moves in a tick, or past it. This is what both
// predicts runs and does them, so they end the same.
static bool has_reached(Sint8 dir, double x, double vx, double to) {
    return dir * (to - x) <= SDL_max(SDL_fabs(vx) / (2.0 * FIXED_T_TARGET_FPS),
                                     ENTITY_ALIGN_TOLERANCE);
}

// Runs a body like `b` at `bx` going `vx` in `dir` until its center gets to
// the center of node `to`, holding `dir` the whole way, setting where it ends
// up and how fast it's going, and on `time` how long it took. The physics move
// bodies by exactly their velocity when nothing's in the way, so it's done
// with arithmetic. Returns `false` when it doesn't get there in time.
static bool predict_run(const movement* mv,
                        const body* b,
                        int to,
                        Sint8 dir,
                        double* bx,
                        double* vx,
                        double* time) {
    const double dt = 1.0 / FIXED_T_TARGET_FPS;
    double target = node_center_x(to);
    for (int tick = 0; tick <= ENTITY_MOVE_MAX_TIME * FIXED_T_TARGET_FPS;
         tick++) {
        if (has_reached(dir, *bx + b->w / 2.0, *vx, target)) {
            *time = tick * dt;
            return true;
        }
        *vx = run_speed(mv, *vx, dir, dt);
        *bx += *vx * dt;
    }
    return false;
}

// Pushes to `out` the move running from node `(x,y)` to the one beside it in
// `dir`, starting the way `s` is, or from rest at its center if that's not
// known.
static void push_run(const entity* e,
                     const body* b,
                     int x,
                     int y,
                     Sint8 dir,
                     node_state s,
                     dynarr* out) {
    double bx = s.known ? s.x : node_pos_x(b, x);
    double vx = s.known ? s.vx : 0.0;
    movement mv = get_movement(
        e, get_body_fluids(e->safe_fluids, b, bx, node_pos_y(b, y)).swimming);
    double time = 0.0;
    if (!predict_run(&mv, b, x + dir, dir, &bx, &vx, &time)) return;
    push_move(
        (path_move){
            .move = motion_run,
            .dir = dir,
            .from_x = x,
            .from_y = y,
            .to_x = x + dir,
            .to_y = y,
            .cost = time,
            .to_state = (node_state){.known = true, .x = bx, .vx = vx},
        },
        out);
}

// The start offset of a body like `b` at `bx` on node `x`, `-1` when its
// center is off the node's tile.
static int get_offset(const body* b, int x, double bx) {
    double o = SDL_round(bx - node_pos_x(b, x)) + START_OFFSETS / 2;
    return o >= 0.0 && o < START_OFFSETS ? (int)o : -1;
}

// Whether a body like `b` at `bx` on the floor of row `y` doesn't overlap any
// wall and has floor under it.
static bool stands_at(const body* b, double bx, int y) {
    int from_x, to_x, from_y, to_y;
    get_span(b, bx, node_pos_y(b, y), &from_x, &to_x, &from_y, &to_y);
    bool floor = false;
    for (int i = from_x; i <= to_x; i++) {
        for (int j = from_y; j <= to_y; j++)
            if (is_solid(i, j)) return false;
        if (is_solid(i, y + 1) || is_platform(i, y + 1)) floor = true;
    }
    return floor;
}

// Where a body like `b` at `bx` going `vx` on the floor of row `y` comes to
// rest by itself, sliding with nothing held, on `rest_x`, and how long it
// takes on `time`. Returns `false` when it would go off the floor or into a
// wall on the way.
static bool predict_rest(const entity* e,
                         const body* b,
                         int y,
                         double bx,
                         double vx,
                         double* rest_x,
                         double* time) {
    movement mv = get_movement(
        e, get_body_fluids(e->safe_fluids, b, bx, node_pos_y(b, y)).swimming);
    const double dt = 1.0 / FIXED_T_TARGET_FPS;
    int ticks = 0;
    for (; SDL_fabs(vx) > EPSILON; ticks++) {
        if (ticks > ENTITY_MOVE_MAX_TIME * FIXED_T_TARGET_FPS) return false;
        vx = run_speed(&mv, vx, 0, dt);
        bx += vx * dt;
        if (!stands_at(b, bx, y)) return false;
    }
    *rest_x = bx;
    *time = ticks * dt;
    return true;
}

// The trajectories a move starting with `start` follows, the dry ones when
// it's in some of each, which are simulated whole anyway.
static medium set_medium(const move_start* start) {
    return start->medium == medium_mixed ? medium_dry : start->medium;
}

// Puts on `out` the ways moves other than running and swimming can start from
// node `(x,y)` with the body `b` getting there the way `s` is, returning how
// many. Going as fast as one of `ENTITY_START_SPEEDS` other than rest, in what
// it's in, it can start right away going that fast. Either way, it can also
// start from rest, once it comes to rest by itself. When where it'll be isn't
// known, it stops at the node's center first. With some fluids and some not,
// there are no trajectories, so it starts right away from exactly how it is.
static size_t get_starts(const entity* e,
                         const body* b,
                         int x,
                         int y,
                         node_state s,
                         move_start out[2]) {
    double standing = node_pos_y(b, y);
    move_start aligned = (move_start){
        .mode = start_aligned,
        .medium = get_medium(e, b, node_pos_x(b, x), standing),
        .offset = START_OFFSETS / 2,
        .x = node_pos_x(b, x),
        .y = standing,
    };
    if (!s.known) {
        out[0] = aligned;
        return 1;
    }

    medium md = get_medium(e, b, s.x, standing);
    medium speeds = md == medium_mixed ? medium_dry : md;
    size_t n = 0;
    int closest = 0;
    for (int i = 1; i < START_SPEEDS; i++)
        if (SDL_fabs(start_speed(e->run, i, speeds) - s.vx) <
            SDL_fabs(start_speed(e->run, closest, speeds) - s.vx))
            closest = i;
    int offset = get_offset(b, x, s.x);

    if (md == medium_mixed) {
        out[0] = (move_start){
            .mode = start_now,
            .medium = medium_mixed,
            .offset = offset >= 0 ? offset : START_OFFSETS / 2,
            .speed = closest,
            .x = s.x,
            .y = standing,
            .vx = s.vx,
        };
        return 1;
    }
    if (offset >= 0 && (closest || SDL_fabs(s.vx) <= EPSILON)) {
        out[n++] = (move_start){
            .mode = start_now,
            .medium = md,
            .offset = offset,
            .speed = closest,
            .x = start_pos_x(b, x, offset),
            .y = standing,
            .vx = start_speed(e->run, closest, md),
        };
        if (!closest) return n;
    }

    double rest_x = 0.0;
    double wait = 0.0;
    if (predict_rest(e, b, y, s.x, s.vx, &rest_x, &wait) &&
        (offset = get_offset(b, x, rest_x)) >= 0) {
        double start_x = start_pos_x(b, x, offset);
        out[n++] = (move_start){
            .mode = start_at_rest,
            .medium = get_medium(e, b, start_x, standing),
            .offset = offset,
            .x = start_x,
            .y = standing,
            .wait = wait,
        };
    } else
        out[n++] = aligned;
    return n;
}

// Whether a body like `b` going from node `(x,y)` to the one `(dx,dy)` from
// it gets past the corner between them when it's diagonal, fitting on both
// nodes beside it.
static bool clears_corner(const body* b, int x, int y, int dx, int dy) {
    return !dx || !dy || (fits_at(b, x + dx, y) && fits_at(b, x, y + dy));
}

// Puts on `out` every move `e` can do from node `(x,y)`, where it swims, with
// its body shaped like `b`: swimming to the nodes around it, the same in every
// direction, but not out of the fluid's surface, which it jumps out of
// instead, from its center. It only swims diagonally to where it swims, and
// past corners its body fits by.
static void get_swim_moves(const entity* e, body b, int x, int y, dynarr* out) {
    movement mv = get_movement(e, true);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            node_kind kind = get_node_kind(e, &b, x + dx, y + dy);
            if (kind == node_none) continue;
            if ((dy < 0 || (dx && dy)) && kind != node_swim) continue;
            if (!clears_corner(&b, x, y, dx, dy)) continue;
            push_swim(&b, x, y, dx, dy, mv.max_speed, true, out);
        }
    }

    // It's at the surface when it doesn't swim over it
    if (!(e->moves & move_jump) || get_node_kind(e, &b, x, y - 1) == node_swim)
        return;
    move_start start = (move_start){
        .mode = start_aligned,
        .x = node_pos_x(&b, x),
        .y = node_pos_y(&b, y),
    };
    path_move m = (path_move){
        .move = motion_jump,
        .jump = e->jump,
        .from_swim = true,
    };
    for (int i = 0; i < HOLDS; i++) {
        set_hold(&m, i);
        simulate_from(e,
                      with_start(m, x, y, &start),
                      b,
                      start.x,
                      start.y,
                      0.0,
                      0.0,
                      true,
                      out);
    }
}

// Where a body like `b` clinging on node `(x,y)` is: against the wall on its
// `side`, `-1` for left, `1` for right, or centered on it if `0`, and standing
// on its floor if it has one, otherwise against its ceiling if `ceiling`, or
// centered on it.
static void climb_pos(const body* b,
                      int x,
                      int y,
                      int side,
                      bool ceiling,
                      double* px,
                      double* py) {
    *px = side > 0   ? (x + 1) * SPRITESHEET_CELL_X - b->w
          : side < 0 ? x * SPRITESHEET_CELL_X
                     : node_pos_x(b, x);
    if (has_floor(b, x, y))
        *py = node_pos_y(b, y);
    else if (ceiling)
        *py = y * SPRITESHEET_CELL_Y;
    else
        *py = node_mid_y(b, y);
}

// Side of a wall beside node `(x,y)` `e` can climb, `1` for its right, which
// it prefers, `-1` for its left, `0` when there's none.
static int get_wall_side(const entity* e, int x, int y) {
    if (!(e->moves & move_climb_wall)) return 0;
    return is_solid(x + 1, y) ? 1 : is_solid(x - 1, y) ? -1 : 0;
}

// What `e` clings to on node `(x,y)` to let go of: a wall beside it,
// preferring its right, or its ceiling, `surface_none` when there's nothing.
static entity_surface get_cling_surface(const entity* e, int x, int y) {
    int side = get_wall_side(e, x, y);
    if (side) return side > 0 ? surface_wall_right : surface_wall_left;
    if (e->moves & move_climb_ceiling && is_solid(x, y - 1))
        return surface_ceiling;
    return surface_none;
}

// Pushes to `out` the move of `e` climbing from node `(x,y)` to the one at
// `(x + dx, y + dy)`, ending against the wall on its `side` or its ceiling if
// `ceiling`, like `climb_pos()`.
static void push_climb(const entity* e,
                       const body* b,
                       int x,
                       int y,
                       int dx,
                       int dy,
                       int side,
                       bool ceiling,
                       dynarr* out) {
    path_move m = (path_move){
        .move = ceiling || (!side && dx && !dy) ? motion_climb_ceiling
                                                : motion_climb_wall,
        .from_x = x,
        .from_y = y,
        .to_x = x + dx,
        .to_y = y + dy,
        .cost = step_distance(dx, dy) / runs[e->run].max_speed,
    };
    climb_pos(b, x + dx, y + dy, side, ceiling, &m.target_x, &m.target_y);
    push_move(m, out);
}

// Puts on `out` the moves of `e` climbing from node `(x,y)` with its body
// shaped like `b`: up and down a wall both nodes are beside, along a ceiling
// both are under, and around the corner of a wall or ceiling to the node on
// its other side, through the free one between them.
static void
get_climb_moves(const entity* e, body b, int x, int y, dynarr* out) {
    bool walls = e->moves & move_climb_wall;
    bool ceilings = e->moves & move_climb_ceiling;
    if (!walls && !ceilings) return;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            int tx = x + dx;
            int ty = y + dy;
            node_kind kind = get_node_kind(e, &b, tx, ty);
            if (kind != node_ground && kind != node_climb) continue;

            // Up or down a wall
            if (!dx) {
                if (!walls) continue;
                for (int side = -1; side <= 1; side += 2) {
                    if (!is_solid(x + side, y) || !is_solid(tx + side, ty))
                        continue;
                    push_climb(e, &b, x, y, dx, dy, side, false, out);
                    break;
                }
            }

            // Along a ceiling
            else if (!dy) {
                if (ceilings && is_solid(x, y - 1) && is_solid(tx, ty - 1))
                    push_climb(e, &b, x, y, dx, dy, 0, true, out);
            }

            // Around the corner of the wall beside it, onto its top going up
            // or under it going down
            else if (is_solid(tx, y) && !is_solid(x, ty)) {
                if (!walls || !fits_at(&b, x, ty) || (dy > 0 && !ceilings))
                    continue;
                push_climb(e, &b, x, y, dx, dy, 0, dy > 0, out);
            }

            // Around the corner of the ceiling over it going up, or of the
            // floor under it going down, onto its side
            else if (is_solid(x, ty) && !is_solid(tx, y)) {
                if (!walls || !fits_at(&b, tx, y) || (dy < 0 && !ceilings))
                    continue;
                push_climb(e, &b, x, y, dx, dy, -dx, false, out);
            }
        }
    }
}

typedef void (*move_func)(const entity*,
                          const trajectory*,
                          body,
                          int,
                          int,
                          const move_start*,
                          dynarr*);

// Puts on `out` the moves of `e` letting go of the wall or ceiling it clings
// to on node `(x,y)`, with its body shaped like `b`, getting to where it
// clings first, with `move`.
static void get_let_go_moves(
    const entity* e, body b, int x, int y, move_func move, dynarr* out) {
    if (!(e->moves & move_drop)) return;
    entity_surface surface = get_cling_surface(e, x, y);
    if (surface == surface_none) return;
    move_start start = (move_start){.mode = start_aligned};
    cling_spot(&b, x, y, surface, &start.x, &start.y);
    start.medium = get_medium(e, &b, start.x, start.y);
    dynarr* set = get_let_goes(e, surface, set_medium(&start));
    for (size_t k = 0; k < set->len; k++)
        move(e, dynarr_at(k, set), b, x, y, &start, out);
}

// Puts on `out` every move `e` can do from node `(x,y)`, where it flies, with
// its body shaped like `b`: flying to the nodes around it where it flies, the
// same in every direction, to their center, diagonally only past corners its
// body fits by.
static void get_fly_moves(const entity* e, body b, int x, int y, dynarr* out) {
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            int tx = x + dx;
            int ty = y + dy;
            if (get_node_kind(e, &b, tx, ty) != node_fly) continue;
            if (!clears_corner(&b, x, y, dx, dy)) continue;
            path_move m = (path_move){
                .move = motion_fly,
                .from_x = x,
                .from_y = y,
                .to_x = tx,
                .to_y = ty,
                .cost = step_distance(dx, dy) / flies[e->fly].max_speed,
                .target_x = node_pos_x(&b, tx),
                .target_y = node_mid_y(&b, ty),
            };
            push_move(m, out);
        }
    }
}

// Puts on `out` every move the entity can do from node `(x,y)`, with its body
// shaped like `b` getting there the way `s` is. Where it climbs, it can only
// climb or let go. With `whole`, moves are simulated whole instead of
// following their trajectories, which is only for tests.
static void get_moves(const entity* e,
                      body b,
                      int x,
                      int y,
                      node_state s,
                      bool whole,
                      dynarr* out) {
    clear_dynarr(out);
    node_kind kind = get_node_kind(e, &b, x, y);
    if (kind == node_swim) {
        get_swim_moves(e, b, x, y, out);
        return;
    }
    move_func move = whole ? simulate_whole : follow_trajectory;
    if (kind == node_climb) {
        get_climb_moves(e, b, x, y, out);
        get_let_go_moves(e, b, x, y, move, out);
        return;
    }
    if (kind == node_fly) {
        get_fly_moves(e, b, x, y, out);
        return;
    }
    get_climb_moves(e, b, x, y, out);

    // Falls off the ledge on each side, `can_fall[0]` being to the left
    bool can_fall[2] = {false, false};
    for (Sint8 dir = -1; dir <= 1; dir += 2) {
        int to = x + dir;
        node_kind kind = get_node_kind(e, &b, to, y);
        if (kind == node_swim)
            push_swim(
                &b, x, y, dir, 0, get_movement(e, true).max_speed, false, out);
        else if (kind == node_ground && e->moves & move_run)
            push_run(e, &b, x, y, dir, s, out);
        else if (kind == node_none)
            can_fall[dir > 0] = e->moves & move_fall && fits_at(&b, to, y);
    }

    move_start starts[2];
    size_t n = get_starts(e, &b, x, y, s, starts);
    bool on_platform = e->moves & move_drop && is_on_platform(&b, x, y);
    for (size_t i = 0; i < n; i++) {
        const move_start* start = &starts[i];
        if (can_fall[0] || can_fall[1] || on_platform) {
            dynarr* set = get_ground_moves(
                e, start->offset, start->speed, set_medium(start));
            for (size_t k = 0; k < set->len; k++) {
                trajectory* t = dynarr_at(k, set);
                bool falls = t->inputs.move == motion_fall &&
                             can_fall[t->inputs.dir > 0];
                bool drops = t->inputs.move == motion_drop && on_platform;
                if (falls || drops) move(e, t, b, x, y, start, out);
            }
        }
        if (e->moves & move_jump) {
            for (jump_class j = 0; j <= e->jump; j++) {
                dynarr* set = get_jumps(
                    j, e, start->offset, start->speed, set_medium(start));
                for (size_t k = 0; k < set->len; k++)
                    move(e, dynarr_at(k, set), b, x, y, start, out);
            }
        }
    }
}



static void heap_push(double f, Uint32 node) {
    push_to_dynarr(&(heap_item){.f = f, .node = node}, &heap);
    heap_item* h = heap.data;
    size_t i = heap.len - 1;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (h[p].f <= h[i].f) break;
        heap_item tmp = h[p];
        h[p] = h[i];
        h[i] = tmp;
        i = p;
    }
}

static heap_item heap_pop(void) {
    heap_item* h = heap.data;
    heap_item top = h[0];
    h[0] = h[heap.len - 1];
    remove_at_from_dynarr(heap.len - 1, &heap);
    size_t i = 0;
    while (true) {
        size_t l = 2 * i + 1;
        size_t r = l + 1;
        size_t min = i;
        if (l < heap.len && h[l].f < h[min].f) min = l;
        if (r < heap.len && h[r].f < h[min].f) min = r;
        if (min == i) break;
        heap_item tmp = h[min];
        h[min] = h[i];
        h[i] = tmp;
        i = min;
    }
    return top;
}

// The fastest `e` can go, for `heuristic()`.
static double get_top_speed(const entity* e) {
    double speed = SDL_max(
        runs[e->run].max_speed,
        SDL_max(falls[e->fall].max_fall_speed, jumps[e->jump].jump_speed));
    if (e->moves & move_swim)
        speed = SDL_max(
            speed,
            SDL_max(swims[e->swim].max_speed, swims[e->swim].jump_speed));
    if (e->moves & move_fly) speed = SDL_max(speed, flies[e->fly].max_speed);
    return speed;
}

// Seconds it would take to go from node `(x,y)` to `(gx,gy)` in a straight
// line going `speed`, the fastest the entity can go.
static double heuristic(double speed, int x, int y, int gx, int gy) {
    double dx = (double)(x - gx) * SPRITESHEET_CELL_X;
    double dy = (double)(y - gy) * SPRITESHEET_CELL_Y;
    return SDL_sqrt(dx * dx + dy * dy) / speed;
}

// Adds a node at `(x,y)` reached from `parent` with `via`, `g` seconds from the
// start, getting there the way `state` is, returning its index.
static Uint32 add_node(
    int x, int y, double g, Sint32 parent, path_move via, node_state state) {
    Uint32 idx = nodes.len;
    push_to_dynarr(&(search_node){.x = x,
                                  .y = y,
                                  .g = g,
                                  .parent = parent,
                                  .via = via,
                                  .state = state},
                   &nodes);
    stamps[x - sim.x][y - sim.y] = search_id;
    indexes[x - sim.x][y - sim.y] = idx;
    return idx;
}

// Whether the body `b` of `e` at node `(x,y)` has no fluids it avoids.
static bool is_node_safe(const entity* e, const body* b, int x, int y) {
    return !get_body_fluids(
                e->safe_fluids, b, node_pos_x(b, x), node_pos_y(b, y))
                .unsafe;
}

// Finds the path of `d`, moving like `e`, from the node `(sx,sy)` its body `b`
// is on, getting
// there the way `s` is, to the node `(gx,gy)`, or to the closest node to it
// found, if it doesn't reach it within `ENTITY_PATH_MAX_EXPANSIONS`, or
// `ENTITY_PATH_MAX_FLY_EXPANSIONS` of the nodes where it flies. With
// `to_safe`, it finds the path to the nearest node without fluids it avoids
// instead, which becomes the goal, or none when it doesn't find one. Each node
// keeps how the search first got to it, from the body as it is now, so moves
// from it start from there. The node it's on can have fluids it avoids, but no
// other can unless `ignoring_unsafe`, see `is_unsafe_ok()`. The path is empty
// when it can't get any closer.
static void find_path(entity_data* d,
                      const entity* e,
                      body b,
                      node_state s,
                      Uint16 sx,
                      Uint16 sy,
                      Uint16 gx,
                      Uint16 gy,
                      bool to_safe) {
    clear_dynarr(&d->path);
    d->path_at = 0;
    d->goal_x = gx;
    d->goal_y = gy;
    d->ignores_unsafe = ignoring_unsafe;
    // Without a goal to go towards, it's the nearest node it finds first
    double weight = to_safe ? 0.0 : ENTITY_PATH_HEURISTIC_WEIGHT;

    clear_dynarr(&nodes);
    clear_dynarr(&heap);
    search_id++;
    unsafe_start_x = sx;
    unsafe_start_y = sy;
    kinds_id++;
    caching_kinds = true;
    add_node(sx, sy, 0.0, -1, (path_move){0}, s);
    heap_push(0.0, 0);
    Uint32 best = 0;
    double speed = get_top_speed(e);
    double best_h = heuristic(speed, sx, sy, gx, gy);

    int expansions = 0;
    int fly_expansions = 0;
    while (heap.len > 0 && expansions < ENTITY_PATH_MAX_EXPANSIONS &&
           fly_expansions < ENTITY_PATH_MAX_FLY_EXPANSIONS) {
        Uint32 idx = heap_pop().node;
        const search_node n = *node_at(idx);
        if (n.closed) continue;
        node_at(idx)->closed = true;
        if (get_node_kind(e, &b, n.x, n.y) == node_fly)
            fly_expansions++;
        else
            expansions++;

        if (to_safe) {
            if (is_node_safe(e, &b, n.x, n.y)) {
                best = idx;
                d->goal_x = n.x;
                d->goal_y = n.y;
                break;
            }
        } else {
            double h = heuristic(speed, n.x, n.y, gx, gy);
            if (h < best_h || (h == best_h && n.g < node_at(best)->g)) {
                best = idx;
                best_h = h;
            }
            if (n.x == gx && n.y == gy) break;
        }

        get_moves(e, b, n.x, n.y, n.state, false, &moves);
        for (size_t i = 0; i < moves.len; i++) {
            const path_move* m = dynarr_at(i, &moves);
            if (!is_within_sim(m->to_x, m->to_y)) continue;
            double g = n.g + m->cost;
            Uint32 to = 0;
            if (stamps[m->to_x - sim.x][m->to_y - sim.y] == search_id) {
                to = indexes[m->to_x - sim.x][m->to_y - sim.y];
                search_node* t = node_at(to);
                if (t->closed || g >= t->g) continue;
                t->g = g;
                t->parent = idx;
                t->via = *m;
                t->state = m->to_state;
            } else
                to = add_node(m->to_x, m->to_y, g, idx, *m, m->to_state);
            heap_push(g + weight * heuristic(speed, m->to_x, m->to_y, gx, gy),
                      to);
        }
    }
    unsafe_start_x = -1;
    unsafe_start_y = -1;
    caching_kinds = false;
    d->reaches_goal =
        node_at(best)->x == d->goal_x && node_at(best)->y == d->goal_y;

    // Moves are pushed from the best node back to the start, then reversed
    for (Sint32 i = best; node_at(i)->parent >= 0; i = node_at(i)->parent)
        push_to_dynarr(&node_at(i)->via, &d->path);
    for (size_t i = 0; i < d->path.len / 2; i++) {
        path_move* a = dynarr_at(i, &d->path);
        path_move* z = dynarr_at(d->path.len - 1 - i, &d->path);
        path_move tmp = *a;
        *a = *z;
        *z = tmp;
    }
}

// Whether the next move on the path `d` already has, moving like `e`, goes from
// node `(x,y)`,
// where its body `b` is getting there the way `s` is, towards `(gx,gy)`,
// ignoring fluids it avoids the same way, and it still gets to the same node
// from there, in which case it's replaced with how it does it now, without
// looking for a new path.
static bool reuse_path(entity_data* d,
                       const entity* e,
                       body b,
                       node_state s,
                       int x,
                       int y,
                       int gx,
                       int gy) {
    path_move* next = move_at(d, d->path_at);
    if (!next || d->goal_x != gx || d->goal_y != gy || next->from_x != x ||
        next->from_y != y || d->ignores_unsafe != ignoring_unsafe)
        return false;
    unsafe_start_x = x;
    unsafe_start_y = y;
    get_moves(e, b, x, y, s, false, &moves);
    unsafe_start_x = -1;
    unsafe_start_y = -1;
    for (size_t i = 0; i < moves.len; i++) {
        path_move* m = dynarr_at(i, &moves);
        if (m->to_x != next->to_x || m->to_y != next->to_y) continue;
        *next = *m;
        return true;
    }
    return false;
}

// Drops the path of `d` but the move it's in the middle of, if any, so the
// next one is found again.
static void drop_path(entity_data* d) {
    path_move* m = move_at(d, d->path_at);
    if (d->moving && m) {
        path_move keep = *m;
        clear_dynarr(&d->path);
        push_to_dynarr(&keep, &d->path);
    } else
        clear_dynarr(&d->path);
    d->path_at = 0;
}

static bool is_climb(const path_move* m) {
    return m->move == motion_climb_wall || m->move == motion_climb_ceiling;
}

// Moves steered to `target_x,target_y` on both axes: swimming, climbing,
// flying and landing. Climbing, flying and landing don't fall on the way.
static bool is_targeted(const path_move* m) {
    return m->move == motion_swim || is_climb(m) || m->move == motion_fly ||
           m->move == motion_land;
}

// Moves are steered when they're running or targeted, instead of being
// replayed from where they start.
static bool is_steered(const path_move* m) {
    return m->move == motion_run || is_targeted(m);
}

// Whether `p`, going at `v`, got to `to`, within half of what it moves in a
// tick, for climbing and flying, which go in any direction.
static bool is_near(double p, double v, double to) {
    return SDL_fabs(to - p) <= SDL_max(SDL_fabs(v) / (2.0 * FIXED_T_TARGET_FPS),
                                       ENTITY_ALIGN_TOLERANCE);
}

// Ends the move the entity at `id` is on, interrupted if `interrupted`,
// otherwise completed if its body is on the node it was going to, or missed if
// it isn't. A completed move goes on to the next one on its path, for when it
// moves towards the same place again, and the path is dropped otherwise. Then,
// it calls its `on_move_finished_func`, returning whether it still exists after
// it.
static bool end_move(size_t id, bool interrupted) {
    entity_data* d = get_entity(id);
    const path_move* m = move_at(d, d->path_at);
    body b = (body){0};
    PHYSICS_get_body(d->entity.body_id, &b);
    bool on_floor = false;
    PHYSICS_is_body_on_floor(d->entity.body_id, &on_floor);
    entity_move_result r = d->result;
    entity e = moving_as(d);
    Uint16 x = 0;
    Uint16 y = 0;
    r.on_node = find_body_node(&e, &b, on_floor, &x, &y) != node_none;
    r.end_x = r.on_node ? x : 0;
    r.end_y = r.on_node ? y : 0;
    if (interrupted)
        r.end = move_interrupted;
    else if (r.on_node && x == m->to_x && y == m->to_y)
        r.end = move_completed;
    else
        r.end = move_missed;

    d->moving = false;
    d->executing = false;
    d->move_time = 0.0;
    if (r.end == move_completed)
        d->path_at++;
    else {
        clear_dynarr(&d->path);
        d->path_at = 0;
    }
    if (d->entity.on_move_finished_func) d->entity.on_move_finished_func(id, r);
    return get_entity(id) != NULL;
}

// Whether a steered move slows down to stop where it's going, when the move
// after it on its path, `next`, starts from its node's center, or there's
// none. It
// doesn't know if it'll be told to do that one, but when it is, the search
// expected it to stop there.
static bool stops_before(const path_move* next) {
    return !next || (!is_steered(next) && next->start == start_aligned);
}

// Starts the move `m` of `d` once its body `b`, on the floor if `on_floor`,
// gets to where the move starts, returning whether it did. Until then, it sets
// the direction to hold on `dir`, and the vertical one on `vdir`, which is only
// held swimming.
// Moves that start right away going somewhere start once the body gets to
// where their trajectory starts, within half of what it moves in a tick. Moves
// that don't start swimming or clinging wait to be on the floor. Ones that
// start from where they stop get there first, `target_x,target_y`, at rest,
// without falling when they start clinging.
static bool start_move(entity_data* d,
                       const path_move* m,
                       const body* b,
                       bool on_floor,
                       const movement* mv,
                       double dt,
                       Sint8* dir,
                       Sint8* vdir) {
    double vx = b->vel_x;
    if (!m->from_swim && m->move != motion_let_go && !on_floor) return false;

    bool ready = false;
    switch (m->start) {
    case start_now:
        if (m->start_vx == 0.0) {
            ready = true;
            break;
        }
        Sint8 going = m->start_vx > 0.0 ? 1 : -1;
        ready = going * (m->start_x - b->x) <= SDL_fabs(vx) * dt / 2.0;
        if (!ready) *dir = going;
        break;
    case start_at_rest:
        ready = SDL_fabs(vx) <= EPSILON;
        break;
    case start_aligned:
        // Standing, gravity keeps it on the floor, otherwise it gets to the
        // height it starts from too
        bool standing = !m->from_swim && m->move != motion_let_go;
        ready = is_near(b->x, vx, m->target_x) && SDL_fabs(vx) <= EPSILON &&
                (standing || (is_near(b->y, b->vel_y, m->target_y) &&
                              SDL_fabs(b->vel_y) <= EPSILON));
        if (!ready) {
            *dir = steer(mv, b->x, vx, m->target_x, true);
            if (!standing) *vdir = steer(mv, b->y, b->vel_y, m->target_y, true);
        }
        break;
    }
    if (!ready) return false;

    d->executing = true;
    d->climbing = false;
    d->move_time = 0.0;
    d->progress = (move_progress){.wet = m->from_swim};
    if (m->move == motion_drop) PHYSICS_drop_down_body(d->entity.body_id);
    return true;
}

// Node a target at `(x,y)`, the bottom center of where the body should be, is
// on.
static void get_goal(double x, double y, Uint16* gx, Uint16* gy) {
    *gx = SDL_clamp(SDL_floor(x / SPRITESHEET_CELL_X), 0, TILEMAP_SIZE_X - 1);
    *gy = SDL_clamp(
        SDL_floor((y - 1.0) / SPRITESHEET_CELL_Y), 0, TILEMAP_SIZE_Y - 1);
}

// Does the move the entity at `id` is on, setting its body's velocity for this
// tick. When a move ends, its `on_move_finished_func` can start the next one,
// which starts on the same tick, so it can jump again the tick it lands, like
// its trajectory was simulated. Without a move, it lets go of every direction,
// so only the physics move it, but it keeps clinging to whatever it climbs as
// long as it touches it. Clinging or flying, it moves vertically like it does
// horizontally, without falling, and entities that fly never fall out of
// fluids.
static void update_movement(size_t id) {
    entity_data* d = get_entity(id);
    entity mover = moving_as(d);
    const entity* e = &mover;
    body b = (body){0};
    PHYSICS_get_body(e->body_id, &b);
    bool on_floor = false;
    PHYSICS_is_body_on_floor(e->body_id, &on_floor);
    body_fluids f = get_body_fluids(e->safe_fluids, &b, b.x, b.y);
    bool swimming = swims_in(e, f.swimming);
    movement mv = get_movement(e, f.swimming);
    double dt = GAME_MANAGER_get_current_dt();

    double vx = b.vel_x;
    double vy = b.vel_y;
    double x = b.x + b.w / 2.0;
    Sint8 dir = 0;
    Sint8 vdir = 0;
    // Whether it moves without falling, clinging or flying
    bool weightless = e->moves & move_fly && !f.swimming;

    // Each move either ends and lets the next one start, or sets what to do
    while (d->moving) {
        path_move* m = move_at(d, d->path_at);
        bool ended = false;
        bool interrupted = d->move_time > ENTITY_MOVE_MAX_TIME;

        // Swimming or flying to where it stands, swims or flies on the next
        // node, climbing to where it clings on it, or getting to where it
        // lands, without stopping if the next move on its path goes on from
        // there
        if (!interrupted && is_targeted(m)) {
            if (is_climb(m)) d->climbing = true;
            if (m->move != motion_swim) weightless = true;
            ended =
                is_near(b.x, vx, m->target_x) && is_near(b.y, vy, m->target_y);
            // Landing, it only lands once it came to rest there
            if (m->move == motion_land)
                ended =
                    ended && SDL_fabs(vx) <= EPSILON && SDL_fabs(vy) <= EPSILON;
            if (ended && m->move == motion_land) {
                d->flying = false;
                d->climbing = get_touching(&d->entity, &b) != surface_none;
            }
            if (!ended) {
                bool stops = stops_before(move_at(d, d->path_at + 1));
                dir = steer(&mv, b.x, vx, m->target_x, stops);
                vdir = steer(&mv, b.y, vy, m->target_y, stops);
                d->move_time += dt;
                break;
            }
        }

        // Running to the next node's center, the same way
        else if (!interrupted && m->move == motion_run) {
            d->climbing = false;
            ended = has_reached(m->dir, x, vx, node_center_x(m->to_x));
            if (!ended) {
                bool stops = stops_before(move_at(d, d->path_at + 1));
                dir = steer(&mv, x, vx, node_center_x(m->to_x), stops);
                d->move_time += dt;
                break;
            }
        }

        // Getting to where it starts, then replaying it as it was simulated,
        // until it lands
        else if (!interrupted) {
            if (!d->executing &&
                !start_move(d, m, &b, on_floor, &mv, dt, &dir, &vdir)) {
                weightless = weightless || m->move == motion_let_go;
                d->move_time += dt;
                break;
            }
            if (d->move_time > 0.0)
                ended = advance(&d->progress,
                                m,
                                d->move_time,
                                on_floor,
                                swimming,
                                vx) != progress_going;
            if (!ended) {
                control_move(&mv, m, d->move_time, dt, &vx, &vy);
                d->move_time += dt;
                PHYSICS_set_body_velocity(e->body_id, vx, vy);
                return;
            }
        }

        // It might have been deleted or moved by its `on_move_finished_func`,
        // which might have also made it fly or land
        if (!end_move(id, interrupted)) return;
        d = get_entity(id);
        mover = moving_as(d);
        swimming = swims_in(e, f.swimming);
        mv = get_movement(e, f.swimming);
        weightless = e->moves & move_fly && !f.swimming;
    }

    if (!d->moving && d->climbing) {
        d->climbing = get_touching(e, &b) != surface_none;
        weightless = weightless || d->climbing;
    }
    PHYSICS_set_body_velocity(e->body_id,
                              run_speed(&mv, vx, dir, dt),
                              weightless ? run_speed(&mv, vy, vdir, dt)
                                         : vertical_speed(&mv, vy, vdir, dt));
}



// Checks there's an entity at `entity_id`, logging what it tried to `do` with
// it otherwise. Returns `3` when it's outside the array's range, `1` when
// there's none at it, `0` when there is.
static int check_id(size_t entity_id, const char* to_do) {
    if (entity_id >= entities.cap) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to %s entity_id=%lu but this is outside the buffer "
                    "range",
                    to_do,
                    entity_id);
        return 3;
    }
    if (!get_entity(entity_id)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to %s inexistent entity at entity_id=%lu",
                    to_do,
                    entity_id);
        return 1;
    }
    return 0;
}

// Outside of `ENTITY_update()`, the simulation space and fluids might have
// changed since it last read them, so they're read again.
static void refresh(void) {
    if (updating) return;
    sim = CAMERA_get_simulation_rect();
    fluids_id++;
}

// Gets the body of the entity `d` on `b` and whether it's on the floor on
// `on_floor`.
static void get_body(const entity_data* d, body* b, bool* on_floor) {
    *b = (body){0};
    PHYSICS_get_body(d->entity.body_id, b);
    *on_floor = false;
    PHYSICS_is_body_on_floor(d->entity.body_id, on_floor);
}

// Gets the entity at `entity_id` on `d` for a call that tries `to_do`
// something with it, with its body on `b` and whether it's on the floor on
// `on_floor`, reading the simulation space and fluids again outside updates.
// Returns what `check_id()` does.
static int begin_call(size_t entity_id,
                      const char* to_do,
                      entity_data** d,
                      body* b,
                      bool* on_floor) {
    int err = check_id(entity_id, to_do);
    if (err) return err;
    *d = get_entity(entity_id);
    refresh();
    get_body(*d, b, on_floor);
    return 0;
}

// Gets on `(x,y)` the node the body `b` of `e`, on the floor if `on_floor`, is
// on, even one with fluids it avoids. Returns whether it's on one it can find
// a path from, which it isn't in the air or outside the simulation space.
static bool get_start_node(
    const entity* e, const body* b, bool on_floor, Uint16* x, Uint16* y) {
    return find_body_node(e, b, on_floor, x, y) != node_none &&
           is_within_sim(*x, *y);
}

static bool is_valid(entity data) {
    if (data.jump < 0 || data.jump >= ENTITY_JUMP_CLASSES_TOTAL ||
        data.fall < 0 || data.fall >= ENTITY_FALL_CLASSES_TOTAL ||
        data.run < 0 || data.run >= ENTITY_RUN_CLASSES_TOTAL || data.swim < 0 ||
        data.swim >= ENTITY_SWIM_CLASSES_TOTAL || data.fly < 0 ||
        data.fly >= ENTITY_FLY_CLASSES_TOTAL || data.size < 0 ||
        data.size >= ENTITY_SIZE_CLASSES_TOTAL)
        return false;
    body b = (body){0};
    if (PHYSICS_get_body(data.body_id, &b)) return false;
    return b.w == sizes[data.size].w && b.h == sizes[data.size].h;
}

static void warn_invalid(const char* to_do, entity data) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Tried to %s with invalid entity data: { body_id = %lu, size = "
                "%d }",
                to_do,
                data.body_id,
                data.size);
}

int ENTITY_new_entity(entity data, size_t* entity_id) {
    if (!is_valid(data)) {
        warn_invalid("create entity", data);
        return 2;
    }
    entity_data d = (entity_data){
        .entity = data,
        .climbing = climbs(&data),
        .flying = only_flies(&data),
    };
    init_dynarr(&d.path, sizeof(path_move));
    *entity_id = add_to_pool(&d, &entities);
    if (data.on_create_func) data.on_create_func(*entity_id);
    return 0;
}

int ENTITY_del_entity(size_t entity_id) {
    int err = check_id(entity_id, "delete");
    if (err) return err;
    void (*on_destroy_func)(size_t) =
        get_entity(entity_id)->entity.on_destroy_func;
    if (on_destroy_func) on_destroy_func(entity_id);

    // Fetched again, since `on_destroy_func` might have moved the pool
    entity_data* d = get_entity(entity_id);
    PHYSICS_del_body(d->entity.body_id);
    deinit_dynarr(&d->path);
    remove_from_pool(entity_id, &entities);
    return 0;
}

int ENTITY_get_entity(size_t entity_id, entity* data) {
    int err = check_id(entity_id, "get");
    if (err) return err;
    *data = get_entity(entity_id)->entity;
    return 0;
}

// It only keeps flying or climbing if it still can, and flies if it can't
// move any other way anymore.
int ENTITY_edit_entity(size_t entity_id, entity data) {
    int err = check_id(entity_id, "edit");
    if (err) return err;
    if (!is_valid(data)) {
        warn_invalid("edit", data);
        return 2;
    }
    entity_data* d = get_entity(entity_id);
    d->entity = data;
    d->flying = (d->flying && data.moves & move_fly) || only_flies(&data);
    d->climbing = d->climbing && climbs(&data);
    drop_path(d);
    return 0;
}

// Starts the move the path of `d` is at with its body `b`, keeping where the
// body was for its result.
static void start_path_move(entity_data* d, const body* b) {
    const path_move* m = move_at(d, d->path_at);
    d->moving = true;
    d->executing = false;
    d->move_time = 0.0;
    d->result = (entity_move_result){
        .move = m->move,
        .start_x = b->x,
        .start_y = b->y,
        .start_vx = b->vel_x,
        .start_vy = b->vel_y,
        .from_x = m->from_x,
        .from_y = m->from_y,
        .to_x = m->to_x,
        .to_y = m->to_y,
        .reaches_goal = d->reaches_goal,
    };
}

// Starts `m` with the body `b` as the only move of the path of `d`, to its
// node, found ignoring fluids it avoids if `ignores_unsafe`.
static void start_single_move(entity_data* d,
                              path_move m,
                              const body* b,
                              bool ignores_unsafe) {
    clear_dynarr(&d->path);
    push_to_dynarr(&m, &d->path);
    d->path_at = 0;
    d->goal_x = m.to_x;
    d->goal_y = m.to_y;
    d->reaches_goal = true;
    d->ignores_unsafe = ignores_unsafe;
    start_path_move(d, b);
}

// Starts the next move of `d` towards `(x,y)`, moving as it does now, with its
// body `b`, on the floor if `on_floor`, going through fluids it avoids if
// `ignore_unsafe`. Returns what `ENTITY_move_towards()` does.
static int start_towards(entity_data* d,
                         body b,
                         bool on_floor,
                         double x,
                         double y,
                         bool ignore_unsafe) {
    entity e = moving_as(d);
    Uint16 sx, sy;
    if (!get_start_node(&e, &b, on_floor, &sx, &sy)) return 5;
    Uint16 gx, gy;
    get_goal(x, y, &gx, &gy);
    if (sx == gx && sy == gy) {
        clear_dynarr(&d->path);
        d->path_at = 0;
        return 6;
    }

    ignoring_unsafe = ignore_unsafe;
    node_state s = (node_state){.known = on_floor, .x = b.x, .vx = b.vel_x};
    if (!reuse_path(d, &e, b, s, sx, sy, gx, gy))
        find_path(d, &e, b, s, sx, sy, gx, gy, false);
    ignoring_unsafe = false;
    if (!move_at(d, d->path_at)) return 7;
    start_path_move(d, &b);
    return 0;
}

// `ENTITY_move_towards()`, going through fluids the entity avoids if
// `ignore_unsafe`. Entities that can only fly always fly, and others never do,
// so they have to land when they're flying.
static int
move_towards(size_t entity_id, double x, double y, bool ignore_unsafe) {
    entity_data* d;
    body b;
    bool on_floor;
    int err = begin_call(entity_id, "move", &d, &b, &on_floor);
    if (err) return err;
    if (d->moving) return 4;
    if (only_flies(&d->entity))
        d->flying = true;
    else if (d->flying)
        return 5;
    return start_towards(d, b, on_floor, x, y, ignore_unsafe);
}

int ENTITY_move_towards(size_t entity_id, double x, double y) {
    return move_towards(entity_id, x, y, false);
}

int ENTITY_move_towards_ignoring_unsafe_fluids(size_t entity_id,
                                               double x,
                                               double y) {
    return move_towards(entity_id, x, y, true);
}

int ENTITY_fly_to(size_t entity_id, double x, double y) {
    entity_data* d;
    body b;
    bool on_floor;
    int err = begin_call(entity_id, "make fly", &d, &b, &on_floor);
    if (err) return err;
    if (!(d->entity.moves & move_fly)) return 2;
    if (d->moving) return 4;
    d->flying = true;
    return start_towards(d, b, on_floor, x, y, false);
}

// Where a body like `b` of `e` lands on node `(x,y)`, on `px,py`: on its
// floor, otherwise against a wall beside it, preferring its right, if it
// climbs walls, or its ceiling, if it climbs ceilings, without fluids it
// avoids. Returns whether it can land there.
static bool get_landing(
    const entity* e, const body* b, int x, int y, double* px, double* py) {
    if (!fits_at(b, x, y)) return false;
    int side = get_wall_side(e, x, y);
    bool ceiling = e->moves & move_climb_ceiling && is_solid(x, y - 1);
    bool floor = has_floor(b, x, y);
    if (!floor && !side && !ceiling) return false;
    climb_pos(b, x, y, floor ? 0 : side, !side, px, py);
    return !get_body_fluids(e->safe_fluids, b, *px, *py).unsafe;
}

int ENTITY_land_on(size_t entity_id, double x, double y) {
    entity_data* d;
    body b;
    bool on_floor;
    int err = begin_call(entity_id, "land", &d, &b, &on_floor);
    if (err) return err;
    if (d->moving) return 4;
    Uint16 gx, gy;
    get_goal(x, y, &gx, &gy);
    double px, py;
    if (!get_landing(&d->entity, &b, gx, gy, &px, &py)) return 2;
    entity e = moving_as(d);
    Uint16 sx, sy;
    if (!get_start_node(&e, &b, on_floor, &sx, &sy)) return 5;
    if (sx != gx || sy != gy) {
        if (only_flies(&d->entity)) d->flying = true;
        return start_towards(d, b, on_floor, x, y, false);
    }
    if (!d->flying && is_near(b.x, 0.0, px) && is_near(b.y, 0.0, py) &&
        SDL_fabs(b.vel_x) <= EPSILON)
        return 6;
    start_single_move(d,
                      (path_move){
                          .move = motion_land,
                          .from_x = sx,
                          .from_y = sy,
                          .to_x = gx,
                          .to_y = gy,
                          .target_x = px,
                          .target_y = py,
                      },
                      &b,
                      false);
    return 0;
}

int ENTITY_interrupt_movement(size_t entity_id) {
    int err = check_id(entity_id, "interrupt the movement of");
    if (err) return err;
    if (!get_entity(entity_id)->moving) return 4;
    end_move(entity_id, true);
    return 0;
}

int ENTITY_is_moving(size_t entity_id, bool* moving) {
    int err = check_id(entity_id, "get if moving");
    if (err) return err;
    *moving = get_entity(entity_id)->moving;
    return 0;
}

int ENTITY_is_flying(size_t entity_id, bool* flying) {
    int err = check_id(entity_id, "get if flying");
    if (err) return err;
    *flying = get_entity(entity_id)->flying;
    return 0;
}

int ENTITY_get_node(size_t entity_id, Uint16* x, Uint16* y) {
    entity_data* d;
    body b;
    bool on_floor;
    int err = begin_call(entity_id, "get the node of", &d, &b, &on_floor);
    if (err) return err;
    entity e = moving_as(d);
    return find_body_node(&e, &b, on_floor, x, y) == node_none ? 4 : 0;
}

int ENTITY_get_clinging(size_t entity_id, entity_surface* surface) {
    entity_data* d;
    body b;
    bool on_floor;
    int err =
        begin_call(entity_id, "get what's clinging to", &d, &b, &on_floor);
    if (err) return err;
    *surface = d->climbing ? get_touching(&d->entity, &b) : surface_none;
    return 0;
}

int ENTITY_get_fluids(size_t entity_id, bool* swimming, bool* unsafe) {
    entity_data* d;
    body b;
    bool on_floor;
    int err = begin_call(entity_id, "get the fluids of", &d, &b, &on_floor);
    if (err) return err;
    body_fluids f = get_body_fluids(d->entity.safe_fluids, &b, b.x, b.y);
    *swimming = f.swimming;
    *unsafe = f.unsafe;
    return 0;
}

int ENTITY_find_nearest_safe_tile(size_t entity_id, Uint16* x, Uint16* y) {
    entity_data* d;
    body b;
    bool on_floor;
    int err = begin_call(entity_id, "find a safe tile for", &d, &b, &on_floor);
    if (err) return err;
    entity e = moving_as(d);
    Uint16 sx, sy;
    if (!get_start_node(&e, &b, on_floor, &sx, &sy)) return 5;
    if (is_node_safe(&e, &b, sx, sy)) {
        *x = sx;
        *y = sy;
        return 0;
    }

    // The path is kept, so moving there next only checks its first move
    if (d->moving) return 4;
    ignoring_unsafe = true;
    node_state s = (node_state){.known = on_floor, .x = b.x, .vx = b.vel_x};
    find_path(d, &e, b, s, sx, sy, sx, sy, true);
    ignoring_unsafe = false;
    if (d->path.len > 0) {
        *x = d->goal_x;
        *y = d->goal_y;
        return 0;
    }

    // Up until it's out of any fluid, unless something's in the way first
    for (int ty = sy - 1; ty >= sim.y && fits_at(&b, sx, ty); ty--) {
        if (get_medium(&e, &b, node_pos_x(&b, sx), node_pos_y(&b, ty)) !=
            medium_dry)
            continue;
        *x = sx;
        *y = ty;
        return 6;
    }
    return 7;
}

// Picks how long `m`, a jump of `e` holding `m->dir`, holds it to land on node
// `(gx,gy)` from the body `b`, as close as it gets to `x`, the horizontal
// position the bottom center of the body should end up at. It tries holding it
// for each tick, from none to the whole jump, simulating each, so it knows
// when to let go. Returns `false` when none land on it.
static bool aim_jump(const entity* e,
                     path_move* m,
                     body b,
                     bool swimming,
                     int gx,
                     int gy,
                     double x) {
    const int max_ticks = ENTITY_MOVE_MAX_TIME * FIXED_T_TARGET_FPS;
    bool found = false;
    double best_off = 0.0;
    double best_time = 0.0;
    for (int ticks = 0; ticks <= max_ticks; ticks++) {
        m->dir_time = (double)ticks / FIXED_T_TARGET_FPS;
        body landing = b;
        sim_state s = simulate_landing(
            e, m, &landing, 0.0, (move_progress){.wet = swimming}, 0.0);
        if (!s.landed) {
            if (!m->dir) break;
            continue;
        }
        Uint16 lx, ly;
        if (find_body_node(e, &landing, s.on_floor, &lx, &ly) != node_none &&
            lx == gx && ly == gy) {
            double off = SDL_fabs(landing.x + landing.w / 2.0 - x);
            if (!found || off < best_off) {
                best_off = off;
                best_time = m->dir_time;
            }
            found = true;
        }
        // Holding it past when it lands does the same as holding it until then
        if (!m->dir || ticks / (double)FIXED_T_TARGET_FPS >= s.time) break;
    }
    m->dir_time = best_time;
    return found;
}

int ENTITY_jump(size_t entity_id, jump_class jump, double x, double y) {
    entity_data* d;
    body b;
    bool on_floor;
    int err = begin_call(entity_id, "make jump", &d, &b, &on_floor);
    if (err) return err;
    if (jump < 0 || jump >= ENTITY_JUMP_CLASSES_TOTAL) return 2;
    if (d->moving) return 4;
    entity e = moving_as(d);
    bool swimming =
        swims_in(&e, get_body_fluids(e.safe_fluids, &b, b.x, b.y).swimming);
    if (!on_floor && !swimming) return 5;
    Uint16 sx = 0;
    Uint16 sy = 0;
    find_body_node(&e, &b, on_floor, &sx, &sy);
    Uint16 gx, gy;
    get_goal(x, y, &gx, &gy);

    double dx = x - (b.x + b.w / 2.0);
    path_move m = (path_move){
        .move = motion_jump,
        .dir = SDL_fabs(dx) <= ENTITY_ALIGN_TOLERANCE ? 0
               : dx > 0.0                             ? 1
                                                      : -1,
        .jump = jump,
        .start = start_now,
        .start_x = b.x,
        .start_vx = b.vel_x,
        .from_x = sx,
        .from_y = sy,
        .to_x = gx,
        .to_y = gy,
        .from_swim = swimming,
    };
    ignoring_unsafe = true;
    bool found = aim_jump(&e, &m, b, swimming, gx, gy, x);
    ignoring_unsafe = false;
    if (!found) return 7;
    start_single_move(d, m, &b, true);
    return 0;
}



void ENTITY_get_size(size_class size, Uint8* w, Uint8* h) {
    *w = sizes[size].w;
    *h = sizes[size].h;
}

void ENTITY_register_classes(entity classes) {
    // Only jumps, falls and drops follow trajectories
    if (!(classes.moves & (move_jump | move_fall | move_drop))) return;
    class_use* use = &used_classes[classes.fall][classes.run][classes.size];
    use->used = true;
    if (classes.safe_fluids && !(classes.moves & move_swim))
        use->submerged = true;
    if (classes.moves & move_drop && climbs(&classes)) use->lets_go = true;
    if (classes.moves & move_jump)
        use->jumps = SDL_max(use->jumps, (int)classes.jump + 1);
}

void ENTITY_get_tile_target(Uint16 x, Uint16 y, double* px, double* py) {
    *px = node_center_x(x);
    *py = (y + 1) * SPRITESHEET_CELL_Y;
}

void ENTITY_generate_trajectories(void) {
    for (fall_class f = 0; f < ENTITY_FALL_CLASSES_TOTAL; f++) {
        for (run_class r = 0; r < ENTITY_RUN_CLASSES_TOTAL; r++) {
            for (size_class s = 0; s < ENTITY_SIZE_CLASSES_TOTAL; s++) {
                class_use use = used_classes[f][r][s];
                if (!use.used) continue;
                for (medium md = 0; md < MEDIUMS; md++) {
                    if (md == medium_submerged && !use.submerged) continue;
                    for (int c = 0; use.lets_go && c < CLING_SURFACES; c++)
                        if (!let_go_sets[md][f][r][s][c].generated)
                            generate_let_go(f, r, s, c + 1, md);
                    for (int o = 0; o < START_OFFSETS; o++) {
                        for (int v = 0; v < START_SPEEDS; v++) {
                            if (!ground_sets[md][f][r][s][o][v].generated)
                                generate_ground(f, r, s, o, v, md);
                            for (jump_class j = 0; j < use.jumps; j++)
                                if (!jump_sets[md][j][f][r][s][o][v].generated)
                                    generate_jumps(j, f, r, s, o, v, md);
                        }
                    }
                }
            }
        }
    }
}

// Map of the `names` of the `total` classes of a kind to the class.
static hashmap get_names_map(const char** names, int total) {
    hashmap map;
    init_hashmap(&map, sizeof(int));
    for (int c = 0; c < total; c++)
        set_on_hashmap(&c, names[c], &map);
    return map;
}

hashmap ENTITY_get_string_jump_classes_map(void) {
    return get_names_map(jump_names, ENTITY_JUMP_CLASSES_TOTAL);
}

hashmap ENTITY_get_string_fall_classes_map(void) {
    return get_names_map(fall_names, ENTITY_FALL_CLASSES_TOTAL);
}

hashmap ENTITY_get_string_run_classes_map(void) {
    return get_names_map(run_names, ENTITY_RUN_CLASSES_TOTAL);
}

hashmap ENTITY_get_string_swim_classes_map(void) {
    return get_names_map(swim_names, ENTITY_SWIM_CLASSES_TOTAL);
}

hashmap ENTITY_get_string_fly_classes_map(void) {
    return get_names_map(fly_names, ENTITY_FLY_CLASSES_TOTAL);
}

hashmap ENTITY_get_string_size_classes_map(void) {
    return get_names_map(size_names, ENTITY_SIZE_CLASSES_TOTAL);
}



// Each class is read from its row, see `class_kind`, its values being laid out
// one after the other on its values struct.
static void load_classes(void) {
    dynarr parsed = CSV_parse(classes_csv);
    char key[KEY_MAX_SIZE];
    for (size_t k = 0; k < SDL_arraysize(class_kinds); k++) {
        const class_kind* kind = &class_kinds[k];
        size_t value_size = kind->type == parsing_uint8 ? 1 : sizeof(double);
        for (int c = 0; c < kind->total; c++) {
            SDL_snprintf(
                key, KEY_MAX_SIZE, "%s_%s", kind->prefix, kind->names[c]);
            Uint8* values = (Uint8*)kind->values + c * kind->stride;
            for (int col = 0; col < kind->columns; col++) {
                parsing_field field = {
                    key, kind->type, values + col * value_size, .at = col};
                read_csv_keys(&parsed, &field, 1);
            }
        }
    }
    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Entity classes loaded successfully.");
}

void ENTITY_start(void) {
    load_classes();
    init_pool_with(&entities, sizeof(entity_data), ENTITY_STARTING_POOL_CAP);
    init_dynarr(&nodes, sizeof(search_node));
    init_dynarr(&heap, sizeof(heap_item));
    init_dynarr(&moves, sizeof(path_move));
    init_dynarr(&generating, sizeof(generated_step));
}

void ENTITY_stop(void) {
    for (size_t idx = 0; idx < entities.cap; idx++)
        if (get_entity(idx)) ENTITY_del_entity(idx);
    deinit_pool(&entities);
    deinit_dynarr(&nodes);
    deinit_dynarr(&heap);
    deinit_dynarr(&moves);
    deinit_dynarr(&generating);
    for (fall_class f = 0; f < ENTITY_FALL_CLASSES_TOTAL; f++) {
        for (run_class r = 0; r < ENTITY_RUN_CLASSES_TOTAL; r++) {
            for (size_class s = 0; s < ENTITY_SIZE_CLASSES_TOTAL; s++) {
                used_classes[f][r][s] = (class_use){0};
                for (medium md = 0; md < MEDIUMS; md++) {
                    for (int c = 0; c < CLING_SURFACES; c++)
                        free_set(&let_go_sets[md][f][r][s][c]);
                    for (int o = 0; o < START_OFFSETS; o++) {
                        for (int v = 0; v < START_SPEEDS; v++) {
                            free_set(&ground_sets[md][f][r][s][o][v]);
                            for (jump_class j = 0;
                                 j < ENTITY_JUMP_CLASSES_TOTAL;
                                 j++)
                                free_set(&jump_sets[md][j][f][r][s][o][v]);
                        }
                    }
                }
            }
        }
    }
}

void ENTITY_update(void) {
    sim = CAMERA_get_simulation_rect();
    // Fluids are read again on each update
    fluids_id++;
    updating = true;
    for (size_t idx = 0; idx < entities.cap; idx++) {
        entity_data* d = get_entity(idx);
        if (!d) continue;
        bool simulated = false;
        PHYSICS_is_body_within_the_simulation(d->entity.body_id, &simulated);
        if (!simulated) continue;
        if (d->entity.update_func) d->entity.update_func(idx);

        // It might have been deleted by its own `update_func`
        if (!get_entity(idx)) continue;
        update_movement(idx);
    }
    updating = false;
}



#ifdef TEST
// Compares each step of the trajectories in `dense`, which kept all of them,
// with the ones `get_step()` gets from `sparse`, the same ones keeping only
// checkpoints, adding how many it compared to `checked` and returning how many
// weren't exactly the same.
static size_t compare_steps(const entity* e,
                            const trajectory_set* dense,
                            const trajectory_set* sparse,
                            size_t* checked) {
    body shape = (body){
        .type = physics_actor, .w = sizes[e->size].w, .h = sizes[e->size].h};
    size_t different = 0;
    for (size_t i = 0; i < dense->trajectories.len; i++) {
        const trajectory* d = dynarr_at(i, &dense->trajectories);
        const trajectory* s = dynarr_at(i, &sparse->trajectories);
        for (int k = 0; k <= d->last_step; k++) {
            const trajectory_step* a = dynarr_at(k, &d->checkpoints);
            trajectory_step b = get_step(e, s, shape, k);
            if (a->x != b.x || a->y != b.y || a->vx != b.vx || a->vy != b.vy)
                different++;
            (*checked)++;
        }
    }
    return different;
}

// Generates `set` with `generate_set()`, once keeping every step and once only
// checkpoints, comparing them, see `compare_steps()`.
static size_t check_replay(const entity* e,
                           trajectory_set* set,
                           void (*generate_set)(const entity*, int, int, int),
                           int a,
                           int b,
                           int c,
                           size_t* checked) {
    free_set(set);
    checkpoint_ticks = 1;
    generate_set(e, a, b, c);
    trajectory_set dense = *set;
    set->generated = false;
    checkpoint_ticks = ENTITY_TRAJECTORY_CHECKPOINT_TICKS;
    generate_set(e, a, b, c);
    size_t different = compare_steps(e, &dense, set, checked);
    free_set(&dense);
    return different;
}

static void gen_ground_set(const entity* e, int o, int v, int md) {
    generate_ground(e->fall, e->run, e->size, o, v, md);
}

static void gen_jump_set(const entity* e, int o, int v, int md) {
    generate_jumps(e->jump, e->fall, e->run, e->size, o, v, md);
}

static void
gen_let_go_set(const entity* e, int c, int _unused, int md) { // NOLINT
    generate_let_go(e->fall, e->run, e->size, c + 1, md);
}

size_t ENTITY_test_check_replays(entity e, size_t* checked) {
    size_t different = 0;
    *checked = 0;
    for (medium md = 0; md < MEDIUMS; md++) {
        for (int o = 0; o < START_OFFSETS; o++) {
            for (int v = 0; v < START_SPEEDS; v++) {
                different +=
                    check_replay(&e,
                                 &ground_sets[md][e.fall][e.run][e.size][o][v],
                                 gen_ground_set,
                                 o,
                                 v,
                                 md,
                                 checked);
                different += check_replay(
                    &e,
                    &jump_sets[md][e.jump][e.fall][e.run][e.size][o][v],
                    gen_jump_set,
                    o,
                    v,
                    md,
                    checked);
            }
        }
        for (int c = 0; c < CLING_SURFACES; c++)
            different +=
                check_replay(&e,
                             &let_go_sets[md][e.fall][e.run][e.size][c],
                             gen_let_go_set,
                             c,
                             0,
                             md,
                             checked);
    }
    return different;
}

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
                             size_t max) {
    refresh();
    entity_data* d = get_entity(entity_id);
    body b = (body){0};
    PHYSICS_get_body(d->entity.body_id, &b);
    dynarr out = (dynarr){0};
    init_dynarr(&out, sizeof(path_move));
    node_state s = (node_state){
        .known = known,
        .x = node_pos_x(&b, x) + offset_x,
        .vx = vx,
    };
    entity e = moving_as(d);
    get_moves(&e, b, x, y, s, whole, &out);
    size_t n = SDL_min(out.len, max);
    for (size_t i = 0; i < n; i++) {
        path_move* m = dynarr_at(i, &out);
        to_x[i] = m->to_x;
        to_y[i] = m->to_y;
        costs[i] = m->cost;
    }
    deinit_dynarr(&out);
    return n;
}
#endif
