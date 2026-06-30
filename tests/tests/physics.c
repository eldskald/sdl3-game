#include <defs.h>
#include <engine/physics.h>
#include <engine/tilemap.h>
#include <stdio.h>
#include <tests.h>
#include <tests/physics.h>

void test_physics(void) {
    printf(YELLOW "\nTesting physics...\n" RESET);

    PHYSICS_start();

    size_t id = 0;
    PHYSICS_new_body(
        (body){
            .type = physics_actor,
            .x = SPRITESHEET_CELL_X * 2.0f,
            .y = SPRITESHEET_CELL_Y * 2.0f,
            .w = SPRITESHEET_CELL_X,
            .h = SPRITESHEET_CELL_Y,
            .vel_x = 0.0f,
            .vel_y = SPRITESHEET_CELL_Y * 4.0f,
        },
        &id);
    body data = (body){0};
    PHYSICS_get_body(id, &data);
    expect("Created actor body, checking bodies...",
           7,
           (bool[]){
               data.type == physics_actor,
               data.x == SPRITESHEET_CELL_X * 2.0f,
               data.y == SPRITESHEET_CELL_Y * 2.0f,
               data.w == SPRITESHEET_CELL_X,
               data.h == SPRITESHEET_CELL_Y,
               data.vel_x == 0.0f,
               data.vel_y == SPRITESHEET_CELL_Y * 4.0f,
           });

    tile map[5][5] = {
        {
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
        },
        {

            (tile){.mat = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.mat = tile_dirt},
        },
        {
            (tile){.mat = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.mat = tile_dirt},
        },
        {
            (tile){.mat = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.mat = tile_dirt},
        },
        {
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
        },
    };
    TILEMAP_set_at(0, 0, 5, 5, &map);

    for (int i = 0; i < 20; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect(
        "Created wall tiles and ticked more than enough times to have the body "
        "collide with them, checking its position...",
        2,
        (bool[]){data.y == SPRITESHEET_CELL_Y * 3.0f, data.vel_y == 0.0f});

    PHYSICS_set_velocity(
        id, 4.0f * SPRITESHEET_CELL_X, 4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 20; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set a diagonal velocity to the body and ticked more than enough "
           "times to have it collide with the corner, checking its position...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 3.0f <= FABS_ZERO_DIFF,
                    data.y - SPRITESHEET_CELL_Y * 3.0f <= FABS_ZERO_DIFF,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_velocity(id, 0.0f, -4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to up, checking if it will move along the wall and "
           "collide with top right corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 3.0f <= FABS_ZERO_DIFF,
                    data.y - SPRITESHEET_CELL_Y * 1.0f <= FABS_ZERO_DIFF,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_velocity(id, -4.0f * SPRITESHEET_CELL_X, 0.0f);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to the left, checking if it will move along the wall "
           "and collide with the top left corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 1.0f <= FABS_ZERO_DIFF,
                    data.y - SPRITESHEET_CELL_Y * 1.0f <= FABS_ZERO_DIFF,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_velocity(id, 0.0f, 4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to down, checking if it will move along the wall "
           "and collide with the bottom left corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 1.0f <= FABS_ZERO_DIFF,
                    data.y - SPRITESHEET_CELL_Y * 3.0f <= FABS_ZERO_DIFF,
                    data.vel_x - 0.0f <= FABS_ZERO_DIFF,
                    data.vel_y - 0.0f <= FABS_ZERO_DIFF});

    PHYSICS_set_velocity(id, 4.0f * SPRITESHEET_CELL_X, 0.0f);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to the right, checking if it will move along the wall "
           "and collide with the bottom right corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 3.0f <= FABS_ZERO_DIFF,
                    data.y - SPRITESHEET_CELL_Y * 3.0f <= FABS_ZERO_DIFF,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_position(
        id, 2.0f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_get_body(id, &data);
    expect(
        "Setting position, checking to see if the function works properly...",
        4,
        (bool[]){data.x == 2.0f * SPRITESHEET_CELL_X,
                 data.y == 2.0f * SPRITESHEET_CELL_Y,
                 data.vel_x == 0.0f,
                 data.vel_y == 0.0f});

    PHYSICS_add_velocity(
        id, -2.0f * SPRITESHEET_CELL_X, -2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_get_body(id, &data);
    expect("Adding velocity, checking to see if the function works properly...",
           4,
           (bool[]){data.x - 2.0f * SPRITESHEET_CELL_X <= FABS_ZERO_DIFF,
                    data.y - 2.0f * SPRITESHEET_CELL_Y <= FABS_ZERO_DIFF,
                    data.vel_x - -2.0f * SPRITESHEET_CELL_X <= FABS_ZERO_DIFF,
                    data.vel_y - -2.0f * SPRITESHEET_CELL_Y <= FABS_ZERO_DIFF});

    PHYSICS_update_body(id,
                        (body){.x = 0.0f,
                               .y = 0.0f,
                               .vel_x = 0.0f,
                               .vel_y = 0.0f,
                               .w = 2 * SPRITESHEET_CELL_X,
                               .h = 2 * SPRITESHEET_CELL_Y});
    PHYSICS_get_body(id, &data);
    expect("Updating body data entirely, checking to see if the function works "
           "properly...",
           4,
           (bool[]){data.x == 0.0f,
                    data.y == 0.0f,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f,
                    data.w == 2 * SPRITESHEET_CELL_X,
                    data.h == 2 * SPRITESHEET_CELL_Y});

    PHYSICS_del_body(id);
    int res = PHYSICS_get_body(id, &data);
    expect(
        "Deleted body, checking if it was deleted...", 1, (bool[]){res == 1});

    size_t new_id = 0;
    PHYSICS_new_body(
        (body){
            .type = physics_intangible,
            .x = SPRITESHEET_CELL_X * 2.0f,
            .y = SPRITESHEET_CELL_Y * 2.0f,
            .w = SPRITESHEET_CELL_X,
            .h = SPRITESHEET_CELL_Y,
            .vel_x = SPRITESHEET_CELL_X * 4.0f,
            .vel_y = SPRITESHEET_CELL_Y * -4.0f,
        },
        &new_id);
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_body(new_id, &data);
    expect("Created intangible body and moving it beyond walls, checking its "
           "position and velocity...",
           4,
           (bool[]){data.x - 6.0f * SPRITESHEET_CELL_X <= FABS_ZERO_DIFF,
                    data.y - -2.0f * SPRITESHEET_CELL_Y <= FABS_ZERO_DIFF,
                    data.vel_x - 4.0f * SPRITESHEET_CELL_X <= FABS_ZERO_DIFF,
                    data.vel_y - -4.0f * SPRITESHEET_CELL_Y <= FABS_ZERO_DIFF});

    TILEMAP_clear();

    tile map2[5][5] = {
        {
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){0},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
        },
        {
            (tile){.mat = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.mat = tile_dirt},
        },
        {
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){0},
        },
        {
            (tile){.mat = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.mat = tile_dirt},
        },
        {
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
            (tile){0},
            (tile){.mat = tile_dirt},
            (tile){.mat = tile_dirt},
        },
    };
    TILEMAP_set_at(0, 0, 5, 5, &map2);

    size_t last_id = 0;
    PHYSICS_new_body(
        (body){
            .type = physics_actor,
            .x = SPRITESHEET_CELL_X * 2.0f,
            .y = SPRITESHEET_CELL_Y * 2.0f,
            .w = SPRITESHEET_CELL_X,
            .h = SPRITESHEET_CELL_Y,
            .vel_x = 0.0f,
            .vel_y = 0.0f,
        },
        &last_id);
    bool check = false;
    PHYSICS_is_on_floor(last_id, &check);
    expect(
        "Recreated tiles and body, body in the middle, checking is on floor...",
        1,
        (bool[]){check == false});

    PHYSICS_set_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_on_floor(last_id, &check);
    expect("Teleporting body to a full floor, checking is on floor...",
           1,
           (bool[]){check == true});

    PHYSICS_set_position(
        last_id, 1.5f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_on_floor(last_id, &check);
    expect("Teleporting body to half floor half out, checking is on floor...",
           1,
           (bool[]){check == true});

    PHYSICS_set_position(
        last_id, 2.0f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_on_floor(last_id, &check);
    expect("Teleporting body to one tile gap, checking is on floor...",
           1,
           (bool[]){check == false});

    PHYSICS_set_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y - 1.0f);
    PHYSICS_is_on_floor(last_id, &check);
    expect("Teleporting body to a full floor but one pixel away, checking is "
           "on floor...",
           1,
           (bool[]){check == false});

    PHYSICS_set_position(
        last_id, 2.0f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_on_ceiling(last_id, &check);
    expect("Teleporting body to middle, checking is on ceiling...",
           1,
           (bool[]){check == false});

    PHYSICS_set_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_on_ceiling(last_id, &check);
    expect("Teleporting body to a full ceiling, checking is on ceiling...",
           1,
           (bool[]){check == true});

    PHYSICS_set_position(
        last_id, 1.5f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_on_ceiling(last_id, &check);
    expect(
        "Teleporting body to half ceiling half out, checking is on ceiling...",
        1,
        (bool[]){check == true});

    PHYSICS_set_position(
        last_id, 2.0f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_on_ceiling(last_id, &check);
    expect("Teleporting body to one tile gap, checking is on ceiling...",
           1,
           (bool[]){check == false});

    PHYSICS_set_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y + 1.0f);
    PHYSICS_is_on_ceiling(last_id, &check);
    expect("Teleporting body to a full ceiling but one pixel away, checking is "
           "on ceiling...",
           1,
           (bool[]){check == false});

    TILEMAP_clear();
    PHYSICS_stop();
}
