#include "dev/dev.h"
#include "engine/audio.h"
#include "engine/bgs.h"
#include "engine/chunks.h"
#include "engine/game-manager.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    // CHUNKS_load_at(4, 0, 0);
    // PLAYER_create(168.0f, 96.0f);

    app_args args = GAME_MANAGER_get_args();
    if (args.muted) AUDIO_set_master_volume(0.0f);

    if (args.dev_level) {
        char* level = SDL_LoadFile(args.dev_level, NULL);
        if (level) {
            CHUNK_load_from_file(level);
            SDL_free(level);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to load dev level %s: %s",
                        args.dev_level,
                        SDL_GetError());
        }
    }

    BGS_play_track(bgs_track_cave);
}

void DEV_update(void) {
}

void DEV_draw(void) {
}

void DEV_stop(void) {
}
