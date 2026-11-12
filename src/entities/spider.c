#include "entities/spider.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/entity.h"
#include "engine/game-manager.h"
#include "engine/physics.h"
#include "engine/sprites.h"
#include "paths.h"
#include "player/player-core.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>

typedef enum {
    spider_anim_idle,
    spider_anim_walk,
    spider_anim_airborne,
} spider_anim;

// What a spider is doing. It's `spider_idle` while the player is close enough,
// and `spider_following` while moving towards them. When it can't move from
// where it is, like after a move was interrupted in the air, it's
// `spider_interrupted` until it can. When it can't get any closer, it's
// `spider_out_of_reach`, staying still and trying again every so often.
typedef enum {
    spider_idle,
    spider_following,
    spider_interrupted,
    spider_out_of_reach,
} spider_mode;

// Every spider is the same, so the values each one's `entity` starts with are
// kept on `entity`. `follow_distance` is how close, in pixels, it gets to the
// player before stopping, and `out_of_reach_retry` how many seconds it waits
// to try again when it can't get any closer. Its sprite is drawn `offset`
// pixels from its body, or `ceiling_offset` upside down on ceilings, and
// `left_wall_offset` or `right_wall_offset` turned on walls on its left or
// right.
typedef struct {
    entity entity;
    double follow_distance;
    double out_of_reach_retry;
    Uint8 coords_x, coords_y, coords_w, coords_h;
    Sint8 offset_x, offset_y;
    Sint8 ceiling_offset_x, ceiling_offset_y;
    Sint8 left_wall_offset_x, left_wall_offset_y;
    Sint8 right_wall_offset_x, right_wall_offset_y;
    animation idle_anim;
    animation walk_anim;
    animation airborne_anim;
} spider_data;

// What each spider keeps on its entity's `state`. `retry_timer` counts down to
// when it tries to move again while it's `spider_out_of_reach`. It faces
// `facing` sideways, and `facing_y` up or down on walls, `-1` for up.
typedef struct {
    size_t sprite_id;
    spider_anim anim;
    Sint8 facing;
    Sint8 facing_y;
    spider_mode mode;
    double retry_timer;
} spider_state;

static const char spider_csv[] = {
#embed SPIDER_CSV_PATH suffix(, '\0')
};

static spider_data data = (spider_data){0};



static void play_anim(spider_state* s) {
    switch (s->anim) {
    case spider_anim_idle:
        SPRITES_play_anim(s->sprite_id, data.idle_anim);
        break;
    case spider_anim_walk:
        SPRITES_play_anim(s->sprite_id, data.walk_anim);
        break;
    case spider_anim_airborne:
        SPRITES_play_anim(s->sprite_id, data.airborne_anim);
        break;
    }
}

static void on_create(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    spider_state* s = e.state;
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
    spider_state* s = e.state;
    SPRITES_del_sprite(s->sprite_id);
    SDL_free(s);
}

// Moves towards the player unless it's within `follow_distance` of them on the
// same floor, setting what it's doing after.
static void follow(size_t entity_id, spider_state* s) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    player_state player = PLAYER_get_player();
    body pb = (body){0};
    if (!player.created || PHYSICS_get_body(player.body_id, &pb)) {
        s->mode = spider_idle;
        return;
    }
    double px = pb.x + pb.w / 2.0;
    double py = pb.y + pb.h;
    double sx = b.x + b.w / 2.0;
    double sy = b.y + b.h;
    if (SDL_fabs(px - sx) <= data.follow_distance &&
        SDL_fabs(py - sy) < SPRITESHEET_CELL_Y) {
        s->mode = spider_idle;
        return;
    }
    switch (ENTITY_move_towards(entity_id, px, py)) {
    case 0:
        s->mode = spider_following;
        break;
    case 5:
        s->mode = spider_interrupted;
        break;
    case 7:
        s->mode = spider_out_of_reach;
        s->retry_timer = data.out_of_reach_retry;
        break;
    default:
        s->mode = spider_idle;
        break;
    }
}

// Decides what the spider at `entity_id` does now that it isn't moving. While
// it's out of reach, it waits to try again.
static void decide(size_t entity_id, spider_state* s) {
    if (s->mode == spider_out_of_reach && s->retry_timer > 0.0) return;
    follow(entity_id, s);
}

static void on_move_finished(size_t entity_id,
                             entity_move_result _result) { // NOLINT
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    decide(entity_id, e.state);
}

