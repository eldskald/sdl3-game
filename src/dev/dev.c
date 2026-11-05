#include "dev/dev.h"
#include "defs.h"
#include "engine/chunks.h"
#include "engine/game-manager.h"
#include "engine/particles.h"
#include "engine/text.h"
#include "player/player-core.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    CHUNKS_load_at(3, 0, 0);
    PLAYER_create(80.0f, 72.0f);
}

static double firework_timer = 0.0;

void DEV_update(void) {
    // Launches a firework every half a second, which bursts into sparks.
    firework_timer += GAME_MANAGER_get_current_dt();
    if (firework_timer >= 0.5) {
        firework_timer -= 0.5;
        PARTICLES_emit(
            particle_firework, 80.0f, 72.0f, 90.0f, -20.0f, 0.0f, 10.0f, 1);
    }
}

// Called by the renderer on the main thread, like the other `..._draw()`.
void DEV_draw(void) {
    TEXT_draw(8.0f,
              160.0f,
              "Testando oi som, ação coé",
              font_main,
              1,
              (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f},
              30);
    TEXT_draw(8.0f,
              140.0f,
              "Testando oi som, ação coé",
              font_unifont,
              1,
              (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f},
              30);
}

void DEV_stop(void) {
}
