#include "tests/reactions.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/reactions.h"
#include "engine/tilemap.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// Sums the fluid of type `type` on the 12x12 cells at the origin, so fluids
// that moved around after a step are still counted.
static double sum_fluid(fluid_type type) {
    fluid_cell cells[12][12] = {0};
    FLUIDS_get_at(0, 0, 12, 12, &cells);
    double sum = 0.0f;
    for (Uint32 i = 0; i < 12; i++)
        for (Uint32 j = 0; j < 12; j++)
            sum += cells[i][j].volume[type];
    return sum;
}

void test_reactions(void) {
    printf(YELLOW "\nTesting reactions...\n" RESET);

    FLUIDS_start();
    REACTIONS_start();

    FLUIDS_set_at(2, 2, 2, 2, fluid_water, 2.0f);
    FLUIDS_set_at(2, 2, 2, 2, fluid_lava, 2.0f);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    // Fluids are cleared on the next step, since this isn't the fluids thread.
    FLUIDS_update();
    tile t = (tile){0};
    TILEMAP_get_at(1, 1, &t);
    fluid_cell probe0[2][2] = {0};
    FLUIDS_get_at(2, 2, 2, 2, &probe0);
    expect("Filled a tile with water and lava and reacted, checking if it "
           "became a metal tile...",
           11,
           (bool[]){
               t.type == tile_metal,
               t.coords_x == 12,
               t.coords_y == 12,
               probe0[0][0].volume[fluid_water] == 0.0f,
               probe0[1][0].volume[fluid_water] == 0.0f,
               probe0[0][1].volume[fluid_water] == 0.0f,
               probe0[1][1].volume[fluid_water] == 0.0f,
               probe0[0][0].volume[fluid_lava] == 0.0f,
               probe0[1][0].volume[fluid_lava] == 0.0f,
               probe0[0][1].volume[fluid_lava] == 0.0f,
               probe0[1][1].volume[fluid_lava] == 0.0f,
           });

    clear_tiles_and_fluids();

    FLUIDS_set_at(2, 2, 2, 2, fluid_lava, 1.0f);
    FLUIDS_set_at(2, 1, 2, 1, fluid_water, 1.0f);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    FLUIDS_update();
    tile above = (tile){0};
    TILEMAP_get_at(1, 1, &t);
    TILEMAP_get_at(1, 0, &above);
    fluid_cell probe1[2][3] = {0};
    FLUIDS_get_at(2, 1, 2, 3, &probe1);
    expect("Filled a tile with lava, put water right on top of it and "
           "reacted, checking if it became a metal tile...",
           10,
           (bool[]){
               t.type == tile_metal,
               t.coords_x == 12,
               t.coords_y == 12,
               above.type == 0,
               probe1[0][0].volume[fluid_water] == 0.0f,
               probe1[1][0].volume[fluid_water] == 0.0f,
               probe1[0][1].volume[fluid_lava] == 0.0f,
               probe1[1][1].volume[fluid_lava] == 0.0f,
               probe1[0][2].volume[fluid_lava] == 0.0f,
               probe1[1][2].volume[fluid_lava] == 0.0f,
           });

    clear_tiles_and_fluids();

    FLUIDS_set_at(
        2, 2, 1, 1, fluid_water, REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD);
    FLUIDS_set_at(
        3, 2, 1, 1, fluid_lava, REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    FLUIDS_update();
    TILEMAP_get_at(1, 1, &t);
    fluid_cell probe2[2][1] = {0};
    FLUIDS_get_at(2, 2, 2, 1, &probe2);
    expect("Put threshold amounts of water and lava on different cells of a "
           "tile and reacted, checking if it became a metal tile...",
           5,
           (bool[]){
               t.type == tile_metal,
               t.coords_x == 12,
               t.coords_y == 12,
               probe2[0][0].volume[fluid_water] == 0.0f,
               probe2[1][0].volume[fluid_lava] == 0.0f,
           });

    clear_tiles_and_fluids();

    FLUIDS_set_at(
        2, 2, 1, 1, fluid_water, REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD);
    FLUIDS_set_at(
        2, 2, 1, 1, fluid_lava, REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    FLUIDS_update();
    TILEMAP_get_at(1, 1, &t);
    fluid_cell probe3[1][1] = {0};
    FLUIDS_get_at(2, 2, 1, 1, &probe3);
    expect("Put threshold amounts of water and lava on the same cell of a tile "
           "and reacted, checking if it became a metal tile...",
           5,
           (bool[]){
               t.type == tile_metal,
               t.coords_x == 12,
               t.coords_y == 12,
               probe3[0][0].volume[fluid_water] == 0.0f,
               probe3[0][0].volume[fluid_lava] == 0.0f,
           });

    clear_tiles_and_fluids();

    FLUIDS_set_at(
        2, 2, 1, 1, fluid_lava, REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD);
    FLUIDS_set_at(
        2, 1, 1, 1, fluid_water, REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    FLUIDS_update();
    TILEMAP_get_at(1, 1, &t);
    TILEMAP_get_at(1, 0, &above);
    fluid_cell probe4[1][2] = {0};
    FLUIDS_get_at(2, 1, 1, 2, &probe4);
    expect("Put threshold amounts of lava on a tile's top cell and water on "
           "the cell above it and reacted, checking if it became a metal "
           "tile...",
           6,
           (bool[]){
               t.type == tile_metal,
               t.coords_x == 12,
               t.coords_y == 12,
               above.type == 0,
               probe4[0][0].volume[fluid_water] == 0.0f,
               probe4[0][1].volume[fluid_lava] == 0.0f,
           });

    clear_tiles_and_fluids();

    double below = REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD - 0.01f;
    FLUIDS_set_at(2, 2, 2, 2, fluid_lava, 1.0f);
    FLUIDS_set_at(2, 2, 2, 2, fluid_water, below);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    FLUIDS_update();
    TILEMAP_get_at(1, 1, &t);
    expect("Filled a tile with lava and less than the threshold of water and "
           "reacted, checking if it didn't react...",
           3,
           (bool[]){
               t.type == 0,
               SDL_fabs(sum_fluid(fluid_lava) - 4.0f) < EPSILON,
               SDL_fabs(sum_fluid(fluid_water) - 4.0f * below) < EPSILON,
           });

    clear_tiles_and_fluids();

    FLUIDS_set_at(2, 2, 2, 2, fluid_lava, 1.0f);
    FLUIDS_set_at(2, 0, 2, 1, fluid_water, 1.0f);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    FLUIDS_update();
    TILEMAP_get_at(1, 1, &t);
    TILEMAP_get_at(1, 0, &above);
    expect("Filled a tile with lava, put water on the top cells of the tile "
           "above and reacted, checking if it didn't react...",
           4,
           (bool[]){
               t.type == 0,
               above.type == 0,
               SDL_fabs(sum_fluid(fluid_lava) - 4.0f) < EPSILON,
               SDL_fabs(sum_fluid(fluid_water) - 2.0f) < EPSILON,
           });

    clear_tiles_and_fluids();

    FLUIDS_set_at(2, 3, 2, 1, fluid_lava, 1.0f);
    FLUIDS_set_at(2, 1, 2, 1, fluid_water, 1.0f);
    FLUIDS_update();
    REACTIONS_do_fluid_fluid_into_tile_reactions();

    FLUIDS_update();
    TILEMAP_get_at(1, 1, &t);
    TILEMAP_get_at(1, 0, &above);
    expect("Put lava on a tile's bottom cells, water on the bottom cells of "
           "the tile above and reacted, checking if it didn't react...",
           4,
           (bool[]){
               t.type == 0,
               above.type == 0,
               SDL_fabs(sum_fluid(fluid_lava) - 2.0f) < EPSILON,
               SDL_fabs(sum_fluid(fluid_water) - 2.0f) < EPSILON,
           });

    clear_tiles_and_fluids();
    REACTIONS_stop();
    FLUIDS_stop();
}
