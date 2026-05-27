#include "sprites.h"
#include "data-structs/list.h"
#include "defs.h"
#include "game-manager.h"
#include <SDL3/SDL.h>

#define STARTING_DYN_ARRS_LENGTH 1048576 // 1024 * 1024
#define Z_INDEX_TOTAL 256 // z_index is Sint8, varies from -127 to 127

static int dynarrs_size = STARTING_DYN_ARRS_LENGTH;
static sprite* sprites = NULL;
static animation* animations = NULL;
static float* anim_times = NULL;
static bool* anim_playing = NULL;
static list z_index_map[Z_INDEX_TOTAL] = {{NULL}};

static SDL_Texture* spritesheet = NULL;



static void start_dyn_arrs(void) {
    sprites = SDL_calloc(STARTING_DYN_ARRS_LENGTH, sizeof(sprite));
    animations = SDL_calloc(STARTING_DYN_ARRS_LENGTH, sizeof(animation));
    anim_times = SDL_calloc(STARTING_DYN_ARRS_LENGTH, sizeof(float));
    anim_playing = SDL_calloc(STARTING_DYN_ARRS_LENGTH, sizeof(bool));
}


static void stop_dyn_arrs(void) {
    SDL_free(sprites);
    SDL_free(animations);
    SDL_free(anim_times);
    SDL_free(anim_playing);
}


static void double_dyn_arrs(void) {
    sprite* new_sprites = SDL_calloc(2 * dynarrs_size, sizeof(sprite));
    animation* new_animations = SDL_calloc(2 * dynarrs_size, sizeof(animation));
    float* new_anim_times = SDL_calloc(2 * dynarrs_size, sizeof(float));
    bool* new_anim_playing = SDL_calloc(2 * dynarrs_size, sizeof(bool));
    for (int i = 0; i < dynarrs_size; i++) {
        new_sprites[i] = sprites[i];
        new_animations[i] = animations[i];
        new_anim_times[i] = anim_times[i];
        new_anim_playing[i] = anim_playing[i];
    }
    SDL_free(sprites);
    SDL_free(animations);
    SDL_free(anim_times);
    SDL_free(anim_playing);
    dynarrs_size = 2 * dynarrs_size;
}



static void add_to_z_index_map(int sprite_id) {
    int map_index = sprites[sprite_id].z_index + Z_INDEX_TOTAL / 2;
    push_to_list(sprite_id, &z_index_map[map_index]);
}


static void remove_from_z_index_map(int sprite_id) {
    int map_index = sprites[sprite_id].z_index + Z_INDEX_TOTAL / 2;
    remove_from_list(sprite_id, &z_index_map[map_index]);
}



int SPRITES_new_sprite(sprite data, int* sprite_id) {
    if (data.coords_w == 0 || data.coords_h == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to create sprite with invalid data: { coords_w = "
                    "%d, coords_h = %d }",
                    data.coords_w,
                    data.coords_h);
        return 2;
    }
    for (int i = 0; i < dynarrs_size; i++) {
        if (sprites[i].coords_w == 0) {
            sprites[i] = data;
            add_to_z_index_map(i);
            *sprite_id = i;
            return 0;
        }
    }
    double_dyn_arrs();
    int index = dynarrs_size / 2 + 1;
    sprites[index] = data;
    add_to_z_index_map(index);
    *sprite_id = index;
    return 0;
}


int SPRITES_del_sprite(int sprite_id) {
    if (sprite_id >= dynarrs_size || sprite_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to delete sprite_id=%d but this is outside the buffer range",
            sprite_id);
        return 3;
    }
    if (sprites[sprite_id].coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent sprite at sprite_id=%d",
                    sprite_id);
        return 1;
    }
    remove_from_z_index_map(sprite_id);
    sprites[sprite_id] = (sprite){0};
    return 0;
}


int SPRITES_get_sprite(int sprite_id, sprite* data) {
    if (sprite_id >= dynarrs_size) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to get sprite_id=%d but this is outside the buffer range",
            sprite_id);
        return 3;
    }
    if (sprites[sprite_id].coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent sprite at sprite_id=%d",
                    sprite_id);
        return 1;
    }

    *data = sprites[sprite_id];
    return 0;
}


int SPRITES_update_sprite(int sprite_id, sprite data) {
    if (sprite_id >= dynarrs_size || sprite_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to update sprite_id=%d but this is outside the buffer range",
            sprite_id);
        return 3;
    }
    if (data.coords_w == 0 || data.coords_h == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to update with invalid sprite data: { coords_w = "
                    "%d, coords_h = %d }",
                    data.coords_w,
                    data.coords_h);
        return 2;
    }
    if (sprites[sprite_id].coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to update inexistent sprite at sprite_id=%d",
                    sprite_id);
        return 1;
    }

    if (data.z_index != sprites[sprite_id].z_index) {
        remove_from_z_index_map(sprite_id);
        sprites[sprite_id] = data;
        add_to_z_index_map(sprite_id);
    } else {
        sprites[sprite_id] = data;
    }
    return 0;
}



