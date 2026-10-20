#include "tests/fluids.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/game-manager.h"
#include "engine/physics.h"
#include "engine/tilemap.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// Slowest move time of each fluid in `tests/mocks/fluids.csv`, so stepping for
// `n` of these runs every kind of move of that fluid at least `n` times.
// UPDATE IF TESTS/MOCKS/FLUIDS.CSV IS CHANGED!!!
#define WATER_MOVE_TIME 0.05
#define LAVA_MOVE_TIME 0.2
#define WATER_PARTICLE_VOL 0.1

// Splash settings in `tests/mocks/fluid-particles.csv`.
// UPDATE IF TESTS/MOCKS/FLUID-PARTICLES.CSV IS CHANGED!!!
#define SPLASH_VOLUME 2.0
#define SPLASH_BODY_MAX_SPEED 200

// Steps the simulation for `seconds` of game time. Steps one extra frame so a
// timer that would fire right at the end isn't missed by rounding.
static void step_for(double seconds) {
    Uint32 frames = SDL_ceil(seconds / GAME_MANAGER_get_current_dt()) + 1;
    for (Uint32 i = 0; i < frames; i++)
        FLUIDS_step();
}

// Sums the water on rect `(x,y,w,h)`, which must be inside the 12x12 cells at
// the origin.
static double sum_water(Uint32 x, Uint32 y, Uint32 w, Uint32 h) {
    fluid_cell cells[12][12] = {0};
    FLUIDS_get_at(0, 0, 12, 12, &cells);
    double sum = 0.0f;
    for (Uint32 i = x; i < x + w; i++)
        for (Uint32 j = y; j < y + h; j++)
            sum += cells[i][j].volume[fluid_water];
    return sum;
}

