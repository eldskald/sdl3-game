#include "tests/fluids.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stdio.h>

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
    FLUIDS_step();
    FLUIDS_step();

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
    FLUIDS_step();
    FLUIDS_get_at(0, 4, 6, 6, &probe);
    expect(
        "Made a hole in the cup and another cup underneath, stepped once and "
        "checking if it starts falling...",
        8,
        (bool[]){
            probe[2][0].volume[fluid_water] == 0.5f,
            probe[3][0].volume[fluid_water] == 0.5f,
            probe[2][1].volume[fluid_water] == 0.0f,
            probe[3][1].volume[fluid_water] == 0.0f,
            probe[2][2].volume[fluid_water] == 0.0f,
            probe[3][2].volume[fluid_water] == 0.0f,
            probe[2][3].volume[fluid_water] == 0.0f,
            probe[3][3].volume[fluid_water] == 0.0f,
        });

    for (int i = 0; i < 10; i++)
        FLUIDS_step();
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
    for (int i = 0; i < 20; i++)
        FLUIDS_step();
    FLUIDS_get_at(6, 0, 6, 6, &probe);
    expect("Created water in the middle and stepped enough for it to flow to "
           "the sides, checking...",
           8,
           (bool[]){
               SDL_abs(probe[0][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_abs(probe[0][3].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_abs(probe[1][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_abs(probe[1][3].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_abs(probe[2][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_abs(probe[2][3].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_abs(probe[3][2].volume[fluid_water] - 0.0f) < EPSILON,
               SDL_abs(probe[3][3].volume[fluid_water] - 1.0f) < EPSILON,
           });

    FLUIDS_set_at(8, 2, 2, 2, fluid_lava, 1.0f);
    for (int i = 0; i < 20; i++)
        FLUIDS_step();
    FLUIDS_get_at(6, 0, 6, 6, &probe);
    expect("Created lava above the water and stepped enough for it to go under "
           "the water, checking...",
           8,
           (bool[]){
               SDL_abs(probe[0][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_abs(probe[0][3].volume[fluid_lava] - 1.0f) < EPSILON,
               SDL_abs(probe[1][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_abs(probe[1][3].volume[fluid_lava] - 1.0f) < EPSILON,
               SDL_abs(probe[2][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_abs(probe[2][3].volume[fluid_lava] - 1.0f) < EPSILON,
               SDL_abs(probe[3][2].volume[fluid_water] - 1.0f) < EPSILON,
               SDL_abs(probe[3][3].volume[fluid_lava] - 1.0f) < EPSILON,
           });

    FLUIDS_stop();
}