int SPRITES_start(char* base_path, SDL_Renderer* renderer) {
    char* sheet_path = NULL;
    SDL_asprintf(
        &sheet_path, "%s%sspritesheet.png", base_path, RES_TEXTURES_PATH);
    SDL_Surface* surface = SDL_LoadPNG(sheet_path);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Couldn't load spritesheet: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_free(sheet_path);
    spritesheet = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Spritesheet loaded successfully.");
    start_dyn_arrs();
    return 0;
}



int SPRITES_play_anim(int sprite_id, animation anim) {
    if (sprite_id >= dynarrs_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate sprite_id=%d but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate inexistent sprite at sprite_id=%d",
                    sprite_id);
        return 1;
    }

    animations[sprite_id] = anim;
    anim_playing[sprite_id] = true;
    anim_times[sprite_id] = 0.0f;
    return 0;
}

int SPRITES_stop_anim(int sprite_id) {
    if (sprite_id >= dynarrs_size || sprite_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in sprite_id=%d but this is outside the "
            "buffer range",
            sprite_id);
        return 3;
    }
    if (sprites[sprite_id].coords_w == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in inexistent sprite at sprite_id=%d",
            sprite_id);
        return 1;
    }

    anim_playing[sprite_id] = false;
    anim_times[sprite_id] = 0.0f;
    return 0;
}

int SPRITES_anim_finished(int sprite_id, bool* finished) {
    if (sprite_id >= dynarrs_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check animation end in sprite_id=%d but this is "
                    "outside the buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].coords_w == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to check animation end in inexistent sprite at sprite_id=%d",
            sprite_id);
        return 1;
    }

    *finished = anim_times[sprite_id] == -1.0f;
    return 0;
}



void SPRITES_update(SDL_Renderer* renderer) {
    for (int i = 0; i < Z_INDEX_TOTAL; i++) {
        node* curr = z_index_map[i].head;
        while (curr) {
            int id = curr->val;

            // Update animation status
            if (anim_playing[id]) {
                anim_times[id] += GAME_MANAGER_get_main_dt();
                animation anim = animations[id];
                int i = 0;
                float time = anim_times[id];
                while (time >= 0.0f && anim.frames[i].duration > 0.0f) {
                    sprites[curr->val].coords_x = anim.frames[i].coords_x;
                    sprites[curr->val].coords_y = anim.frames[i].coords_y;
                    time -= anim.frames[i].duration;
                    i++;
                }
                if (anim.looped && time >= 0.0f) {
                    anim_times[id] = time;
                    sprites[curr->val].coords_x = anim.frames[0].coords_x;
                    sprites[curr->val].coords_y = anim.frames[0].coords_y;

                    // Non looping animations when they end, we leave them with
                    // anim_times == -1.0f for one frame to trigger
                    // SPRITES_anim_finished() once
                } else if (!anim.looped && time >= 0.0f) {
                    anim_playing[id] = false;
                    anim_times[id] = -1.0f;
                }
            } else if (anim_times[id] == -1.0f) {
                anim_times[id] = 0.0f;
            }

            // Render sprite
            SDL_FRect srcrect = (SDL_FRect){
                .x = SPRITESHEET_CELL_X * sprites[curr->val].coords_x,
                .y = SPRITESHEET_CELL_Y * sprites[curr->val].coords_y,
                .w = SPRITESHEET_CELL_X * sprites[curr->val].coords_w,
                .h = SPRITESHEET_CELL_Y * sprites[curr->val].coords_h,
            };
            SDL_FRect dstrect = (SDL_FRect){
                .x = sprites[curr->val].pos_x,
                .y = sprites[curr->val].pos_y,
                .w = SPRITESHEET_CELL_X * sprites[curr->val].coords_w,
                .h = SPRITESHEET_CELL_Y * sprites[curr->val].coords_h,
            };
            SDL_RenderTexture(renderer, spritesheet, &srcrect, &dstrect);
            curr = curr->next;
        }
    }
}


void SPRITES_stop(void) {
    SDL_DestroyTexture(spritesheet);
    for (int i = 0; i < Z_INDEX_TOTAL; i++) {
        clear_list(&z_index_map[i]);
    }
    stop_dyn_arrs();
}
