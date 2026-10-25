#include "engine/tilemap.h"
#include "defs.h"
#include "tests.h"
#include "tests/tilemap.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// Expected coords of each autotile on `tests/mocks/editor/autotiles/`.
// UPDATE IF THE MOCKED AUTOTILE RULE MAPS ARE CHANGED!!!
typedef struct {
    // Tile placed on the tilemap, any tile of the autotile will do.
    Uint8 tile[2];
    // Each tile of a 3x3 block surrounded by empty tiles, indexed by `[x][y]`
    // like the tilemap.
    Uint8 block[3][3][2];
    // Two tiles side by side, the left one and then the right one.
    Uint8 pair[2][2];
    // A single tile at the tilemap's top left corner.
    Uint8 corner[2];
    // A tile with the neighbors of each of `NEIGHBORS_VALS`.
    Uint8 vals[7][2];
} autotile_expected;

#define AUTOTILES_TOTAL 2
static const autotile_expected AUTOTILES[AUTOTILES_TOTAL] = {
    {
        .tile = {1, 0},
        .block = {{{0, 1}, {2, 1}, {0, 2}},
                  {{3, 1}, {2, 0}, {2, 2}},
                  {{1, 1}, {3, 2}, {1, 2}}},
        .pair = {{2, 3}, {3, 4}},
        .corner = {1, 2},
        .vals = {{1, 0}, {0, 2}, {0, 2}, {0, 2}, {3, 6}, {0, 9}, {2, 0}},
    },
    {
        .tile = {5, 0},
        .block = {{{4, 1}, {6, 1}, {4, 2}},
                  {{7, 1}, {6, 0}, {6, 2}},
                  {{5, 1}, {7, 2}, {5, 2}}},
        .pair = {{6, 3}, {7, 4}},
        .corner = {5, 2},
        .vals = {{5, 0}, {4, 2}, {4, 2}, {4, 2}, {7, 6}, {4, 9}, {6, 0}},
    },
};

