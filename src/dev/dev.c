#include "dev.h"
#include "../tilemap.h"

void DEV_start(void) {
    tile data[7][7] = {
        {(tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0}},
        {(tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0}},
        {(tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0}},
        {(tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0}},
        {(tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0}},
        {(tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0}},
        {(tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){null, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0},
         (tile){dirt, 0, 0}},
    };
    TILEMAP_set_at(1, 1, 7, 7, &data);
}

void DEV_update(void) {
}

void DEV_stop(void) {
    TILEMAP_clear();
}
