#include "dev.h"
#include "../sprites.h"

static int sprite_id = 0;

void DEV_start(void) {
    // sprite_id = SPRITES_new_sprite((sprite){
    //     .pos_x = 10,
    //     .pos_y = 30,
    //     .coords_x = 6,
    //     .coords_y = 1,
    //     .coords_w = 1,
    //     .coords_h = 1,
    //     .z_index = 0,
    // });
}

void DEV_update(void) {
    // sprite data = SPRITES_get_sprite(sprite_id);
    // data.pos_x += 1;
    // SPRITES_update_sprite(sprite_id, data);
}

void DEV_stop(void) {
}
