#include "dev/dev.h"
#include "engine/chunks.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    CHUNKS_load_at(1, 0, 0);
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
