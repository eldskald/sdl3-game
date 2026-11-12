#include "entities/frog.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/entity.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/physics.h"
#include "engine/sprites.h"
#include "paths.h"
#include "player/player-core.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

typedef enum {
    frog_anim_idle,
    frog_anim_walk,
    frog_anim_airborne_up,
    frog_anim_airborne_down,
} frog_anim;

// What a frog is doing. It's `frog_idle` while the player is close enough, and
// `frog_following` while moving towards them. When it touches a fluid it
// avoids, it's `frog_escaping`, moving towards the nearest place without any.
// When it can't move from where it is, like after a move was interrupted in
// the air, it's `frog_interrupted` until it can. When it can't get any closer,
// it's `frog_out_of_reach`, staying still and trying again every so often.
typedef enum {
    frog_idle,
    frog_following,
    frog_escaping,
    frog_interrupted,
    frog_out_of_reach,
} frog_mode;

// Every frog is the same, so the values each one's `entity` starts with are
// kept on `entity`. `follow_distance` is how close, in pixels, it gets to the
// player before stopping, and `out_of_reach_retry` how many seconds it waits
// to try again when it can't get any closer.
typedef struct {
    entity entity;
    double follow_distance;
    double out_of_reach_retry;
    Uint8 coords_x, coords_y, coords_w, coords_h;
    Sint8 offset_x, offset_y;
    animation idle_anim;
    animation walk_anim;
    animation airborne_up_anim;
    animation airborne_down_anim;
} frog_data;

// What each frog keeps on its entity's `state`. `retry_timer` counts down to
// when it tries to move again while it's `frog_out_of_reach`.
typedef struct {
    size_t sprite_id;
    frog_anim anim;
    Sint8 facing;
    frog_mode mode;
    double retry_timer;
} frog_state;

static const char frog_csv[] = {
#embed FROG_CSV_PATH suffix(, '\0')
};

static frog_data data = (frog_data){0};



static void play_anim(frog_state* s) {
    switch (s->anim) {
    case frog_anim_idle:
        SPRITES_play_anim(s->sprite_id, data.idle_anim);
        break;
    case frog_anim_walk:
        SPRITES_play_anim(s->sprite_id, data.walk_anim);
        break;
    case frog_anim_airborne_up:
        SPRITES_play_anim(s->sprite_id, data.airborne_up_anim);
        break;
    case frog_anim_airborne_down:
        SPRITES_play_anim(s->sprite_id, data.airborne_down_anim);
        break;
    }
}

static void on_create(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    frog_state* s = e.state;
    SPRITES_new_sprite(
        (sprite){
            .coords_x = data.coords_x,
            .coords_y = data.coords_y,
            .coords_w = data.coords_w,
            .coords_h = data.coords_h,
            .tint_r = 1.0,
            .tint_g = 1.0,
            .tint_b = 1.0,
            .tint_a = 1.0,
        },
        &s->sprite_id);
    play_anim(s);
}

static void on_destroy(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    frog_state* s = e.state;
    SPRITES_del_sprite(s->sprite_id);
    SDL_free(s);
}

// What the frog `s` is doing after trying to move, with the result `moved` of
// `ENTITY_move_towards()`, `frog_following` or `frog_escaping` being what it
// does when it started a move.
static void set_mode(frog_state* s, int moved, frog_mode moving) {
    switch (moved) {
    case 0:
        s->mode = moving;
        break;
    case 5:
        s->mode = frog_interrupted;
        break;
    case 7:
        s->mode = frog_out_of_reach;
        s->retry_timer = data.out_of_reach_retry;
        break;
    default:
        s->mode = frog_idle;
        break;
    }
}

// Moves towards the nearest place without fluids it avoids, through them.
// When there's none, it swims up towards the air above it and, once it can't
// get any higher, it jumps in panic, landing back where it was, which ends the
// move so it tries again. When there's no air above it either, it stays put.
static void escape(size_t entity_id, frog_state* s) {
    Uint16 x = 0;
    Uint16 y = 0;
    double px = 0.0;
    double py = 0.0;
    int moved = 0;
    int found = ENTITY_find_nearest_safe_tile(entity_id, &x, &y);
    ENTITY_get_tile_target(x, y, &px, &py);
    switch (found) {
    case 0:
        moved = ENTITY_move_towards_ignoring_unsafe_fluids(entity_id, px, py);
        break;
    case 6:
        moved = ENTITY_move_towards_ignoring_unsafe_fluids(entity_id, px, py);
        if (moved == 6 || moved == 7) {
            ENTITY_get_node(entity_id, &x, &y);
            ENTITY_get_tile_target(x, y, &px, &py);
            moved = ENTITY_jump(entity_id, data.entity.jump, px, py);
        }
        break;
    case 5:
        moved = 5;
        break;
    default:
        moved = 7;
        break;
    }
    set_mode(s, moved, frog_escaping);
}

// Moves towards the player unless it's within `follow_distance` of them on the
// same floor.
static void follow(size_t entity_id, frog_state* s) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    player_state player = PLAYER_get_player();
    body pb = (body){0};
    if (!player.created || PHYSICS_get_body(player.body_id, &pb)) {
        s->mode = frog_idle;
        return;
    }
    double px = pb.x + pb.w / 2.0;
    double py = pb.y + pb.h;
    double fx = b.x + b.w / 2.0;
    double fy = b.y + b.h;
    if (SDL_fabs(px - fx) <= data.follow_distance &&
        SDL_fabs(py - fy) < SPRITESHEET_CELL_Y) {
        s->mode = frog_idle;
        return;
    }
    set_mode(s, ENTITY_move_towards(entity_id, px, py), frog_following);
}

