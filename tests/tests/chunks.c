#include "engine/chunks.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/physics.h"
#include "engine/tilemap.h"
#include "props/vines.h"
#include "tests.h"
#include "tests/chunks.h"
#include <SDL3/SDL.h>
#include <stdio.h>

static bool is_empty_at(Uint16 x, Uint16 y) {
    tile t = (tile){0};
    TILEMAP_get_at(x, y, &t);
    return t.type == 0;
}

#ifdef TEST
// Whether the point at `chain[i]` is at `(x,y)` and pinned as `pinned`.
static bool
is_vine_point(dynarr* chain, size_t i, double x, double y, bool pinned) {
    prop_point p = (prop_point){0};
    PHYSICS_get_prop_point((size_t)chain->at[i], &p);
    return p.x == x && p.y == y && p.pinned == pinned;
}

// Whether the connection at `conn_id` links `chain[i]` to `chain[i + 1]` as a
// vine segment.
static bool is_vine_link(dynarr* chain, size_t i, size_t conn_id) {
    size_t a = 0;
    size_t b = 0;
    double elasticity = 0.0f;
    double length = 0.0f;
    bool two_ways = false;
    PHYSICS_test_get_connection(
        conn_id, &a, &b, &elasticity, &length, &two_ways);
    return a == (size_t)chain->at[i] && b == (size_t)chain->at[i + 1] &&
           elasticity == 0.0f && length == SPRITESHEET_CELL_Y / 2.0f &&
           two_ways;
}
#endif

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
        data.vines.len == 4,
        ((vine_spawn*)data.vines.at[0])->double_pinned == true,
        ((vine_spawn*)data.vines.at[0])->a_x == 21,
        ((vine_spawn*)data.vines.at[0])->a_y == 24,
        ((vine_spawn*)data.vines.at[0])->b_x == 29,
        ((vine_spawn*)data.vines.at[0])->b_y == 24,
        ((vine_spawn*)data.vines.at[0])->bend_y == 28,
        ((vine_spawn*)data.vines.at[0])->segments == 4,
        ((vine_spawn*)data.vines.at[1])->double_pinned == false,
        ((vine_spawn*)data.vines.at[1])->a_x == 18,
        ((vine_spawn*)data.vines.at[1])->a_y == 40,
        ((vine_spawn*)data.vines.at[1])->b_x == 18,
        ((vine_spawn*)data.vines.at[1])->b_y == 52,
        ((vine_spawn*)data.vines.at[1])->bend_y == 52,
        ((vine_spawn*)data.vines.at[1])->segments == 3,
        ((vine_spawn*)data.vines.at[2])->double_pinned == false,
        ((vine_spawn*)data.vines.at[2])->a_x == 34,
        ((vine_spawn*)data.vines.at[2])->a_y == 40,
        ((vine_spawn*)data.vines.at[2])->b_x == 64,
        ((vine_spawn*)data.vines.at[2])->b_y == 52,
        ((vine_spawn*)data.vines.at[2])->bend_y == 52,
        ((vine_spawn*)data.vines.at[2])->segments == 10,
        ((vine_spawn*)data.vines.at[3])->double_pinned == false,
        ((vine_spawn*)data.vines.at[3])->a_x == 53,
        ((vine_spawn*)data.vines.at[3])->a_y == 40,
        ((vine_spawn*)data.vines.at[3])->b_x == 53,
        ((vine_spawn*)data.vines.at[3])->b_y == 60,
        ((vine_spawn*)data.vines.at[3])->bend_y == 60,
        ((vine_spawn*)data.vines.at[3])->segments == 5,
    };

    expect("Initiated mocked chunk, checking contents...",
           sizeof(checks0),
           checks0);

    FLUIDS_start();
    PHYSICS_start();
    VINES_start();
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

#ifdef TEST
    // Vines are created in load order, so are their points and connections.
    // Nothing updated them, so the first load's are still where they spawned.
    dynarr vine0 = VINES_test_get_vine(0);
    dynarr vine1 = VINES_test_get_vine(1);
    dynarr vine2 = VINES_test_get_vine(2);
    dynarr vine3 = VINES_test_get_vine(3);
    dynarr vine4 = VINES_test_get_vine(4);
    dynarr vine5 = VINES_test_get_vine(5);
    bool checks3[] = {
        vine0.len == 5,
        is_vine_point(&vine0, 0, 21.0f, 24.0f, true),
        is_vine_point(&vine0, 1, 21.0f, 28.0f, false),
        is_vine_point(&vine0, 2, 25.0f, 28.0f, false),
        is_vine_point(&vine0, 3, 29.0f, 28.0f, false),
        is_vine_point(&vine0, 4, 29.0f, 24.0f, true),
        is_vine_link(&vine0, 0, 0),
        is_vine_link(&vine0, 1, 1),
        is_vine_link(&vine0, 2, 2),
        is_vine_link(&vine0, 3, 3),
        vine1.len == 4,
        is_vine_point(&vine1, 0, 18.0f, 40.0f, true),
        is_vine_point(&vine1, 1, 18.0f, 44.0f, false),
        is_vine_point(&vine1, 2, 18.0f, 48.0f, false),
        is_vine_point(&vine1, 3, 18.0f, 52.0f, false),
        is_vine_link(&vine1, 0, 4),
        is_vine_link(&vine1, 1, 5),
        is_vine_link(&vine1, 2, 6),
        // Bends right and ends on the chunk's side, crossing vine3
        vine2.len == 11,
        is_vine_point(&vine2, 0, 34.0f, 40.0f, true),
        is_vine_point(&vine2, 5, 43.0f, 52.0f, false),
        is_vine_point(&vine2, 10, 64.0f, 52.0f, false),
        vine3.len == 6,
        is_vine_point(&vine3, 0, 53.0f, 40.0f, true),
        is_vine_point(&vine3, 3, 53.0f, 52.0f, false),
        is_vine_point(&vine3, 5, 53.0f, 60.0f, false),
        // Offset copies of vine0 and vine1
        is_vine_point(&vine4, 0, 181.0f, 264.0f, true),
        is_vine_point(&vine4, 4, 189.0f, 264.0f, true),
        is_vine_point(&vine5, 0, 178.0f, 280.0f, true),
        is_vine_point(&vine5, 3, 178.0f, 292.0f, false),
    };
    clear_dynarr(&vine0);
    clear_dynarr(&vine1);
    clear_dynarr(&vine2);
    clear_dynarr(&vine3);
    clear_dynarr(&vine4);
    clear_dynarr(&vine5);

    expect("Checking mocked chunk's vines, loaded normally and at an "
           "offset...",
           sizeof(checks3),
           checks3);
#endif

    clear_tiles_and_fluids();
    int res0 = CHUNKS_load_at(0, TILEMAP_SIZE_X - 8, TILEMAP_SIZE_Y - 8);
    int res1 = CHUNKS_load_at(0, TILEMAP_SIZE_X - 7, 0);
    int res2 = CHUNKS_load_at(0, 0, TILEMAP_SIZE_Y - 7);
    expect("Loaded mocked chunk flush with the tilemap's corner and one tile "
           "past its edges, checking results...",
           3,
           (bool[]){res0 == 0, res1 == 2, res2 == 2});

    TILEMAP_clear();
    VINES_stop();
    PHYSICS_stop();
    FLUIDS_stop();
    CHUNKS_stop();
}
