#include "dev.h"
#include "../sprites.h"

static int sprite_id = 0;

void DEV_start(void) {
    SPRITES_new_sprite(
        (sprite){
            .pos_x = 10,
            .pos_y = 30,
            .coords_x = 5,
            .coords_y = 0,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 0,
        },
        &sprite_id);
    SPRITES_play_anim(
        sprite_id,
        (animation){
            .looped = false,
            .frames =
                {
                    (keyframe){.coords_x = 5, .coords_y = 0, .duration = 2.0f},
                    (keyframe){.coords_x = 5, .coords_y = 1, .duration = 2.0f},
                    (keyframe){.coords_x = 4, .coords_y = 1, .duration = 2.0f},
                },
        });
}

void DEV_update(void) {
    bool finished = false;
    SPRITES_anim_finished(sprite_id, &finished);
    if (finished) {
        SDL_Log("AAAAAAAAAAAA");
    }
}

void DEV_stop(void) {
}