// Decides what the frog at `entity_id` does now that it isn't moving: escaping
// when it touches a fluid it avoids, otherwise following the player. While
// it's out of reach, it waits to try again.
static void decide(size_t entity_id, frog_state* s) {
    if (s->mode == frog_out_of_reach && s->retry_timer > 0.0) return;
    bool swimming = false;
    bool unsafe = false;
    ENTITY_get_fluids(entity_id, &swimming, &unsafe);
    if (unsafe)
        escape(entity_id, s);
    else
        follow(entity_id, s);
}

static void on_move_finished(size_t entity_id,
                             entity_move_result _result) { // NOLINT
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    decide(entity_id, e.state);
}

// Swimming, it's idle or walking like on the floor.
static void
update_sprite(frog_state* s, size_t entity_id, size_t body_id, body b) {
    bool on_floor = false;
    PHYSICS_is_body_on_floor(body_id, &on_floor);
    bool swimming = false;
    bool unsafe = false;
    ENTITY_get_fluids(entity_id, &swimming, &unsafe);
    frog_anim anim = frog_anim_idle;
    if (!on_floor && !swimming)
        anim = b.vel_y < 0.0 ? frog_anim_airborne_up : frog_anim_airborne_down;
    else if (SDL_fabs(b.vel_x) > EPSILON)
        anim = frog_anim_walk;
    if (anim != s->anim) {
        s->anim = anim;
        play_anim(s);
    }
    if (b.vel_x > EPSILON)
        s->facing = 1;
    else if (b.vel_x < -EPSILON)
        s->facing = -1;

    sprite spr = (sprite){0};
    SPRITES_get_sprite(s->sprite_id, &spr);
    spr.pos_x = (Sint32)SDL_round(b.x) + data.offset_x;
    spr.pos_y = (Sint32)SDL_round(b.y) + data.offset_y;
    spr.flip_mode = s->facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    SPRITES_edit_sprite(s->sprite_id, spr);
}

// Moves only start here when it isn't moving, like while idle, interrupted or
// out of reach. Otherwise, they start when the last one finished.
static void update(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    frog_state* s = e.state;
    s->retry_timer -= GAME_MANAGER_get_current_dt();
    bool moving = false;
    ENTITY_is_moving(entity_id, &moving);
    if (!moving) decide(entity_id, s);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    update_sprite(s, entity_id, e.body_id, b);
}



size_t FROG_create(double x, double y) {
    Uint8 w = 0;
    Uint8 h = 0;
    ENTITY_get_size(data.entity.size, &w, &h);
    size_t body_id = 0;
    PHYSICS_new_body(
        (body){
            .type = physics_actor,
            .x = x - w / 2.0,
            .y = y - h,
            .w = w,
            .h = h,
        },
        &body_id);
    frog_state* s = SDL_calloc(1, sizeof(frog_state));
    s->anim = frog_anim_idle;
    s->facing = 1;
    s->mode = frog_idle;
    entity e = data.entity;
    e.body_id = body_id;
    e.state = s;
    size_t entity_id = 0;
    ENTITY_new_entity(e, &entity_id);
    return entity_id;
}



void FROG_start(void) {
    frog_data* d = &data;
    entity* e = &d->entity;
    parsing_field fields[] = {
        {"jump_class", parsing_jump_class, &e->jump},
        {"fall_class", parsing_fall_class, &e->fall},
        {"run_class", parsing_run_class, &e->run},
        {"swim_class", parsing_swim_class, &e->swim},
        {"size_class", parsing_size_class, &e->size},
        {"follow_distance", parsing_double, &d->follow_distance},
        {"out_of_reach_retry", parsing_double, &d->out_of_reach_retry},
        {"coords", parsing_uint8, &d->coords_x},
        {"coords", parsing_uint8, &d->coords_y, .at = 1},
        {"coords", parsing_uint8, &d->coords_w, .at = 2},
        {"coords", parsing_uint8, &d->coords_h, .at = 3},
        {"offset", parsing_sint8, &d->offset_x},
        {"offset", parsing_sint8, &d->offset_y, .at = 1},
        PARSING_ANIM_FIELDS("idle", d->idle_anim),
        PARSING_ANIM_FIELDS("walk", d->walk_anim),
        PARSING_ANIM_FIELDS("airborne_up", d->airborne_up_anim),
        PARSING_ANIM_FIELDS("airborne_down", d->airborne_down_anim),
    };
    dynarr parsed = CSV_parse(frog_csv);
    read_csv_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    d->idle_anim.looped = true;
    d->walk_anim.looped = true;
    d->airborne_up_anim.looped = true;
    d->airborne_down_anim.looped = true;

    e->moves = move_jump | move_fall | move_drop | move_swim;
    e->safe_fluids = 1 << fluid_water;
    e->on_create_func = on_create;
    e->on_destroy_func = on_destroy;
    e->update_func = update;
    e->on_move_finished_func = on_move_finished;
    ENTITY_register_classes(*e);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frog data loaded successfully.");
}