// Clinging to a ceiling, it's drawn upside down, and on walls turned so its
// legs are on the wall's side: turned clockwise for walls on its left, which
// puts what's on the bottom of its sprite on the left, and flipped both ways
// too for walls on its right. Turned, flipping its sprite sideways flips it up
// and down on the screen, so it faces where it climbs. Clinging to anything,
// it's walking while it moves either way, otherwise it's in the air when it
// isn't on the floor.
static void
update_sprite(spider_state* s, size_t entity_id, size_t body_id, body b) {
    bool on_floor = false;
    PHYSICS_is_body_on_floor(body_id, &on_floor);
    entity_surface surface = surface_none;
    ENTITY_get_clinging(entity_id, &surface);
    bool clinging = surface != surface_none;
    bool moving = SDL_fabs(b.vel_x) > EPSILON ||
                  (clinging && SDL_fabs(b.vel_y) > EPSILON);
    spider_anim anim = spider_anim_idle;
    if (!on_floor && !clinging)
        anim = spider_anim_airborne;
    else if (moving)
        anim = spider_anim_walk;
    if (anim != s->anim) {
        s->anim = anim;
        play_anim(s);
    }
    if (b.vel_x > EPSILON)
        s->facing = 1;
    else if (b.vel_x < -EPSILON)
        s->facing = -1;
    if (b.vel_y > EPSILON)
        s->facing_y = 1;
    else if (b.vel_y < -EPSILON)
        s->facing_y = -1;

    Sint8 offset_x = data.offset_x;
    Sint8 offset_y = data.offset_y;
    SDL_FlipMode flip = s->facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    sprite_rotation rotation = rotation_none;
    SDL_FlipMode climbing_up =
        s->facing_y < 0 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    switch (surface) {
    case surface_none:
        break;
    case surface_ceiling:
        offset_x = data.ceiling_offset_x;
        offset_y = data.ceiling_offset_y;
        flip |= SDL_FLIP_VERTICAL;
        break;
    case surface_wall_left:
        offset_x = data.left_wall_offset_x;
        offset_y = data.left_wall_offset_y;
        flip = climbing_up;
        rotation = rotation_90;
        break;
    case surface_wall_right:
        offset_x = data.right_wall_offset_x;
        offset_y = data.right_wall_offset_y;
        flip = SDL_FLIP_VERTICAL | climbing_up;
        rotation = rotation_90;
        break;
    }

    sprite spr = (sprite){0};
    SPRITES_get_sprite(s->sprite_id, &spr);
    spr.pos_x = (Sint32)SDL_round(b.x) + offset_x;
    spr.pos_y = (Sint32)SDL_round(b.y) + offset_y;
    spr.flip_mode = flip;
    spr.rotation = rotation;
    SPRITES_edit_sprite(s->sprite_id, spr);
}

// Moves only start here when it isn't moving, like while idle, interrupted or
// out of reach. Otherwise, they start when the last one finished.
static void update(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    spider_state* s = e.state;
    s->retry_timer -= GAME_MANAGER_get_current_dt();
    bool moving = false;
    ENTITY_is_moving(entity_id, &moving);
    if (!moving) decide(entity_id, s);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    update_sprite(s, entity_id, e.body_id, b);
}



size_t SPIDER_create(double x, double y) {
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
    spider_state* s = SDL_calloc(1, sizeof(spider_state));
    s->anim = spider_anim_idle;
    s->facing = 1;
    s->facing_y = -1;
    s->mode = spider_idle;
    entity e = data.entity;
    e.body_id = body_id;
    e.state = s;
    size_t entity_id = 0;
    ENTITY_new_entity(e, &entity_id);
    return entity_id;
}



void SPIDER_start(void) {
    spider_data* d = &data;
    entity* e = &d->entity;
    parsing_field fields[] = {
        {"fall_class", parsing_fall_class, &e->fall},
        {"run_class", parsing_run_class, &e->run},
        {"size_class", parsing_size_class, &e->size},
        {"follow_distance", parsing_double, &d->follow_distance},
        {"out_of_reach_retry", parsing_double, &d->out_of_reach_retry},
        {"coords", parsing_uint8, &d->coords_x},
        {"coords", parsing_uint8, &d->coords_y, .at = 1},
        {"coords", parsing_uint8, &d->coords_w, .at = 2},
        {"coords", parsing_uint8, &d->coords_h, .at = 3},
        {"offset", parsing_sint8, &d->offset_x},
        {"offset", parsing_sint8, &d->offset_y, .at = 1},
        {"ceiling_offset", parsing_sint8, &d->ceiling_offset_x},
        {"ceiling_offset", parsing_sint8, &d->ceiling_offset_y, .at = 1},
        {"left_wall_offset", parsing_sint8, &d->left_wall_offset_x},
        {"left_wall_offset", parsing_sint8, &d->left_wall_offset_y, .at = 1},
        {"right_wall_offset", parsing_sint8, &d->right_wall_offset_x},
        {"right_wall_offset", parsing_sint8, &d->right_wall_offset_y, .at = 1},
        PARSING_ANIM_FIELDS("idle", d->idle_anim),
        PARSING_ANIM_FIELDS("walk", d->walk_anim),
        PARSING_ANIM_FIELDS("airborne", d->airborne_anim),
    };
    dynarr parsed = CSV_parse(spider_csv);
    read_csv_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    d->idle_anim.looped = true;
    d->walk_anim.looped = true;
    d->airborne_anim.looped = true;

    e->moves =
        move_run | move_fall | move_drop | move_climb_wall | move_climb_ceiling;
    e->on_create_func = on_create;
    e->on_destroy_func = on_destroy;
    e->update_func = update;
    e->on_move_finished_func = on_move_finished;
    ENTITY_register_classes(*e);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Spider data loaded successfully.");
}
