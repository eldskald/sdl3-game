#pragma once
#include <SDL3/SDL.h>

typedef struct {
    bool created;
    size_t body_id;
    Sint8 facing;
    bool jumping;
} player_state;

void PLAYER_start(void);

void PLAYER_create(double at_x, double at_y);

void PLAYER_destroy(void);

void PLAYER_update(void);
