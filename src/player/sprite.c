#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <data-structs/dynarr.h>
#include <defs.h>
#include <engine/physics.h>
#include <engine/sprites.h>
#include <player/player.h>
#include <player/sprite.h>
#include <services/csv.h>

typedef enum {
    state_idle,
    state_moving,
    state_airborne,
} states_machine_state;

typedef struct {
    Uint8 coords_x;
    Uint8 coords_y;
    Uint8 coords_w;
    Uint8 coords_h;
    Sint8 offset_x;
    Sint8 offset_y;
    animation idle_anim;
    animation moving_anim;
    animation airborne_anim;
} sprite_data;

typedef struct {
    bool created;
    size_t sprite_id;
    states_machine_state state;
} sprite_state;

static sprite_data data = (sprite_data){0};
static sprite_state state = (sprite_state){0};



static void state_changed(states_machine_state new_state) {
    switch (new_state) {
    case state_idle:
        SPRITES_play_anim(state.sprite_id, data.idle_anim);
        break;
    case state_moving:
        SPRITES_play_anim(state.sprite_id, data.moving_anim);
        break;
    case state_airborne:
        SPRITES_play_anim(state.sprite_id, data.airborne_anim);
        break;
    }
}



void PLAYER_SPRITE_start(void) {
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
            data.coords_x = SDL_strtol(x, NULL, 10);
            data.coords_y = SDL_strtol(y, NULL, 10);
            data.coords_w = SDL_strtol(w, NULL, 10);
            data.coords_h = SDL_strtol(h, NULL, 10);
        }
        if (SDL_strcmp(key, "offset") == 0) {
            char* x = (char*)((dynarr*)parsed.at[i])->at[1];
            char* y = (char*)((dynarr*)parsed.at[i])->at[2];
            data.offset_x = SDL_strtol(x, NULL, 10);
            data.offset_y = SDL_strtol(y, NULL, 10);
        }
        data.idle_anim.looped = true;
        if (SDL_strcmp(key, "idle_anim_x") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.idle_anim.frames[j].coords_x = SDL_strtol(val, NULL, 10);
            }
        }
        if (SDL_strcmp(key, "idle_anim_y") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.idle_anim.frames[j].coords_y = SDL_strtol(val, NULL, 10);
            }
        }
        if (SDL_strcmp(key, "idle_anim_dur") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.idle_anim.frames[j].duration = SDL_strtod(val, NULL);
            }
        }
        data.moving_anim.looped = true;
        if (SDL_strcmp(key, "moving_anim_x") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.moving_anim.frames[j].coords_x = SDL_strtol(val, NULL, 10);
            }
        }
        if (SDL_strcmp(key, "moving_anim_y") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.moving_anim.frames[j].coords_y = SDL_strtol(val, NULL, 10);
            }
        }
        if (SDL_strcmp(key, "moving_anim_dur") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.moving_anim.frames[j].duration = SDL_strtod(val, NULL);
            }
        }
        data.airborne_anim.looped = true;
        if (SDL_strcmp(key, "airborne_anim_x") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.airborne_anim.frames[j].coords_x =
                    SDL_strtol(val, NULL, 10);
            }
        }
        if (SDL_strcmp(key, "airborne_anim_y") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.airborne_anim.frames[j].coords_y =
                    SDL_strtol(val, NULL, 10);
            }
        }
        if (SDL_strcmp(key, "airborne_anim_dur") == 0) {
            for (int j = 0; j < ((dynarr*)parsed.at[i])->len - 1; j++) {
                char* val = (char*)((dynarr*)parsed.at[i])->at[j + 1];
                data.airborne_anim.frames[j].duration = SDL_strtod(val, NULL);
            }
        }
    }

    CSV_free(&parsed);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Player sprite data loaded successfully.");
}

void PLAYER_SPRITE_create(void) {
    SPRITES_new_sprite(
        (sprite){
            .coords_x = data.coords_x,
            .coords_y = data.coords_y,
            .coords_w = data.coords_w,
            .coords_h = data.coords_h,
        },
        &state.sprite_id);
    state.created = true;
    state_changed(state.state);
}

void PLAYER_SPRITE_destroy(void) {
    SPRITES_del_sprite(state.sprite_id);
    state.created = false;
}



void PLAYER_SPRITE_update(player_state player) {
    if (!state.created) return;

    sprite s = (sprite){0};
    body b = (body){0};
    SPRITES_get_sprite(state.sprite_id, &s);
    PHYSICS_get_body(player.body_id, &b);

    s.pos_x = (Uint32)SDL_roundf(b.x) + data.offset_x;
    s.pos_y = (Uint32)SDL_roundf(b.y) + data.offset_y;
    s.flip_mode = player.facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

    SPRITES_update_sprite(state.sprite_id, s);

    states_machine_state new_state = state.state;
    bool is_on_floor = false;
    PHYSICS_is_on_floor(player.body_id, &is_on_floor);
    if (is_on_floor && SDL_abs(b.vel_x) < FABS_ZERO_DIFF)
        new_state = state_idle;
    else if (is_on_floor)
        new_state = state_moving;
    else if (!is_on_floor)
        new_state = state_airborne;
    if (new_state != state.state) {
        state.state = new_state;
        state_changed(new_state);
    }
}
