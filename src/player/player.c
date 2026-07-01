#include "player/player.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/game-manager.h"
#include "engine/inputs.h"
#include "engine/physics.h"
#include "player/sprite.h"
#include "services/csv.h"
#include <SDL3/SDL.h>

typedef struct {
    double max_speed;
    double acceleration;
    double friction;
    double gravity;
    double max_fall_speed;
    double jump_speed;
    double jump_cut;
    Uint8 w;
    Uint8 h;
    double coyote_jump_time;
    double jump_buffer_time;
} player_data;

static player_data data = (player_data){0};
static player_state state = (player_state){0};



void PLAYER_create(double at_x, double at_y) {
    PHYSICS_new_body(
        (body){
            .type = physics_actor,
            .x = at_x,
            .y = at_y,
            .w = data.w,
            .h = data.h,
        },
        &state.body_id);
    state.created = true;
    state.facing = 1;
    PLAYER_SPRITE_create();
}

void PLAYER_destroy(void) {
    PHYSICS_del_body(state.body_id);
    PLAYER_SPRITE_destroy();
    state.created = false;
}



static void update_body(Sint8 dir) {
    body b = (body){0};
    PHYSICS_get_body(state.body_id, &b);

    double dt = GAME_MANAGER_get_current_dt();
    double new_x = b.vel_x;
    double new_y = b.vel_y;

    // Move horizontally
    if (dir) {
        // The player is pressing the direction against its current speed
        if (dir * new_x < 0.0f) {
            new_x += new_x > 0.0f ? -(data.friction + data.acceleration) * dt
                                  : (data.friction + data.acceleration) * dt;
        }
        // The player is pressing the direction of its current speed
        else {
            new_x = SDL_clamp(new_x + dir * data.acceleration * dt,
                              -data.max_speed,
                              data.max_speed);
        }
    }
    // The player is not pressing any direction
    else if (new_x != 0.0f) {
        new_x += new_x > 0.0f ? -data.friction * dt : data.friction * dt;
        if (new_x * b.vel_x < 0.0f) new_x = 0.0f;
    }

    // Jumping
    action_state jump_state = INPUTS_get_action_state(action_jump);
    bool can_jump =
        (state.coyote_jump_timer < data.coyote_jump_time) && !state.jumping;
    bool pressed_jump =
        jump_state.just_pressed ||
        (jump_state.held && jump_state.time_held < data.jump_buffer_time);
    if (can_jump && pressed_jump) {
        state.jumping = true;
        new_y = -data.jump_speed;
    } else if (state.jumping && jump_state.just_released) {
        state.jumping = false;
        new_y = b.vel_y * data.jump_cut;
    } else if (state.jumping && (-b.vel_y <= FABS_ZERO_DIFF || can_jump))
        state.jumping = false;

    // Coyote time
    bool is_on_floor = false;
    PHYSICS_is_on_floor(state.body_id, &is_on_floor);
    if (is_on_floor)
        state.coyote_jump_timer = 0.0f;
    else
        state.coyote_jump_timer += dt;

    // Gravity
    new_y += data.gravity * dt;
    if (new_y >= data.max_fall_speed) new_y = data.max_fall_speed;

    PHYSICS_set_velocity(state.body_id, new_x, new_y);
}

void PLAYER_update(void) {
    if (!state.created) return;

    Sint8 dir = INPUTS_get_x_dir();
    if (dir && dir != state.facing) state.facing = dir;

    update_body(dir);
    PLAYER_SPRITE_update(state);
}



static void load_base_data_from_csv(void) {
    char csv[] = {
#embed PLAYER_BASE_CSV_PATH suffix(, '\0')
    };

    dynarr parsed = CSV_parse(csv);

    for (int i = 0; i < parsed.len; i++) {
        char* key = (char*)((dynarr*)parsed.at[i])->at[0];
        char* val = (char*)((dynarr*)parsed.at[i])->at[1];
        if (SDL_strcmp(key, "max_speed") == 0)
            data.max_speed = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "acceleration") == 0)
            data.acceleration = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "friction") == 0)
            data.friction = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "gravity") == 0)
            data.gravity = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "max_fall_speed") == 0)
            data.max_fall_speed = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "jump_speed") == 0)
            data.jump_speed = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "jump_cut") == 0)
            data.jump_cut = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "width") == 0)
            data.w = SDL_strtol(val, NULL, 10);
        else if (SDL_strcmp(key, "height") == 0)
            data.h = SDL_strtol(val, NULL, 10);
        else if (SDL_strcmp(key, "coyote_jump_time") == 0)
            data.coyote_jump_time = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "jump_buffer_time") == 0)
            data.jump_buffer_time = SDL_strtod(val, NULL);
    }

    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Player base data loaded successfully.");
}

void PLAYER_start(void) {
    load_base_data_from_csv();
    PLAYER_SPRITE_start();
}
