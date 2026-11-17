#include "entities/wisp.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/entity.h"
#include "engine/game-manager.h"
#include "engine/particles.h"
#include "engine/physics.h"
#include "engine/sprites.h"
#include "paths.h"
#include "player/player-core.h"
#include "services/csv.h"
#include "utils/parsing.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>

// What a wisp is doing. It's `wisp_idle` while the player is close enough, and
// `wisp_following` while flying towards them. When it can't move from where it
// is, it's `wisp_interrupted` until it can. When it can't get any closer, it's
// `wisp_out_of_reach`, staying where it is and trying again every so often.
typedef enum {
    wisp_idle,
    wisp_following,
    wisp_interrupted,
    wisp_out_of_reach,
} wisp_mode;

// Every wisp is the same, so the values each one's `entity` starts with are
// kept on `entity`. `follow_distance` is how close, in pixels, it gets to the
// player before stopping, and `out_of_reach_retry` how many seconds it waits
// to try again when it can't get any closer. Its sprite is drawn `offset`
// pixels from its body, always flying.
typedef struct {
    entity entity;
    double follow_distance;
    double out_of_reach_retry;
    Uint8 coords_x, coords_y, coords_w, coords_h;
    Sint8 offset_x, offset_y;
    animation fly_anim;
} wisp_data;

// What each wisp keeps on its entity's `state`. `retry_timer` counts down to
// when it tries to move again while it's `wisp_out_of_reach`.
typedef struct {
    wisp_mode mode;
    double retry_timer;
    double time;
} wisp_state;

static const char wisp_csv[] = {
#embed WISP_CSV_PATH suffix(, '\0')
};

static wisp_data data = (wisp_data){0};



static void on_create(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
}

static void on_destroy(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    wisp_state* s = e.state;
    SDL_free(s);
}

// Moves towards the player unless it's within `follow_distance` of them on the
// same height, setting what it's doing after.
static void follow(size_t entity_id, wisp_state* s) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    player_state player = PLAYER_get_player();
    body pb = (body){0};
    if (!player.created || PHYSICS_get_body(player.body_id, &pb)) {
        s->mode = wisp_idle;
        return;
    }
    double px = pb.x + pb.w / 2.0;
    double py = pb.y + pb.h;
    double wx = b.x + b.w / 2.0;
    double wy = b.y + b.h;
    if (SDL_fabs(px - wx) <= data.follow_distance &&
        SDL_fabs(py - wy) < SPRITESHEET_CELL_Y) {
        s->mode = wisp_idle;
        return;
    }
    switch (ENTITY_move_towards(entity_id, px, py)) {
    case 0:
        s->mode = wisp_following;
        break;
    case 5:
        s->mode = wisp_interrupted;
        break;
    case 7:
        s->mode = wisp_out_of_reach;
        s->retry_timer = data.out_of_reach_retry;
        break;
    default:
        s->mode = wisp_idle;
        break;
    }
}

// Decides what the wisp at `entity_id` does now that it isn't moving. While
// it's out of reach, it waits to try again.
static void decide(size_t entity_id, wisp_state* s) {
    if (s->mode == wisp_out_of_reach && s->retry_timer > 0.0) return;
    follow(entity_id, s);
}

static void on_move_finished(size_t entity_id,
                             entity_move_result _result) { // NOLINT
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    decide(entity_id, e.state);
}

// Moves only start here when it isn't moving, like while idle, interrupted or
// out of reach. Otherwise, they start when the last one finished.
static void update(size_t entity_id) {
    entity e = (entity){0};
    ENTITY_get_entity(entity_id, &e);
    wisp_state* s = e.state;
    s->retry_timer -= GAME_MANAGER_get_current_dt();
    bool moving = false;
    ENTITY_is_moving(entity_id, &moving);
    if (!moving) decide(entity_id, s);
    body b = (body){0};
    PHYSICS_get_body(e.body_id, &b);
    s->time += GAME_MANAGER_get_current_dt();
    if (s->time >= 0.05f) {
        s->time -= 0.05f;
        PARTICLES_emit(particle_flames_base,
                       (particles_emit_settings){
                           .shape = particles_emit_rect,
                           .rect = {.x = b.x, .y = b.y, .w = 4.0f, .h = 4.0f},
                           .vel = {.x = 0.0f, .y = -20.0f},
                           .count = 1,
                       });
    }
}



size_t WISP_create(double x, double y) {
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
    wisp_state* s = SDL_calloc(1, sizeof(wisp_state));
    s->mode = wisp_idle;
    entity e = data.entity;
    e.body_id = body_id;
    e.state = s;
    size_t entity_id = 0;
    ENTITY_new_entity(e, &entity_id);
    return entity_id;
}



void WISP_start(void) {
    wisp_data* d = &data;
    entity* e = &d->entity;
    parsing_field fields[] = {
        {"fly_class", parsing_fly_class, &e->fly},
        {"size_class", parsing_size_class, &e->size},
        {"follow_distance", parsing_double, &d->follow_distance},
        {"out_of_reach_retry", parsing_double, &d->out_of_reach_retry},
        {"coords", parsing_uint8, &d->coords_x},
        {"coords", parsing_uint8, &d->coords_y, .at = 1},
        {"coords", parsing_uint8, &d->coords_w, .at = 2},
        {"coords", parsing_uint8, &d->coords_h, .at = 3},
        {"offset", parsing_sint8, &d->offset_x},
        {"offset", parsing_sint8, &d->offset_y, .at = 1},
        PARSING_ANIM_FIELDS("fly", d->fly_anim),
    };
    dynarr parsed = CSV_parse(wisp_csv);
    read_csv_keys(&parsed, fields, SDL_arraysize(fields));
    CSV_free(&parsed);

    d->fly_anim.looped = true;

    e->moves = move_fly;
    e->on_create_func = on_create;
    e->on_destroy_func = on_destroy;
    e->update_func = update;
    e->on_move_finished_func = on_move_finished;
    ENTITY_register_classes(*e);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Wisp data loaded successfully.");
}
