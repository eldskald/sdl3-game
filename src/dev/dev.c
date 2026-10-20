#include "dev/dev.h"
#include "defs.h"
#include "engine/chunks.h"
#include "engine/sprites.h"
#include "player/player-core.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    CHUNKS_load_at(3, 0, 0);
    PLAYER_create(56.0f, 72.0f);

    // // Checkerboard background, to see the fluids refracting it.
    // for (Uint32 x = 0; x < WINDOW_SIZE_X / SPRITESHEET_CELL_X; x++) {
    //     for (Uint32 y = 0; y <= WINDOW_SIZE_Y / SPRITESHEET_CELL_Y; y++) {
    //         if ((x + y) % 2) continue;
    //         size_t id = 0;
    //         SPRITES_new_sprite(
    //             (sprite){
    //                 .pos_x = x * SPRITESHEET_CELL_X,
    //                 .pos_y = y * SPRITESHEET_CELL_Y,
    //                 .coords_x = 11,
    //                 .coords_y = 1,
    //                 .coords_w = 1,
    //                 .coords_h = 1,
    //                 .z_index = -100,
    //             },
    //             &id);
    //     }
    // }
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
