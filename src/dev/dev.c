#include "dev/dev.h"
#include "defs.h"
#include "engine/chunks.h"
#include "engine/sprites.h"
#include "player/player-core.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    CHUNKS_load_at(3, 0, 0);
    PLAYER_create(80.0f, 72.0f);
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
