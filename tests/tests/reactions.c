#include "tests/reactions.h"
#include "engine/fluids.h"
#include "engine/reactions.h"
#include "engine/tilemap.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stdio.h>

void test_reactions(void) {
    printf(YELLOW "\nTesting reactions...\n" RESET);

    tile data0[3][3] = {
        {
            (tile){.type = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.type = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.type = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){0},
            (tile){0},
            (tile){.type = tile_stone, .coords_x = 0, .coords_y = 0},
        },
        {
            (tile){.type = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.type = tile_stone, .coords_x = 0, .coords_y = 0},
            (tile){.type = tile_stone, .coords_x = 0, .coords_y = 0},
        },
    };
    TILEMAP_set_at(0, 0, 3, 3, &data0);
    FLUIDS_start();
    REACTIONS_start();

    // Fluids are set at the end of the step, after they move, so they're
    // still side by side when the reactions are checked.
    FLUIDS_set_at(2, 2, 1, 1, fluid_lava, 1.0f);
    FLUIDS_set_at(3, 2, 1, 1, fluid_water, 1.0f);
    FLUIDS_step();
    REACTIONS_do_fluid_fluid_to_tile_reactions();

    // Fluids are cleared on the next step, since this isn't the fluids thread.
    FLUIDS_step();
    tile t = (tile){0};
    TILEMAP_get_at(1, 1, &t);
    fluid_cell probe[2][2] = {0};
    FLUIDS_get_at(2, 2, 2, 2, &probe);
    expect("Put lava and water side by side in a cup and reacted, checking if "
           "they became a stone tile...",
           9,
           (bool[]){
               t.type == tile_stone,
               probe[0][0].volume[fluid_lava] == 0.0f,
               probe[1][0].volume[fluid_lava] == 0.0f,
               probe[0][1].volume[fluid_lava] == 0.0f,
               probe[1][1].volume[fluid_lava] == 0.0f,
               probe[0][0].volume[fluid_water] == 0.0f,
               probe[1][0].volume[fluid_water] == 0.0f,
               probe[0][1].volume[fluid_water] == 0.0f,
               probe[1][1].volume[fluid_water] == 0.0f,
           });

    TILEMAP_clear();
    REACTIONS_stop();
    FLUIDS_stop();
}