void test_fluids(void) {
    printf(YELLOW "\nTesting fluids...\n" RESET);

    tile data0[3][3] = {
        {
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){0},
            (tile){0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
    };
    TILEMAP_set_at(0, 0, 3, 3, &data0);
    FLUIDS_start();
    FLUIDS_set_at(2, 2, 2, 2, fluid_water, 1.0f);
    step_for(2 * WATER_MOVE_TIME);

    fluid_cell probe[6][6] = {0};
    FLUIDS_get_at(0, 0, 6, 6, &probe);
    expect("Loaded a cup and put a tile of water in it and stepped, checking "
           "if it remains still...",
           4,
           (bool[]){
               probe[2][2].volume[fluid_water] == 1.0f,
               probe[3][2].volume[fluid_water] == 1.0f,
               probe[2][3].volume[fluid_water] == 1.0f,
               probe[3][3].volume[fluid_water] == 1.0f,
           });

    tile data1[3][3] = {
        {
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){0},
            (tile){0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
    };
    TILEMAP_set_at(0, 2, 3, 3, &data1);
    step_for(WATER_MOVE_TIME);
    FLUIDS_get_at(0, 4, 6, 6, &probe);
    expect(
        "Made a hole in the cup and another cup underneath, stepped once and "
        "checking if it starts falling...",
        8,
        (bool[]){
            probe[2][0].volume[fluid_water] == 1.0f,
            probe[3][0].volume[fluid_water] == 1.0f,
            probe[2][1].volume[fluid_water] == 0.0f,
            probe[3][1].volume[fluid_water] == 0.0f,
            probe[2][2].volume[fluid_water] == 0.0f,
            probe[3][2].volume[fluid_water] == 0.0f,
            probe[2][3].volume[fluid_water] == 0.0f,
            probe[3][3].volume[fluid_water] == 0.0f,
        });

    step_for(10 * WATER_MOVE_TIME);
    FLUIDS_get_at(0, 4, 6, 6, &probe);
    expect("Stepped enough for all the water to fall down, checking...",
           8,
           (bool[]){
               probe[2][0].volume[fluid_water] == 0.0f,
               probe[3][0].volume[fluid_water] == 0.0f,
               probe[2][1].volume[fluid_water] == 0.0f,
               probe[3][1].volume[fluid_water] == 0.0f,
               probe[2][2].volume[fluid_water] == 1.0f,
               probe[3][2].volume[fluid_water] == 1.0f,
               probe[2][3].volume[fluid_water] == 1.0f,
               probe[3][3].volume[fluid_water] == 1.0f,
           });

    tile data2[3][3] = {
        {
            (tile){0},
            (tile){0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){0},
            (tile){0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.mat = tile_stone, .coords_x = 0, .coords_y = 0},
        },
    };
    TILEMAP_set_at(3, 0, 3, 3, &data2);
    FLUIDS_set_at(8, 2, 2, 2, fluid_water, 1.0f);
    step_for(20 * WATER_MOVE_TIME);
    FLUIDS_get_at(6, 0, 6, 6, &probe);
    expect("Created water in the middle and stepped enough for it to flow to "
           "the sides, checking...",
           8,
           (bool[]){
               SDL_fabs(probe[0][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_fabs(probe[0][3].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_fabs(probe[1][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_fabs(probe[1][3].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_fabs(probe[2][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_fabs(probe[2][3].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_fabs(probe[3][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_fabs(probe[3][3].volume[fluid_water] - 1.0f) < EPSILON,
           });

    // Mods are applied at the end of a step, so the first step only sets the
    // uneven surface and the second one is the one that has to level it.
    FLUIDS_set_at(6, 2, 1, 1, fluid_water, 0.8f);
    FLUIDS_set_at(7, 2, 1, 1, fluid_water, 0.2f);
    FLUIDS_set_at(8, 2, 1, 1, fluid_water, 0.3f);
    FLUIDS_set_at(9, 2, 1, 1, fluid_water, 0.1f);
    FLUIDS_step();
    FLUIDS_get_at(6, 0, 6, 6, &probe);
    bool uneven = SDL_fabs(probe[0][2].volume[fluid_water] - 0.8f) < EPSILON &&
                  SDL_fabs(probe[1][2].volume[fluid_water] - 0.2f) < EPSILON &&
                  SDL_fabs(probe[2][2].volume[fluid_water] - 0.3f) < EPSILON &&
                  SDL_fabs(probe[3][2].volume[fluid_water] - 0.1f) < EPSILON;
    FLUIDS_step();
    FLUIDS_get_at(6, 0, 6, 6, &probe);
    expect("Created an uneven surface on top of the water and stepped once, "
           "checking if it levelled in a single tick...",
           5,
           (bool[]){
               uneven,
               SDL_fabs(probe[0][2].volume[fluid_water] - 0.35f) < EPSILON,
               SDL_fabs(probe[1][2].volume[fluid_water] - 0.35f) < EPSILON,
               SDL_fabs(probe[2][2].volume[fluid_water] - 0.35f) < EPSILON,
               SDL_fabs(probe[3][2].volume[fluid_water] - 0.35f) < EPSILON,
           });
    FLUIDS_clear_at(6, 2, 4, 1, fluid_water);

    FLUIDS_set_at(8, 2, 2, 2, fluid_lava, 1.0f);
    step_for(20 * LAVA_MOVE_TIME);
    FLUIDS_get_at(6, 0, 6, 6, &probe);
    expect("Created lava above the water and stepped enough for it to go under "
           "the water, checking...",
           8,
           (bool[]){
               SDL_fabs(probe[0][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_fabs(probe[0][3].volume[fluid_lava] - 1.0f) < EPSILON,
               SDL_fabs(probe[1][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_fabs(probe[1][3].volume[fluid_lava] - 1.0f) < EPSILON,
               SDL_fabs(probe[2][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_fabs(probe[2][3].volume[fluid_lava] - 1.0f) < EPSILON,
               SDL_fabs(probe[3][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_fabs(probe[3][3].volume[fluid_lava] - 1.0f) < EPSILON,
           });

    // A closed square room of tiles over all the previous ones, so its fluid
    // cells 2 to 9 are the only open ones.
    tile room[6][6] = {0};
    for (int x = 0; x < 6; x++)
        for (int y = 0; y < 6; y++)
            if (x == 0 || y == 0 || x == 5 || y == 5)
                room[x][y] = (tile){.mat = tile_stone};
    TILEMAP_set_at(0, 0, 6, 6, &room);
    FLUIDS_clear_at(0, 0, 12, 12, fluid_water);
    FLUIDS_clear_at(0, 0, 12, 12, fluid_lava);
    FLUIDS_step();

    // Shoots left from the middle of cell (4,4) into the wall at column 1.
    bool added = true;
    for (int i = 0; i < 10; i++)
        if (FLUIDS_add_particle(fluid_water,
                                4.5f * FLUIDS_CELL_W,
                                4.5f * FLUIDS_CELL_H,
                                -300.0f,
                                0.0f) != 0)
            added = false;
    for (int i = 0; i < 5; i++)
        FLUIDS_step();
    expect(
        "Cleared everything, made a square room and shot water particles "
        "into a wall, checking if they became water next to it...",
        3,
        (bool[]){
            added,
            SDL_fabs(sum_water(2, 2, 1, 8) - 10 * WATER_PARTICLE_VOL) < EPSILON,
            SDL_fabs(sum_water(0, 0, 12, 12) - 10 * WATER_PARTICLE_VOL) <
                EPSILON,
        });

    // Shoots down from above the middle of the room at the puddle.
    step_for(20 * WATER_MOVE_TIME);
    added = true;
    for (int i = 0; i < 5; i++)
        if (FLUIDS_add_particle(fluid_water,
                                6.0f * FLUIDS_CELL_W,
                                3.5f * FLUIDS_CELL_H,
                                0.0f,
                                300.0f) != 0)
            added = false;
    for (int i = 0; i < 10; i++)
        FLUIDS_step();
    expect(
        "Let the water settle into a puddle and shot water particles down "
        "at it, checking if they landed in it...",
        3,
        (bool[]){
            added,
            SDL_fabs(sum_water(2, 9, 8, 1) - 15 * WATER_PARTICLE_VOL) < EPSILON,
            SDL_fabs(sum_water(0, 0, 12, 12) - 15 * WATER_PARTICLE_VOL) <
                EPSILON,
        });

    // A pool two cells deep at the bottom of the room. Splashes are applied
    // and their particles thrown up in the same step, so right after it the
    // water missing from the grid is what was splashed, up to a particle per
    // column short from what doesn't make a whole one being put back.
    FLUIDS_clear_at(0, 0, 12, 12, fluid_water);
    FLUIDS_set_at(2, 8, 8, 2, fluid_water, 1.0f);
    FLUIDS_step();
    double pool = sum_water(0, 0, 12, 12);
    FLUIDS_splash_at(4, 8, 2, 0.0f);
    FLUIDS_step();
    double still = sum_water(0, 0, 12, 12);
    FLUIDS_splash_at(4, 8, 2, SPLASH_BODY_MAX_SPEED / 2.0f);
    FLUIDS_step();
    double half = pool - sum_water(0, 0, 12, 12);
    step_for(40 * WATER_MOVE_TIME);
    double settled = sum_water(0, 0, 12, 12);
    FLUIDS_splash_at(4, 8, 2, SPLASH_BODY_MAX_SPEED * 2.0f);
    FLUIDS_step();
    double full = pool - sum_water(0, 0, 12, 12);
    expect("Made a pool and splashed 2 of its columns still, at half and at "
           "twice the max speed, checking how much water was thrown up...",
           6,
           (bool[]){
               SDL_fabs(pool - 16.0f) < EPSILON,
               SDL_fabs(still - pool) < EPSILON,
               half > SPLASH_VOLUME - 2 * WATER_PARTICLE_VOL - EPSILON,
               half < SPLASH_VOLUME + EPSILON,
               SDL_fabs(settled - pool) < EPSILON,
               full > 2 * SPLASH_VOLUME - 2 * WATER_PARTICLE_VOL - EPSILON &&
                   full < 2 * SPLASH_VOLUME + EPSILON,
           });

    // Drops a body one fluid cell wide into the pool at the max speed. It
    // takes 2 updates to reach the surface and a third one moves it within
    // the water, which mustn't splash again.
    step_for(40 * WATER_MOVE_TIME);
    PHYSICS_start();
    size_t id = 0;
    PHYSICS_new_body(
        (body){
            .type = physics_actor,
            .x = 4.0f * FLUIDS_CELL_W,
            .y = 5.0f * FLUIDS_CELL_H,
            .w = FLUIDS_CELL_W,
            .h = 2 * FLUIDS_CELL_H,
            .vel_y = SPLASH_BODY_MAX_SPEED,
        },
        &id);
    PHYSICS_update();
    FLUIDS_step();
    double before = sum_water(0, 0, 12, 12);
    PHYSICS_update();
    PHYSICS_update();
    FLUIDS_step();
    double splashed = before - sum_water(0, 0, 12, 12);
    PHYSICS_update();
    FLUIDS_step();
    double after = before - sum_water(0, 0, 12, 12);
    PHYSICS_stop();
    expect("Dropped a body into the pool, checking if it splashed its column "
           "once...",
           3,
           (bool[]){
               SDL_fabs(before - pool) < EPSILON,
               splashed > SPLASH_VOLUME - WATER_PARTICLE_VOL - EPSILON &&
                   splashed < SPLASH_VOLUME + EPSILON,
               SDL_fabs(after - splashed) < EPSILON,
           });

    FLUIDS_stop();
}
