#include "dev.h"
#include "chunks.h"
#include "tilemap.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    chunk data = (chunk){0};
    TILEMAP_set_at(0, 0, data.w, data.h, (tile(*)[data.w][data.h])data.map);
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
