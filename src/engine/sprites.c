#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <data-structs/list.h>
#include <defs.h>
#include <engine/game-manager.h>
#include <engine/sprites.h>

#ifdef TEST
#include <stubs.h>
#endif

// This is how we're doing it: we have this huge sprites array of sprite_data to
// represent all current sprites rendering. It is just a big block of memory.
// When SPRITES_new_sprite() is called, we iterate through it to find the
// smallest unoccupied index and place the sprite there, where it shall remain
// so that its sprite_id always points to it in the array. If the array gets
// full, we double its size like a dynamic array but hopefully it shouldn't need
// to.
typedef struct {
    sprite sprite;
    animation anim;
    double anim_time;
    bool anim_playing;
} sprite_data;

static sprite_data* sprites = NULL;
static size_t sprites_size = 0;

static void double_dynarr(void) {
    sprite_data* new_sprites =
        SDL_calloc(sprites_size << 1, sizeof(sprite_data));
    SDL_memcpy(new_sprites, sprites, sprites_size * sizeof(sprite_data));
    SDL_free(sprites);
    sprites_size = sprites_size << 1;
    sprites = new_sprites;
}



// The z_index map is our data structure to order sprite ids in order of z_index
// that we should iterate through to draw them in z_index order. Each z_index is
// mapped to z_index + Z_INDEX_TOTAL / 2, so they all fit in the array. We need
// to update the map whenever a new sprite is added, removed or updated.
static list z_index_map[Z_INDEX_TOTAL] = {{NULL}};

static void add_to_z_index_map(size_t sprite_id) {
    int map_index = sprites[sprite_id].sprite.z_index + (Z_INDEX_TOTAL >> 1);
    push_to_list((void*)sprite_id, &z_index_map[map_index]);
}

static void remove_from_z_index_map(size_t sprite_id) {
    int map_index = sprites[sprite_id].sprite.z_index + (Z_INDEX_TOTAL >> 1);
    remove_from_list((void*)sprite_id, &z_index_map[map_index]);
}



int SPRITES_new_sprite(sprite data, size_t* sprite_id) {
    if (data.coords_w == 0 || data.coords_h == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to create sprite with invalid data: { coords_w = "
                    "%d, coords_h = %d }",
                    data.coords_w,
                    data.coords_h);
        return 2;
    }
    for (size_t i = 0; i < sprites_size; i++) {
        if (sprites[i].sprite.coords_w == 0) {
            sprites[i].sprite = data;
            add_to_z_index_map(i);
            *sprite_id = i;
            return 0;
        }
    }
    double_dynarr();
    size_t index = sprites_size / 2 + 1;
    sprites[index].sprite = data;
    add_to_z_index_map(index);
    *sprite_id = index;
    return 0;
}


int SPRITES_del_sprite(size_t sprite_id) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete sprite_id=%lu but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to delete inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }
    remove_from_z_index_map(sprite_id);
    sprites[sprite_id].sprite = (sprite){0};
    return 0;
}


int SPRITES_get_sprite(size_t sprite_id, sprite* data) {
    if (sprite_id >= sprites_size) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to get sprite_id=%lu but this is outside the buffer range",
            sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to get inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    *data = sprites[sprite_id].sprite;
    return 0;
}


int SPRITES_update_sprite(size_t sprite_id, sprite data) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to update sprite_id=%lu but this is outside the "
                    "buffer range",
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
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to update inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    if (data.z_index != sprites[sprite_id].sprite.z_index) {
        remove_from_z_index_map(sprite_id);
        sprites[sprite_id].sprite = data;
        add_to_z_index_map(sprite_id);
    } else {
        sprites[sprite_id].sprite = data;
    }
    return 0;
}



void SPRITES_start(void) {
    sprites = SDL_calloc(STARTING_SPRITES_DYNARR_CAP, sizeof(sprite_data));
    sprites_size = STARTING_SPRITES_DYNARR_CAP;
}



int SPRITES_play_anim(size_t sprite_id, animation anim) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate sprite_id=%lu but this is outside the "
                    "buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to animate inexistent sprite at sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    sprites[sprite_id].anim = anim;
    sprites[sprite_id].anim_playing = true;
    sprites[sprite_id].anim_time = 0.0f;
    return 0;
}

