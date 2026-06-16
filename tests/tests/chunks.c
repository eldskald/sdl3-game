#include <chunks.h>
#include <stdio.h>
#include <tests.h>
#include <tests/chunks.h>
#include <tilemap.h>

void test_chunks(void) {
    printf(YELLOW "\nTesting chunks...\n" RESET);

    CHUNKS_start();

    chunk data = (chunk){0};
    CHUNKS_get(0, &data);
    tile(*map)[data.w][data.h] = (tile(*)[data.w][data.h])data.map;
    expect("Loaded mocked chunk, checking tiles...",
           50,
           (bool[]){
               data.w == 8,
               data.h == 8,
               (*map)[0][0].mat == dirt,
               (*map)[1][0].mat == dirt,
               (*map)[2][0].mat == dirt,
               (*map)[3][0].mat == dirt,
               (*map)[0][0].coords_x == 0,
               (*map)[1][0].coords_x == 1,
               (*map)[2][0].coords_x == 2,
               (*map)[3][0].coords_x == 3,
               (*map)[0][0].coords_y == 1,
               (*map)[1][0].coords_y == 1,
               (*map)[2][0].coords_y == 1,
               (*map)[3][0].coords_y == 1,
               (*map)[0][2].mat == wood,
               (*map)[1][2].mat == wood,
               (*map)[2][2].mat == wood,
               (*map)[3][2].mat == wood,
               (*map)[0][2].coords_x == 4,
               (*map)[1][2].coords_x == 5,
               (*map)[2][2].coords_x == 6,
               (*map)[3][2].coords_x == 7,
               (*map)[0][2].coords_y == 6,
               (*map)[1][2].coords_y == 6,
               (*map)[2][2].coords_y == 6,
               (*map)[3][2].coords_y == 6,
               (*map)[0][4].mat == stone,
               (*map)[1][4].mat == stone,
               (*map)[2][4].mat == stone,
               (*map)[3][4].mat == stone,
               (*map)[0][4].coords_x == 8,
               (*map)[1][4].coords_x == 9,
               (*map)[2][4].coords_x == 10,
               (*map)[3][4].coords_x == 11,
               (*map)[0][4].coords_y == 6,
               (*map)[1][4].coords_y == 6,
               (*map)[2][4].coords_y == 6,
               (*map)[3][4].coords_y == 6,
               (*map)[0][6].mat == metal,
               (*map)[1][6].mat == metal,
               (*map)[2][6].mat == metal,
               (*map)[3][6].mat == metal,
               (*map)[0][6].coords_x == 12,
               (*map)[1][6].coords_x == 13,
               (*map)[2][6].coords_x == 14,
               (*map)[3][6].coords_x == 15,
               (*map)[0][6].coords_y == 0,
               (*map)[1][6].coords_y == 0,
               (*map)[2][6].coords_y == 0,
               (*map)[3][6].coords_y == 0,
           });

    CHUNKS_stop();
}