// Neighbor combinations in the same bitwise format of the autotiles' lookup
// tables. Diagonals only matter when both of their sides are occupied, so the
// ones in between should have the same coords.
static const Uint8 NEIGHBORS_VALS[7] = {0, 23, 54, 55, 110, 127, 255};
static const int NEIGHBORS[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

static tile autotile(Uint8 idx) {
    return (tile){
        .type = tile_dirt,
        .coords_x = AUTOTILES[idx].tile[0],
        .coords_y = AUTOTILES[idx].tile[1],
    };
}

static bool has_coords(Uint16 x, Uint16 y, Uint8 coords_x, Uint8 coords_y) {
    tile t = (tile){0};
    TILEMAP_get_at(x, y, &t);
    return t.coords_x == coords_x && t.coords_y == coords_y;
}

static bool is_empty(Uint16 x, Uint16 y) {
    tile t = (tile){0};
    TILEMAP_get_at(x, y, &t);
    return t.type == 0 && !t.platform && t.coords_x == 0 && t.coords_y == 0;
}

void test_tilemap(void) {
    printf(YELLOW "\nTesting tilemap...\n" RESET);

    TILEMAP_clear();

    tile t = (tile){.type = tile_metal, .coords_x = 1, .coords_y = 1};
    int res0 = TILEMAP_get_at(10, 10, &t);
    int res1 = TILEMAP_get_at(TILEMAP_SIZE_X, 0, &t);
    int res2 = TILEMAP_get_at(0, TILEMAP_SIZE_Y, &t);
    expect("Got tiles from an empty tilemap, checking results and tile...",
           4,
           (bool[]){
               res0 == 0,
               res1 == 1,
               res2 == 1,
               is_empty(10, 10),
           });

    // Metal at these coords isn't part of any autotile, so it's left as is.
    tile data0[3][2] = {
        {
            {.type = tile_metal, .coords_x = 12, .coords_y = 12},
            {.type = tile_metal, .coords_x = 12, .coords_y = 13},
        },
        {
            {.type = tile_metal,
             .platform = true,
             .coords_x = 13,
             .coords_y = 12},
            {.type = tile_metal,
             .platform = true,
             .coords_x = 13,
             .coords_y = 13},
        },
        {
            {.type = tile_wood, .coords_x = 14, .coords_y = 12},
            {.type = tile_wood, .coords_x = 14, .coords_y = 13},
        },
    };
    res0 = TILEMAP_set_at(10, 20, 3, 2, &data0);
    tile got[3][2] = {0};
    for (Uint16 i = 0; i < 3; i++)
        for (Uint16 j = 0; j < 2; j++)
            TILEMAP_get_at(10 + i, 20 + j, &got[i][j]);
    bool checks0[] = {
        res0 == 0,
        got[0][0].type == tile_metal,
        got[0][1].type == tile_metal,
        got[1][0].type == tile_metal,
        got[1][1].type == tile_metal,
        got[2][0].type == tile_wood,
        got[2][1].type == tile_wood,
        got[0][0].platform == false,
        got[0][1].platform == false,
        got[1][0].platform == true,
        got[1][1].platform == true,
        got[2][0].platform == false,
        got[2][1].platform == false,
        got[0][0].coords_x == 12 && got[0][0].coords_y == 12,
        got[0][1].coords_x == 12 && got[0][1].coords_y == 13,
        got[1][0].coords_x == 13 && got[1][0].coords_y == 12,
        got[1][1].coords_x == 13 && got[1][1].coords_y == 13,
        got[2][0].coords_x == 14 && got[2][0].coords_y == 12,
        got[2][1].coords_x == 14 && got[2][1].coords_y == 13,
        is_empty(9, 20),
        is_empty(13, 20),
        is_empty(10, 19),
        is_empty(10, 22),
    };
    expect(
        "Set a 3x2 dataset on the tilemap, checking tiles and around them...",
        sizeof(checks0),
        checks0);

    tile data1[2][1] = {{autotile(0)}, {autotile(0)}};
    res0 = TILEMAP_set_at(TILEMAP_SIZE_X - 1, 0, 2, 1, &data1);
    res1 = TILEMAP_set_at(0, TILEMAP_SIZE_Y - 1, 1, 2, (tile(*)[1][2]) & data1);
    expect("Set datasets reaching outside the tilemap, checking results and "
           "if tiles weren't set...",
           4,
           (bool[]){
               res0 == 1,
               res1 == 1,
               is_empty(TILEMAP_SIZE_X - 1, 0),
               is_empty(0, TILEMAP_SIZE_Y - 1),
           });

    res0 = TILEMAP_set_at(TILEMAP_SIZE_X - 2, 0, 2, 1, &data1);
    res1 = TILEMAP_set_at(0, TILEMAP_SIZE_Y - 2, 1, 2, (tile(*)[1][2]) & data1);
    expect("Set datasets reaching the tilemap's last column and row, checking "
           "results and if tiles were set...",
           6,
           (bool[]){
               res0 == 0,
               res1 == 0,
               !is_empty(TILEMAP_SIZE_X - 2, 0),
               !is_empty(TILEMAP_SIZE_X - 1, 0),
               !is_empty(0, TILEMAP_SIZE_Y - 2),
               !is_empty(0, TILEMAP_SIZE_Y - 1),
           });

    TILEMAP_clear();
    expect("Cleared the tilemap, checking if tiles are empty...",
           6,
           (bool[]){
               is_empty(10, 20),
               is_empty(11, 20),
               is_empty(12, 20),
               is_empty(10, 21),
               is_empty(11, 21),
               is_empty(12, 21),
           });

    for (Uint8 idx = 0; idx < AUTOTILES_TOTAL; idx++) {
        autotile_expected e = AUTOTILES[idx];
        char desc[FAIL_MESSAGE_MAX] = "";

        tile block[3][3] = {0};
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                block[i][j] = autotile(idx);
        TILEMAP_set_at(10, 10, 3, 3, &block);
        bool checks0[9] = {0};
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                checks0[i + 3 * j] = has_coords(
                    10 + i, 10 + j, e.block[i][j][0], e.block[i][j][1]);
        sprintf(
            desc, "Set a 3x3 block of autotile %d, checking coords...", idx);
        expect(desc, 9, checks0);
        TILEMAP_clear();

        // Setting a tile updates the autotiles around it.
        tile single = autotile(idx);
        TILEMAP_set_at(10, 10, 1, 1, (tile(*)[1][1]) & single);
        bool before = has_coords(10, 10, e.vals[0][0], e.vals[0][1]);
        TILEMAP_set_at(11, 10, 1, 1, (tile(*)[1][1]) & single);
        sprintf(desc,
                "Set a tile of autotile %d, then another to its right, "
                "checking if the first one got updated...",
                idx);
        expect(desc,
               3,
               (bool[]){
                   before,
                   has_coords(10, 10, e.pair[0][0], e.pair[0][1]),
                   has_coords(11, 10, e.pair[1][0], e.pair[1][1]),
               });
        TILEMAP_clear();

        // Outside the tilemap counts as a neighbor of the same autotile.
        TILEMAP_set_at(0, 0, 1, 1, (tile(*)[1][1]) & single);
        sprintf(desc,
                "Set a tile of autotile %d at the tilemap's corner, checking "
                "if it connected to the outside...",
                idx);
        expect(desc, 1, (bool[]){has_coords(0, 0, e.corner[0], e.corner[1])});
        TILEMAP_clear();

        bool checks1[7] = {0};
        for (int k = 0; k < 7; k++) {
            TILEMAP_set_at(10, 10, 1, 1, (tile(*)[1][1]) & single);
            for (int n = 0; n < 8; n++)
                if (NEIGHBORS_VALS[k] & (1 << n))
                    TILEMAP_set_at(10 + NEIGHBORS[n][0],
                                   10 + NEIGHBORS[n][1],
                                   1,
                                   1,
                                   (tile(*)[1][1]) & single);
            checks1[k] = has_coords(10, 10, e.vals[k][0], e.vals[k][1]);
            TILEMAP_clear();
        }
        sprintf(desc,
                "Surrounded tiles of autotile %d with several combinations of "
                "neighbors, checking coords...",
                idx);
        expect(desc, 7, checks1);
    }

    // Adjacent tiles of different autotiles don't connect to each other.
    tile data2[2][1] = {{autotile(0)}, {autotile(1)}};
    TILEMAP_set_at(10, 10, 2, 1, &data2);
    expect("Set tiles of each autotile next to each other, checking if they "
           "didn't connect...",
           2,
           (bool[]){
               has_coords(
                   10, 10, AUTOTILES[0].vals[0][0], AUTOTILES[0].vals[0][1]),
               has_coords(
                   11, 10, AUTOTILES[1].vals[0][0], AUTOTILES[1].vals[0][1]),
           });
    TILEMAP_clear();

    // Non autotiled tiles don't count as neighbors and aren't updated.
    tile data3[2][1] = {
        {autotile(0)},
        {{.type = tile_metal, .coords_x = 12, .coords_y = 12}},
    };
    TILEMAP_set_at(10, 10, 2, 1, &data3);
    expect("Set an autotile next to a non autotiled tile, checking if they "
           "didn't connect...",
           2,
           (bool[]){
               has_coords(
                   10, 10, AUTOTILES[0].vals[0][0], AUTOTILES[0].vals[0][1]),
               has_coords(11, 10, 12, 12),
           });
    TILEMAP_clear();
}