int SPRITES_stop_anim(size_t sprite_id) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in sprite_id=%lu but this is outside the "
            "buffer range",
            sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Tried to stop animation in inexistent sprite at sprite_id=%lu",
            sprite_id);
        return 1;
    }

    sprites[sprite_id].anim_playing = false;
    sprites[sprite_id].anim_time = 0.0f;
    return 0;
}

int SPRITES_anim_finished(size_t sprite_id, bool* finished) {
    if (sprite_id >= sprites_size || sprite_id < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check animation end in sprite_id=%lu but this is "
                    "outside the buffer range",
                    sprite_id);
        return 3;
    }
    if (sprites[sprite_id].sprite.coords_w == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Tried to check animation end in inexistent sprite at "
                    "sprite_id=%lu",
                    sprite_id);
        return 1;
    }

    *finished = sprites[sprite_id].anim_time == -1.0f;
    return 0;
}



void SPRITES_update(SDL_Renderer* renderer, SDL_Texture* spritesheet) {
    for (int i = 0; i < Z_INDEX_TOTAL; i++) {
        node* curr = z_index_map[i].head;
        while (curr) {
            size_t id = (size_t)curr->val;

            // Update animation status
            if (sprites[id].anim_playing) {
                sprites[id].anim_time += GAME_MANAGER_get_current_dt();
                animation anim = sprites[id].anim;
                int i = 0;
                double time = sprites[id].anim_time;
                while (time >= 0.0f && anim.frames[i].duration > 0.0f) {
                    sprites[(size_t)curr->val].sprite.coords_x =
                        anim.frames[i].coords_x;
                    sprites[(size_t)curr->val].sprite.coords_y =
                        anim.frames[i].coords_y;
                    time -= anim.frames[i].duration;
                    i++;
                }
                if (anim.looped && time >= 0.0f) {
                    sprites[id].anim_time = time;
                    sprites[(size_t)curr->val].sprite.coords_x =
                        anim.frames[0].coords_x;
                    sprites[(size_t)curr->val].sprite.coords_y =
                        anim.frames[0].coords_y;

                    // Non looping animations when they end, we leave them with
                    // anim_time == -1.0f for one frame to trigger
                    // SPRITES_anim_finished() once
                } else if (!anim.looped && time >= 0.0f) {
                    sprites[id].anim_playing = false;
                    sprites[id].anim_time = -1.0f;
                }
            } else if (sprites[id].anim_time == -1.0f) {
                sprites[id].anim_time = 0.0f;
            }

            // Render sprite
            SDL_FRect srcrect = (SDL_FRect){
                .x = SPRITESHEET_CELL_X *
                     sprites[(size_t)curr->val].sprite.coords_x,
                .y = SPRITESHEET_CELL_Y *
                     sprites[(size_t)curr->val].sprite.coords_y,
                .w = SPRITESHEET_CELL_X *
                     sprites[(size_t)curr->val].sprite.coords_w,
                .h = SPRITESHEET_CELL_Y *
                     sprites[(size_t)curr->val].sprite.coords_h,
            };
            SDL_FRect dstrect = (SDL_FRect){
                .x = sprites[(size_t)curr->val].sprite.pos_x,
                .y = sprites[(size_t)curr->val].sprite.pos_y,
                .w = SPRITESHEET_CELL_X *
                     sprites[(size_t)curr->val].sprite.coords_w,
                .h = SPRITESHEET_CELL_Y *
                     sprites[(size_t)curr->val].sprite.coords_h,
            };
#ifndef TEST
            SDL_FPoint center = (SDL_FPoint){.x = 0.0f, .y = 0.0f};
            SDL_RenderTextureRotated(
                renderer,
                spritesheet,
                &srcrect,
                &dstrect,
                0.0f,
                &center,
                sprites[(size_t)curr->val].sprite.flip_mode);
#else
            stubbed_SDL_RenderTexture(
                &srcrect,
                &dstrect,
                sprites[(size_t)curr->val].sprite.flip_mode);
#endif
            curr = curr->next;
        }
    }
}


void SPRITES_stop(void) {
    for (int i = 0; i < Z_INDEX_TOTAL; i++) {
        clear_list(&z_index_map[i]);
    }
    SDL_free(sprites);
    sprites = NULL;
    sprites_size = 0;
}
