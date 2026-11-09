#include "dev/dev.h"
#include "defs.h"
#include "engine/bgs.h"
#include "engine/chunks.h"
#include "engine/game-manager.h"
#include "engine/particles.h"
#include "engine/text.h"
#include "player/player-core.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    CHUNKS_load_at(0, 0, 0);
    // PLAYER_create(168.0f, 96.0f);
    // PLAYER_create(80.0f, 72.0f);

    BGS_play_track(bgs_track_cave);
}

void DEV_update(void) {
}

void DEV_draw(void) {
    // TEXT_draw(8.0f,
    //           160.0f,
    //           "Testando oi som, ação coé",
    //           font_main,
    //           1,
    //           (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f},
    //           30);
    // TEXT_draw(8.0f,
    //           140.0f,
    //           "Testando oi som, ação coé",
    //           font_unifont,
    //           1,
    //           (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f},
    //           30);
}

void DEV_stop(void) {
}
