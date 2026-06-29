#include <SDL3/SDL.h>
#include <dev/dev.h>
#include <engine/chunks.h>
#include <engine/tilemap.h>
#include <player/player.h>

void DEV_start(void) {
    chunk chunk_data = (chunk){0};
    CHUNKS_get(0, &chunk_data);
    TILEMAP_set_at(0, 0, 32, 32, (tile(*)[32][32])chunk_data.map);
    PLAYER_create(32, 0);
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
