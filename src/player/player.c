#include <SDL3/SDL.h>
#include <data-structs/dynarr.h>
#include <defs.h>
#include <engine/game-manager.h>
#include <engine/inputs.h>
#include <engine/physics.h>
#include <engine/sprites.h>
#include <player/player.h>
#include <services/csv.h>

typedef struct {
    // Base data
    double max_speed;
    double acceleration;
    double friction;
    double gravity;
    double max_fall_speed;
    double jump_speed;
    double jump_cut;

    // Sprite data
    Uint8 sx;
    Uint8 sy;
    Uint8 sw;
    Uint8 sh;
} player_data;

typedef struct {
    bool loaded;
    size_t sprite_id;
    size_t body_id;
    Sint8 facing;
    bool jumping;
} player_state;

static player_data data = (player_data){0};
static player_state state = (player_state){0};



void PLAYER_create(double at_x, double at_y) {
    PHYSICS_new_body(
        (body){
            .type = physics_actor,
            .x = at_x,
            .y = at_y,
            .w = SPRITESHEET_CELL_X * data.sw,
            .h = SPRITESHEET_CELL_Y * data.sh,
        },
        &state.body_id);
    SPRITES_new_sprite(
        (sprite){
            .coords_x = data.sx,
            .coords_y = data.sy,
            .coords_w = data.sw,
            .coords_h = data.sh,
        },
        &state.sprite_id);
    state.loaded = true;
}

void PLAYER_destroy(void) {
    PHYSICS_del_body(state.body_id);
    SPRITES_del_sprite(state.sprite_id);
    state.loaded = false;
}



static void update_sprite(sprite s, body b, Sint8 dir) {
    if (dir && dir != state.facing) {
        s.flip_mode = dir == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
        state.facing = dir;
    }
    s.pos_x = (Uint32)SDL_roundf(b.x);
    s.pos_y = (Uint32)SDL_roundf(b.y);
    SPRITES_update_sprite(state.sprite_id, s);
}

static void update_body(body b, Sint8 dir) {
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
    else {
        new_x += new_x > 0.0f ? -data.friction * dt : data.friction * dt;
        if (new_x * b.vel_x < 0.0f) new_x = 0.0f;
    }

    // Move vertically
    new_y += data.gravity * dt;
    if (new_y >= data.max_fall_speed) new_y = data.max_fall_speed;

    PHYSICS_set_velocity(state.body_id, new_x, new_y);
}

void PLAYER_update(void) {
    if (!state.loaded) return;

    Sint8 dir = INPUTS_get_x_dir();
    body b = (body){0};
    sprite s = (sprite){0};
    PHYSICS_get_body(state.body_id, &b);
    SPRITES_get_sprite(state.sprite_id, &s);

    update_sprite(s, b, dir);
    update_body(b, dir);
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
    }

    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Player base data loaded successfully.");
}

static void load_sprite_data_from_csv(void) {
    char csv[] = {
#embed PLAYER_SPRITE_CSV_PATH suffix(, '\0')
    };

    dynarr parsed = CSV_parse(csv);

    for (int i = 0; i < parsed.len; i++) {
        char* key = (char*)((dynarr*)parsed.at[i])->at[0];
        if (SDL_strcmp(key, "coords") == 0) {
            char* x = (char*)((dynarr*)parsed.at[i])->at[1];
            char* y = (char*)((dynarr*)parsed.at[i])->at[2];
            char* w = (char*)((dynarr*)parsed.at[i])->at[3];
            char* h = (char*)((dynarr*)parsed.at[i])->at[4];
            data.sx = SDL_strtol(x, NULL, 10);
            data.sy = SDL_strtol(y, NULL, 10);
            data.sw = SDL_strtol(w, NULL, 10);
            data.sh = SDL_strtol(h, NULL, 10);
        }
    }

    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Player sprite data loaded successfully.");
}

void PLAYER_start(void) {
    load_base_data_from_csv();
    load_sprite_data_from_csv();
}
