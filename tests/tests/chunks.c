#include "engine/chunks.h"
#include "data-structs/dynarr.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "tests.h"
#include "tests/chunks.h"
#include <SDL3/SDL.h>
#include <stdio.h>

static bool is_empty_at(Uint16 x, Uint16 y) {
    tile t = (tile){0};
    TILEMAP_get_at(x, y, &t);
    return t.type == 0;
}

void test_chunks(void) {
    printf(YELLOW "\nTesting chunks...\n" RESET);

    CHUNKS_start();

    chunk data = (chunk){0};
    CHUNKS_get(0, &data);
    tile(*map)[data.w][data.h] = (tile(*)[data.w][data.h])data.map;

    bool checks0[] = {
        data.w == 8,
        data.h == 8,
        (*map)[0][0].type == tile_dirt,
        (*map)[1][0].type == tile_dirt,
        (*map)[2][0].type == tile_dirt,
        (*map)[3][0].type == tile_dirt,
        (*map)[0][0].coords_x == 0,
        (*map)[1][0].coords_x == 1,
        (*map)[2][0].coords_x == 2,
        (*map)[3][0].coords_x == 3,
        (*map)[0][0].coords_y == 1,
        (*map)[1][0].coords_y == 1,
        (*map)[2][0].coords_y == 1,
        (*map)[3][0].coords_y == 1,
        (*map)[0][0].platform == false,
        (*map)[1][0].platform == false,
        (*map)[2][0].platform == false,
        (*map)[3][0].platform == false,
        (*map)[4][0].type == 0,
        (*map)[5][0].type == 0,
        (*map)[6][0].type == 0,
        (*map)[7][0].type == 0,
        (*map)[0][1].type == 0,
        (*map)[1][1].type == 0,
        (*map)[2][1].type == 0,
        (*map)[3][1].type == 0,
        (*map)[4][1].type == 0,
        (*map)[5][1].type == 0,
        (*map)[6][1].type == 0,
        (*map)[7][1].type == 0,
        (*map)[0][2].type == tile_stone,
        (*map)[1][2].type == tile_stone,
        (*map)[2][2].type == tile_stone,
        (*map)[3][2].type == tile_stone,
        (*map)[0][2].coords_x == 4,
        (*map)[1][2].coords_x == 5,
        (*map)[2][2].coords_x == 6,
        (*map)[3][2].coords_x == 7,
        (*map)[0][2].coords_y == 6,
        (*map)[1][2].coords_y == 6,
        (*map)[2][2].coords_y == 6,
        (*map)[3][2].coords_y == 6,
        (*map)[0][2].platform == false,
        (*map)[1][2].platform == false,
        (*map)[2][2].platform == false,
        (*map)[3][2].platform == false,
        (*map)[4][2].type == 0,
        (*map)[5][2].type == 0,
        (*map)[6][2].type == 0,
        (*map)[7][2].type == 0,
        (*map)[0][3].type == 0,
        (*map)[1][3].type == 0,
        (*map)[2][3].type == 0,
        (*map)[3][3].type == 0,
        (*map)[4][3].type == 0,
        (*map)[5][3].type == 0,
        (*map)[6][3].type == 0,
        (*map)[7][3].type == 0,
        (*map)[0][4].type == tile_wood,
        (*map)[1][4].type == tile_wood,
        (*map)[2][4].type == tile_wood,
        (*map)[3][4].type == tile_wood,
        (*map)[0][4].coords_x == 8,
        (*map)[1][4].coords_x == 9,
        (*map)[2][4].coords_x == 10,
        (*map)[3][4].coords_x == 11,
        (*map)[0][4].coords_y == 6,
        (*map)[1][4].coords_y == 6,
        (*map)[2][4].coords_y == 6,
        (*map)[3][4].coords_y == 6,
        (*map)[0][4].platform == true,
        (*map)[1][4].platform == true,
        (*map)[2][4].platform == false,
        (*map)[3][4].platform == false,
        (*map)[4][4].type == 0,
        (*map)[5][4].type == 0,
        (*map)[6][4].type == 0,
        (*map)[7][4].type == 0,
        (*map)[0][5].type == 0,
        (*map)[1][5].type == 0,
        (*map)[2][5].type == 0,
        (*map)[3][5].type == 0,
        (*map)[4][5].type == 0,
        (*map)[5][5].type == 0,
        (*map)[6][5].type == 0,
        (*map)[7][5].type == 0,
        (*map)[0][6].type == tile_metal,
        (*map)[1][6].type == tile_metal,
        (*map)[2][6].type == tile_metal,
        (*map)[3][6].type == tile_metal,
        (*map)[4][6].type == 0,
        (*map)[5][6].type == 0,
        (*map)[6][6].type == 0,
        (*map)[7][6].type == 0,
        (*map)[0][6].coords_x == 12,
        (*map)[1][6].coords_x == 13,
        (*map)[2][6].coords_x == 14,
        (*map)[3][6].coords_x == 15,
        (*map)[0][6].coords_y == 0,
        (*map)[1][6].coords_y == 0,
        (*map)[2][6].coords_y == 0,
        (*map)[3][6].coords_y == 0,
        (*map)[0][6].platform == false,
        (*map)[1][6].platform == false,
        (*map)[2][6].platform == false,
        (*map)[3][6].platform == false,
        (*map)[0][7].type == tile_stone,
        (*map)[1][7].type == tile_stone,
        (*map)[2][7].type == tile_stone,
        (*map)[3][7].type == tile_stone,
        (*map)[0][7].coords_x == 4,
        (*map)[1][7].coords_x == 5,
        (*map)[2][7].coords_x == 6,
        (*map)[3][7].coords_x == 7,
        (*map)[0][7].coords_y == 12,
        (*map)[1][7].coords_y == 12,
        (*map)[2][7].coords_y == 12,
        (*map)[3][7].coords_y == 12,
        (*map)[0][7].platform == true,
        (*map)[1][7].platform == true,
        (*map)[2][7].platform == true,
        (*map)[3][7].platform == true,
        (*map)[4][7].type == 0,
        (*map)[5][7].type == 0,
        (*map)[6][7].type == 0,
        (*map)[7][7].type == 0,
        data.fluids.len == 1,
        ((fluid_pool*)data.fluids.at[0])->type == fluid_water,
        ((fluid_pool*)data.fluids.at[0])->x == 10,
        ((fluid_pool*)data.fluids.at[0])->y == 2,
        ((fluid_pool*)data.fluids.at[0])->w == 4,
        ((fluid_pool*)data.fluids.at[0])->h == 4,
    };

    expect("Initiated mocked chunk, checking contents...",
           sizeof(checks0),
           checks0);

    FLUIDS_start();
    CHUNKS_load_at(0, 0, 0);
    FLUIDS_update();

    tile tiles[8][8] = {0};
    for (Uint16 i = 0; i < 8; i++) {
        for (Uint16 j = 0; j < 8; j++) {
            TILEMAP_get_at(i, j, &tiles[i][j]);
        }
    }

    fluid_cell fluids[16][16];
    FLUIDS_get_at(0, 0, 16, 16, &fluids);

    bool checks1[] = {
        tiles[0][0].type == tile_dirt,
        tiles[1][0].type == tile_dirt,
        tiles[2][0].type == tile_dirt,
        tiles[3][0].type == tile_dirt,
        tiles[0][0].platform == false,
        tiles[1][0].platform == false,
        tiles[2][0].platform == false,
        tiles[3][0].platform == false,
        tiles[4][0].type == 0,
        tiles[5][0].type == 0,
        tiles[6][0].type == 0,
        tiles[7][0].type == 0,
        tiles[0][1].type == 0,
        tiles[1][1].type == 0,
        tiles[2][1].type == 0,
        tiles[3][1].type == 0,
        tiles[4][1].type == 0,
        tiles[5][1].type == 0,
        tiles[6][1].type == 0,
        tiles[7][1].type == 0,
        tiles[0][2].type == tile_stone,
        tiles[1][2].type == tile_stone,
        tiles[2][2].type == tile_stone,
        tiles[3][2].type == tile_stone,
        tiles[0][2].coords_x == 7,
        tiles[1][2].coords_x == 7,
        tiles[2][2].coords_x == 7,
        tiles[3][2].coords_x == 7,
        tiles[0][2].coords_y == 10,
        tiles[1][2].coords_y == 10,
        tiles[2][2].coords_y == 10,
        tiles[3][2].coords_y == 4,
        tiles[0][2].platform == false,
        tiles[1][2].platform == false,
        tiles[2][2].platform == false,
        tiles[3][2].platform == false,
        tiles[4][2].type == 0,
        tiles[5][2].type == 0,
        tiles[6][2].type == 0,
        tiles[7][2].type == 0,
        tiles[0][3].type == 0,
        tiles[1][3].type == 0,
        tiles[2][3].type == 0,
        tiles[3][3].type == 0,
        tiles[4][3].type == 0,
        tiles[5][3].type == 0,
        tiles[6][3].type == 0,
        tiles[7][3].type == 0,
        tiles[0][4].type == tile_wood,
        tiles[1][4].type == tile_wood,
        tiles[2][4].type == tile_wood,
        tiles[3][4].type == tile_wood,
        tiles[0][4].coords_x == 8,
        tiles[1][4].coords_x == 9,
        tiles[2][4].coords_x == 10,
        tiles[3][4].coords_x == 11,
        tiles[0][4].coords_y == 6,
        tiles[1][4].coords_y == 6,
        tiles[2][4].coords_y == 6,
        tiles[3][4].coords_y == 6,
        tiles[0][4].platform == true,
        tiles[1][4].platform == true,
        tiles[2][4].platform == false,
        tiles[3][4].platform == false,
        tiles[4][4].type == 0,
        tiles[5][4].type == 0,
        tiles[6][4].type == 0,
        tiles[7][4].type == 0,
        tiles[0][5].type == 0,
        tiles[1][5].type == 0,
        tiles[2][5].type == 0,
        tiles[3][5].type == 0,
        tiles[4][5].type == 0,
        tiles[5][5].type == 0,
        tiles[6][5].type == 0,
        tiles[7][5].type == 0,
        tiles[0][6].type == tile_metal,
        tiles[1][6].type == tile_metal,
        tiles[2][6].type == tile_metal,
        tiles[3][6].type == tile_metal,
        tiles[4][6].type == 0,
        tiles[5][6].type == 0,
        tiles[6][6].type == 0,
        tiles[7][6].type == 0,
        tiles[0][6].coords_x == 12,
        tiles[1][6].coords_x == 13,
        tiles[2][6].coords_x == 14,
        tiles[3][6].coords_x == 15,
        tiles[0][6].coords_y == 0,
        tiles[1][6].coords_y == 0,
        tiles[2][6].coords_y == 0,
        tiles[3][6].coords_y == 0,
        tiles[0][6].platform == false,
        tiles[1][6].platform == false,
        tiles[2][6].platform == false,
        tiles[3][6].platform == false,
        tiles[0][7].type == tile_stone,
        tiles[1][7].type == tile_stone,
        tiles[2][7].type == tile_stone,
        tiles[3][7].type == tile_stone,
        tiles[0][7].coords_x == 4,
        tiles[1][7].coords_x == 5,
        tiles[2][7].coords_x == 6,
        tiles[3][7].coords_x == 7,
        tiles[0][7].coords_y == 12,
        tiles[1][7].coords_y == 12,
        tiles[2][7].coords_y == 12,
        tiles[3][7].coords_y == 12,
        tiles[0][7].platform == true,
        tiles[1][7].platform == true,
        tiles[2][7].platform == true,
        tiles[3][7].platform == true,
        tiles[4][7].type == 0,
        tiles[5][7].type == 0,
        tiles[6][7].type == 0,
        tiles[7][7].type == 0,
        fluids[10][2].volume[fluid_water] == 1.0f,
        fluids[11][2].volume[fluid_water] == 1.0f,
        fluids[12][2].volume[fluid_water] == 1.0f,
        fluids[13][2].volume[fluid_water] == 1.0f,
        fluids[10][3].volume[fluid_water] == 1.0f,
        fluids[11][3].volume[fluid_water] == 1.0f,
        fluids[12][3].volume[fluid_water] == 1.0f,
        fluids[13][3].volume[fluid_water] == 1.0f,
        fluids[10][4].volume[fluid_water] == 1.0f,
        fluids[11][4].volume[fluid_water] == 1.0f,
        fluids[12][4].volume[fluid_water] == 1.0f,
        fluids[13][4].volume[fluid_water] == 1.0f,
        fluids[10][5].volume[fluid_water] == 1.0f,
        fluids[11][5].volume[fluid_water] == 1.0f,
        fluids[12][5].volume[fluid_water] == 1.0f,
        fluids[13][5].volume[fluid_water] == 1.0f,
    };

    expect("Loaded mocked chunk, checking tiles and fluids...",
           sizeof(checks1),
           checks1);

    clear_tiles_and_fluids();
    int res = CHUNKS_load_at(0, 20, 30);
    FLUIDS_update();

    tile t0 = (tile){0};
    tile t1 = (tile){0};
    tile t2 = (tile){0};
    TILEMAP_get_at(20, 30, &t0);
    TILEMAP_get_at(20, 32, &t1);
    TILEMAP_get_at(20, 37, &t2);
    fluid_cell offset_fluids[4][4] = {0};
    FLUIDS_get_at(50, 62, 4, 4, &offset_fluids);
    fluid_cell origin_fluids[4][4] = {0};
    FLUIDS_get_at(10, 2, 4, 4, &origin_fluids);
    bool checks2[] = {
        res == 0,
        t0.type == tile_dirt,
        t1.type == tile_stone,
        t2.type == tile_stone,
        t2.platform == true,
        is_empty_at(0, 0),
        is_empty_at(19, 30),
        is_empty_at(28, 30),
        is_empty_at(20, 29),
        is_empty_at(20, 38),
        offset_fluids[0][0].volume[fluid_water] == 1.0f,
        offset_fluids[3][0].volume[fluid_water] == 1.0f,
        offset_fluids[0][3].volume[fluid_water] == 1.0f,
        offset_fluids[3][3].volume[fluid_water] == 1.0f,
        origin_fluids[0][0].volume[fluid_water] == 0.0f,
        origin_fluids[3][3].volume[fluid_water] == 0.0f,
    };

    expect("Loaded mocked chunk at an offset, checking tiles and fluids...",
           sizeof(checks2),
           checks2);

    clear_tiles_and_fluids();
    int res0 = CHUNKS_load_at(0, TILEMAP_SIZE_X - 8, TILEMAP_SIZE_Y - 8);
    int res1 = CHUNKS_load_at(0, TILEMAP_SIZE_X - 7, 0);
    int res2 = CHUNKS_load_at(0, 0, TILEMAP_SIZE_Y - 7);
    expect("Loaded mocked chunk flush with the tilemap's corner and one tile "
           "past its edges, checking results...",
           3,
           (bool[]){res0 == 0, res1 == 2, res2 == 2});

    TILEMAP_clear();
    FLUIDS_stop();
    CHUNKS_stop();
}
