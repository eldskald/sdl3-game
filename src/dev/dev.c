#include "dev/dev.h"
#include "engine/chunks.h"
#include "player/player-core.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    CHUNKS_load_at(0, 0, 0);
    // PLAYER_create(56.0f, 72.0f);
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
