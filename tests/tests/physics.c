#include "engine/physics.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/game-manager.h"
#include "engine/tilemap.h"
#include "tests.h"
#include "tests/physics.h"
#include <SDL3/SDL.h>
#include <stdio.h>

static size_t on_delete_calls = 0;
static size_t on_delete_point_id = 0;
static size_t on_delete_arg = 0;

static void record_on_delete(size_t point_id, size_t arg) {
    on_delete_calls++;
    on_delete_point_id = point_id;
    on_delete_arg = arg;
}

void test_physics(void) {
    printf(YELLOW "\nTesting physics...\n" RESET);

    PHYSICS_start();

    // Body tests
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
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
        },
        {

            (tile){.type = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.type = tile_dirt},
        },
        {
            (tile){.type = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.type = tile_dirt},
        },
        {
            (tile){.type = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.type = tile_dirt},
        },
        {
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
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

    PHYSICS_set_body_velocity(
        id, 4.0f * SPRITESHEET_CELL_X, 4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 20; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set a diagonal velocity to the body and ticked more than enough "
           "times to have it collide with the corner, checking its position...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 3.0f <= EPSILON,
                    data.y - SPRITESHEET_CELL_Y * 3.0f <= EPSILON,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_velocity(id, 0.0f, -4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to up, checking if it will move along the wall and "
           "collide with top right corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 3.0f <= EPSILON,
                    data.y - SPRITESHEET_CELL_Y * 1.0f <= EPSILON,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_velocity(id, -4.0f * SPRITESHEET_CELL_X, 0.0f);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to the left, checking if it will move along the wall "
           "and collide with the top left corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 1.0f <= EPSILON,
                    data.y - SPRITESHEET_CELL_Y * 1.0f <= EPSILON,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_velocity(id, 0.0f, 4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to down, checking if it will move along the wall "
           "and collide with the bottom left corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 1.0f <= EPSILON,
                    data.y - SPRITESHEET_CELL_Y * 3.0f <= EPSILON,
                    data.vel_x - 0.0f <= EPSILON,
                    data.vel_y - 0.0f <= EPSILON});

    PHYSICS_set_body_velocity(id, 4.0f * SPRITESHEET_CELL_X, 0.0f);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(id, &data);
    expect("Set velocity to the right, checking if it will move along the wall "
           "and collide with the bottom right corner...",
           4,
           (bool[]){data.x - SPRITESHEET_CELL_X * 3.0f <= EPSILON,
                    data.y - SPRITESHEET_CELL_Y * 3.0f <= EPSILON,
                    data.vel_x == 0.0f,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_position(
        id, 2.0f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_get_body(id, &data);
    expect(
        "Setting position, checking to see if the function works properly...",
        4,
        (bool[]){data.x == 2.0f * SPRITESHEET_CELL_X,
                 data.y == 2.0f * SPRITESHEET_CELL_Y,
                 data.vel_x == 0.0f,
                 data.vel_y == 0.0f});

    PHYSICS_add_body_velocity(
        id, -2.0f * SPRITESHEET_CELL_X, -2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_get_body(id, &data);
    expect("Adding velocity, checking to see if the function works properly...",
           4,
           (bool[]){data.x - 2.0f * SPRITESHEET_CELL_X <= EPSILON,
                    data.y - 2.0f * SPRITESHEET_CELL_Y <= EPSILON,
                    data.vel_x - -2.0f * SPRITESHEET_CELL_X <= EPSILON,
                    data.vel_y - -2.0f * SPRITESHEET_CELL_Y <= EPSILON});

    PHYSICS_edit_body(id,
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
           (bool[]){data.x - 6.0f * SPRITESHEET_CELL_X <= EPSILON,
                    data.y - -2.0f * SPRITESHEET_CELL_Y <= EPSILON,
                    data.vel_x - 4.0f * SPRITESHEET_CELL_X <= EPSILON,
                    data.vel_y - -4.0f * SPRITESHEET_CELL_Y <= EPSILON});

    TILEMAP_clear();

    tile map2[5][5] = {
        {
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){0},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
        },
        {
            (tile){.type = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.type = tile_dirt},
        },
        {
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){0},
        },
        {
            (tile){.type = tile_dirt},
            (tile){0},
            (tile){0},
            (tile){0},
            (tile){.type = tile_dirt},
        },
        {
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
            (tile){0},
            (tile){.type = tile_dirt},
            (tile){.type = tile_dirt},
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
    PHYSICS_is_body_on_floor(last_id, &check);
    expect(
        "Recreated tiles and body, body in the middle, checking is on floor...",
        1,
        (bool[]){check == false});

    PHYSICS_set_body_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_floor(last_id, &check);
    expect("Teleporting body to a full floor, checking is on floor...",
           1,
           (bool[]){check == true});

    PHYSICS_set_body_position(
        last_id, 1.5f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_floor(last_id, &check);
    expect("Teleporting body to half floor half out, checking is on floor...",
           1,
           (bool[]){check == true});

    PHYSICS_set_body_position(
        last_id, 2.0f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_floor(last_id, &check);
    expect("Teleporting body to one tile gap, checking is on floor...",
           1,
           (bool[]){check == false});

    PHYSICS_set_body_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y - 1.0f);
    PHYSICS_is_body_on_floor(last_id, &check);
    expect("Teleporting body to a full floor but one pixel away, checking is "
           "on floor...",
           1,
           (bool[]){check == false});

    PHYSICS_set_body_position(
        last_id, 2.0f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_ceiling(last_id, &check);
    expect("Teleporting body to middle, checking is on ceiling...",
           1,
           (bool[]){check == false});

    PHYSICS_set_body_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_ceiling(last_id, &check);
    expect("Teleporting body to a full ceiling, checking is on ceiling...",
           1,
           (bool[]){check == true});

    PHYSICS_set_body_position(
        last_id, 1.5f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_ceiling(last_id, &check);
    expect(
        "Teleporting body to half ceiling half out, checking is on ceiling...",
        1,
        (bool[]){check == true});

    PHYSICS_set_body_position(
        last_id, 2.0f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_ceiling(last_id, &check);
    expect("Teleporting body to one tile gap, checking is on ceiling...",
           1,
           (bool[]){check == false});

    PHYSICS_set_body_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y + 1.0f);
    PHYSICS_is_body_on_ceiling(last_id, &check);
    expect("Teleporting body to a full ceiling but one pixel away, checking is "
           "on ceiling...",
           1,
           (bool[]){check == false});

    TILEMAP_clear();

    tile map3[7][7] = {0};
    for (int x = 0; x < 7; x++)
        for (int y = 0; y < 7; y++)
            if (x == 0 || y == 0 || x == 6 || y == 6)
                map3[x][y] = (tile){.type = tile_dirt};
    for (int x = 1; x < 5; x++)
        map3[x][3] = (tile){.type = tile_wood, .platform = true};
    map3[5][3] = (tile){.type = tile_dirt};
    TILEMAP_set_at(0, 0, 7, 7, &map3);

    PHYSICS_set_body_position(
        last_id, 3.0f * SPRITESHEET_CELL_X, 1.0f * SPRITESHEET_CELL_Y);
    PHYSICS_set_body_velocity(last_id, 0.0f, 4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 40; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    expect("Made a room with platforms and dropped the body on them, checking "
           "if it landed on top...",
           2,
           (bool[]){SDL_fabs(data.y - 2.0f * SPRITESHEET_CELL_Y) <= EPSILON,
                    data.vel_y == 0.0f});

    PHYSICS_is_body_on_floor(last_id, &check);
    bool on_platform = false;
    PHYSICS_is_body_on_platform(last_id, &on_platform);
    expect("Checking is on floor and is on platform while on platforms...",
           2,
           (bool[]){check == true, on_platform == true});

    PHYSICS_set_body_position(
        last_id, 4.5f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_floor(last_id, &check);
    PHYSICS_is_body_on_platform(last_id, &on_platform);
    expect("Teleporting body to half platform half solid tile, checking is on "
           "floor and is on platform...",
           2,
           (bool[]){check == true, on_platform == false});

    PHYSICS_set_body_position(
        last_id, 3.0f * SPRITESHEET_CELL_X, 4.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_ceiling(last_id, &check);
    expect("Teleporting body right under the platforms, checking is on "
           "ceiling...",
           1,
           (bool[]){check == false});

    PHYSICS_set_body_position(
        last_id, 3.0f * SPRITESHEET_CELL_X, 5.0f * SPRITESHEET_CELL_Y);
    PHYSICS_set_body_velocity(last_id, 0.0f, -8.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    expect("Teleporting body to the floor and setting velocity to up, checking "
           "if it went through the platforms and hit the ceiling...",
           2,
           (bool[]){SDL_fabs(data.y - 1.0f * SPRITESHEET_CELL_Y) <= EPSILON,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_position(
        last_id, 3.0f * SPRITESHEET_CELL_X, 2.5f * SPRITESHEET_CELL_Y);
    PHYSICS_set_body_velocity(last_id, 0.0f, 4.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    expect("Teleporting body to halfway through the platforms and setting "
           "velocity to down, checking if it fell to the floor...",
           2,
           (bool[]){SDL_fabs(data.y - 5.0f * SPRITESHEET_CELL_Y) <= EPSILON,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_position(
        last_id, 1.0f * SPRITESHEET_CELL_X, 3.0f * SPRITESHEET_CELL_Y);
    PHYSICS_set_body_velocity(last_id, 4.0f * SPRITESHEET_CELL_X, 0.0f);
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    expect("Teleporting body inside the platforms row and setting velocity to "
           "the right, checking if it went through them and hit the solid "
           "tile...",
           3,
           (bool[]){SDL_fabs(data.x - 4.0f * SPRITESHEET_CELL_X) <= EPSILON,
                    SDL_fabs(data.y - 3.0f * SPRITESHEET_CELL_Y) <= EPSILON,
                    data.vel_x == 0.0f});

    PHYSICS_set_body_velocity(last_id, -4.0f * SPRITESHEET_CELL_X, 0.0f);
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    expect("Setting velocity to the left, checking if it went through the "
           "platforms and hit the wall...",
           3,
           (bool[]){SDL_fabs(data.x - 1.0f * SPRITESHEET_CELL_X) <= EPSILON,
                    SDL_fabs(data.y - 3.0f * SPRITESHEET_CELL_Y) <= EPSILON,
                    data.vel_x == 0.0f});

    PHYSICS_set_body_position(
        last_id, 3.0f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_set_body_velocity(last_id, 0.0f, 2.0f * SPRITESHEET_CELL_Y);
    for (int i = 0; i < 30; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    bool stayed = SDL_fabs(data.y - 2.0f * SPRITESHEET_CELL_Y) <= EPSILON;
    PHYSICS_set_body_velocity(last_id, 0.0f, 2.0f * SPRITESHEET_CELL_Y);
    int drop_res = PHYSICS_drop_down_body(last_id);
    for (int i = 0; i < 120; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    expect("Teleporting body on the platforms and moving it down slowly, then "
           "dropping down, checking if it stayed on them before and fell to "
           "the floor after...",
           4,
           (bool[]){stayed,
                    drop_res == 0,
                    SDL_fabs(data.y - 5.0f * SPRITESHEET_CELL_Y) <= EPSILON,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_position(
        last_id, 4.5f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_set_body_velocity(last_id, 0.0f, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_drop_down_body(last_id);
    for (int i = 0; i < 120; i++)
        PHYSICS_update();
    PHYSICS_get_body(last_id, &data);
    expect("Teleporting body to half platform half solid tile and dropping "
           "down, checking if the solid tile held it...",
           2,
           (bool[]){SDL_fabs(data.y - 2.0f * SPRITESHEET_CELL_Y) <= EPSILON,
                    data.vel_y == 0.0f});

    PHYSICS_set_body_position(
        last_id, 3.0f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_set_body_velocity(last_id, 0.0f, 0.0f);
    PHYSICS_drop_down_body(last_id);
    PHYSICS_is_body_on_floor(last_id, &check);
    PHYSICS_is_body_on_platform(last_id, &on_platform);
    expect("Teleporting body on the platforms and dropping down, checking is "
           "on floor and is on platform...",
           2,
           (bool[]){check == false, on_platform == false});

    PHYSICS_set_body_position(
        last_id, 4.5f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_floor(last_id, &check);
    PHYSICS_is_body_on_platform(last_id, &on_platform);
    expect("Teleporting body to half platform half solid tile while dropping "
           "down, checking is on floor and is on platform...",
           2,
           (bool[]){check == true, on_platform == false});

    for (int i = 0; i < 30; i++)
        PHYSICS_update();
    PHYSICS_set_body_position(
        last_id, 3.0f * SPRITESHEET_CELL_X, 2.0f * SPRITESHEET_CELL_Y);
    PHYSICS_is_body_on_floor(last_id, &check);
    PHYSICS_is_body_on_platform(last_id, &on_platform);
    expect("Ticked until done dropping down and teleported back on the "
           "platforms, checking is on floor and is on platform...",
           2,
           (bool[]){check == true, on_platform == true});

    TILEMAP_clear();
    PHYSICS_del_body(new_id);
    PHYSICS_del_body(last_id);



    // Prop tests
    size_t point_id = 0;
    prop_point point = (prop_point){0};
    double center_x = 3.5f * SPRITESHEET_CELL_X;
    double center_y = 3.5f * SPRITESHEET_CELL_Y;
    double acc = 64.0f * SPRITESHEET_CELL_X;
    PHYSICS_new_prop_point(
        (prop_point){
            .x = center_x,
            .y = center_y,
            .acc_x = 0.0f,
            .acc_y = 0.0f,
            .pinned = false,
        },
        &point_id);
    for (int i = 0; i < 30; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Cleared tiles and bodies, created an unpinned prop point without "
           "acceleration and ticked, checking it didn't move...",
           2,
           (bool[]){point.x == center_x, point.y == center_y});

    tile map4[7][7] = {0};
    for (int x = 0; x < 7; x++)
        for (int y = 0; y < 7; y++)
            if (x == 0 || y == 0 || x == 6 || y == 6)
                map4[x][y] = (tile){.type = tile_dirt};
    TILEMAP_set_at(0, 0, 7, 7, &map4);

    int edit_res = PHYSICS_edit_prop_point(
        point_id, (prop_point){.x = center_x, .y = center_y, .acc_x = acc});
    for (int i = 0; i < 5; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Made a room of tiles around the point and set its acceleration to "
           "the right, checking if it started moving towards the wall...",
           4,
           (bool[]){edit_res == 0,
                    point.x > center_x,
                    point.x < 6.0f * SPRITESHEET_CELL_X,
                    point.y == center_y});

    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Ticked more than enough times for the point to reach the right "
           "wall, checking if it collided...",
           3,
           (bool[]){point.x < 6.0f * SPRITESHEET_CELL_X,
                    point.x > 6.0f * SPRITESHEET_CELL_X - 0.01f,
                    point.y == center_y});

    edit_res = PHYSICS_edit_prop_point(
        point_id, (prop_point){.x = center_x, .y = center_y, .acc_x = -acc});
    PHYSICS_get_prop_point(point_id, &point);
    bool edited = edit_res == 0 && point.x == center_x && point.y == center_y &&
                  point.acc_x == -acc && point.acc_y == 0.0f;
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Edited the point back to the center with acceleration to the "
           "left, checking the edit and if it collided with the left wall...",
           4,
           (bool[]){edited,
                    point.x > 1.0f * SPRITESHEET_CELL_X,
                    point.x < 1.0f * SPRITESHEET_CELL_X + 0.01f,
                    point.y == center_y});

    edit_res = PHYSICS_edit_prop_point(
        point_id, (prop_point){.x = center_x, .y = center_y, .acc_y = acc});
    PHYSICS_get_prop_point(point_id, &point);
    edited = edit_res == 0 && point.x == center_x && point.y == center_y &&
             point.acc_x == 0.0f && point.acc_y == acc;
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Edited the point back to the center with acceleration downwards, "
           "checking the edit and if it collided with the floor...",
           4,
           (bool[]){edited,
                    point.x == center_x,
                    point.y < 6.0f * SPRITESHEET_CELL_Y,
                    point.y > 6.0f * SPRITESHEET_CELL_Y - 0.01f});

    edit_res = PHYSICS_edit_prop_point(
        point_id, (prop_point){.x = center_x, .y = center_y, .acc_y = -acc});
    PHYSICS_get_prop_point(point_id, &point);
    edited = edit_res == 0 && point.x == center_x && point.y == center_y &&
             point.acc_x == 0.0f && point.acc_y == -acc;
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Edited the point back to the center with acceleration upwards, "
           "checking the edit and if it collided with the ceiling...",
           4,
           (bool[]){edited,
                    point.x == center_x,
                    point.y > 1.0f * SPRITESHEET_CELL_Y,
                    point.y < 1.0f * SPRITESHEET_CELL_Y + 0.01f});

    edit_res = PHYSICS_edit_prop_point(
        point_id,
        (prop_point){.x = center_x, .y = center_y, .acc_x = acc, .acc_y = acc});
    PHYSICS_get_prop_point(point_id, &point);
    edited = edit_res == 0 && point.x == center_x && point.y == center_y &&
             point.acc_x == acc && point.acc_y == acc;
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Edited the point back to the center with diagonal acceleration, "
           "checking the edit and if it slid into the bottom right corner...",
           5,
           (bool[]){edited,
                    point.x < 6.0f * SPRITESHEET_CELL_X,
                    point.x > 6.0f * SPRITESHEET_CELL_X - 0.01f,
                    point.y < 6.0f * SPRITESHEET_CELL_Y,
                    point.y > 6.0f * SPRITESHEET_CELL_Y - 0.01f});

    TILEMAP_clear();

    tile map5[7][7] = {0};
    for (int x = 0; x < 7; x++)
        for (int y = 0; y < 7; y++)
            if (x == 0 || y == 0 || x == 6 || y == 6)
                map5[x][y] = (tile){.type = tile_dirt};
    for (int x = 1; x < 5; x++)
        map5[x][3] = (tile){.type = tile_wood, .platform = true};
    map5[5][3] = (tile){.type = tile_dirt};
    TILEMAP_set_at(0, 0, 7, 7, &map5);

    PHYSICS_edit_prop_point(point_id,
                            (prop_point){.x = 2.5f * SPRITESHEET_CELL_X,
                                         .y = 1.5f * SPRITESHEET_CELL_Y,
                                         .acc_y = acc});
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Made a room with platforms and dropped the point on them, checking "
           "if it went through them and collided with the floor...",
           3,
           (bool[]){point.x == 2.5f * SPRITESHEET_CELL_X,
                    point.y < 6.0f * SPRITESHEET_CELL_Y,
                    point.y > 6.0f * SPRITESHEET_CELL_Y - 0.01f});

    PHYSICS_edit_prop_point(point_id,
                            (prop_point){.x = 2.5f * SPRITESHEET_CELL_X,
                                         .y = 5.5f * SPRITESHEET_CELL_Y,
                                         .acc_y = -acc});
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Moved the point under the platforms and set acceleration upwards, "
           "checking if it went through them and collided with the ceiling...",
           3,
           (bool[]){point.x == 2.5f * SPRITESHEET_CELL_X,
                    point.y > 1.0f * SPRITESHEET_CELL_Y,
                    point.y < 1.0f * SPRITESHEET_CELL_Y + 0.01f});

    PHYSICS_edit_prop_point(point_id,
                            (prop_point){.x = 1.5f * SPRITESHEET_CELL_X,
                                         .y = 3.5f * SPRITESHEET_CELL_Y,
                                         .acc_x = acc});
    for (int i = 0; i < 60; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Moved the point inside the platforms row and set acceleration to "
           "the right, checking if it went through them and collided with the "
           "solid tile...",
           3,
           (bool[]){point.x < 5.0f * SPRITESHEET_CELL_X,
                    point.x > 5.0f * SPRITESHEET_CELL_X - 0.01f,
                    point.y == 3.5f * SPRITESHEET_CELL_Y});

    PHYSICS_del_prop_point(point_id);
    TILEMAP_clear();

    double prop_dt = GAME_MANAGER_get_current_dt();
    double start_xy = 10.0f * SPRITESHEET_CELL_X;
    PHYSICS_new_prop_point(
        (prop_point){.x = start_xy, .y = start_xy, .acc_y = acc}, &point_id);
    for (int i = 0; i < 3; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    double prev_x = point.x;
    double prev_y = point.y;
    PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    double fall_dy = point.y - prev_y;
    prev_x = point.x;
    prev_y = point.y;
    int acc_res = PHYSICS_set_prop_point_acceleration(point_id, acc, -acc);
    PHYSICS_get_prop_point(point_id, &point);
    bool acc_set = point.acc_x == acc && point.acc_y == -acc;
    PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect(
        "Made a falling point and set its acceleration to the right and "
        "upwards, checking if it kept its velocity and accelerated by the "
        "new acceleration...",
        4,
        (bool[]){acc_res == 0,
                 acc_set,
                 SDL_fabs(point.x - prev_x - prop_dt * prop_dt * acc) <= 0.001f,
                 SDL_fabs(point.y - prev_y -
                          (fall_dy - prop_dt * prop_dt * acc)) <= 0.001f});

    double vel = 4.0f * SPRITESHEET_CELL_X;
    PHYSICS_set_prop_point_acceleration(point_id, 0.0f, 0.0f);
    int vel_res = PHYSICS_set_prop_point_velocity(point_id, -vel, vel);
    PHYSICS_get_prop_point(point_id, &point);
    prev_x = point.x;
    prev_y = point.y;
    PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Removed its acceleration and set its velocity to the left and "
           "downwards, checking if it moved by that velocity...",
           3,
           (bool[]){vel_res == 0,
                    SDL_fabs(point.x - prev_x + vel * prop_dt) <= 0.001f,
                    SDL_fabs(point.y - prev_y - vel * prop_dt) <= 0.001f});

    double vel_x_before = 0.0f;
    double vel_y_before = 0.0f;
    double vel_x_after = 0.0f;
    double vel_y_after = 0.0f;
    PHYSICS_get_prop_point_velocity(point_id, &vel_x_before, &vel_y_before);
    int damping_res = PHYSICS_set_prop_point_damping(point_id, 1.0f);
    PHYSICS_get_prop_point(point_id, &point);
    bool damping_set = point.damping == 1.0f;
    PHYSICS_get_prop_point_velocity(point_id, &vel_x_after, &vel_y_after);
    prev_x = point.x;
    prev_y = point.y;
    PHYSICS_update();
    PHYSICS_get_prop_point(point_id, &point);
    expect("Set its damping to 1.0, checking if it kept its velocity until the "
           "next step, which stopped it...",
           6,
           (bool[]){damping_res == 0,
                    damping_set,
                    vel_x_after == vel_x_before,
                    vel_y_after == vel_y_before,
                    SDL_fabs(point.x - prev_x) <= 0.001f,
                    SDL_fabs(point.y - prev_y) <= 0.001f});

    PHYSICS_del_prop_point(point_id);
    expect("Deleted the point, checking errors setting acceleration, velocity "
           "and damping of a deleted point and of an index out of range...",
           6,
           (bool[]){
               PHYSICS_set_prop_point_acceleration(point_id, acc, acc) == 1,
               PHYSICS_set_prop_point_velocity(point_id, vel, vel) == 1,
               PHYSICS_set_prop_point_damping(point_id, 0.5f) == 1,
               PHYSICS_set_prop_point_acceleration(
                   PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP << 4, acc, acc) == 3,
               PHYSICS_set_prop_point_velocity(
                   PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP << 4, vel, vel) == 3,
               PHYSICS_set_prop_point_damping(
                   PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP << 4, 0.5f) == 3});

    size_t pin_id = 0;
    size_t weight_id = 0;
    prop_point pin = (prop_point){0};
    prop_point weight = (prop_point){0};
    double pin_x = 16.0f * SPRITESHEET_CELL_X;
    double pin_y = 16.0f * SPRITESHEET_CELL_Y;
    double rope_length = 4.0f * SPRITESHEET_CELL_Y;
    PHYSICS_new_prop_point((prop_point){.x = pin_x, .y = pin_y, .pinned = true},
                           &pin_id);
    PHYSICS_new_prop_point((prop_point){.x = pin_x, .y = pin_y, .acc_y = acc},
                           &weight_id);
    int connect_res = PHYSICS_connect_prop_points(
        pin_id, weight_id, 0.0f, rope_length, false);
    PHYSICS_update();
    PHYSICS_get_prop_point(pin_id, &pin);
    PHYSICS_get_prop_point(weight_id, &weight);
    double dist = SDL_sqrt((weight.x - pin.x) * (weight.x - pin.x) +
                           (weight.y - pin.y) * (weight.y - pin.y));
    expect("Created a pinned point and a point with gravity on the same spot "
           "and connected them with a rope, then ticked once, checking if the "
           "rope let the point fall while still close to the pin...",
           5,
           (bool[]){connect_res == 0,
                    pin.x == pin_x,
                    pin.y == pin_y,
                    weight.y > pin_y,
                    dist < rope_length});

    for (int i = 0; i < 119; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(pin_id, &pin);
    PHYSICS_get_prop_point(weight_id, &weight);
    dist = SDL_sqrt((weight.x - pin.x) * (weight.x - pin.x) +
                    (weight.y - pin.y) * (weight.y - pin.y));
    expect("Ticked enough times for the rope to stretch, checking if the pin "
           "stayed still and the rope holds the point hanging below it...",
           5,
           (bool[]){pin.x == pin_x,
                    pin.y == pin_y,
                    SDL_fabs(dist - rope_length) <= 0.01f,
                    SDL_fabs(weight.x - pin_x) <= 0.01f,
                    weight.y > pin_y});

    double new_pin_x = pin_x + 8.0f * SPRITESHEET_CELL_X;
    double weight_x0 = weight.x;
    PHYSICS_edit_prop_point(
        pin_id, (prop_point){.x = new_pin_x, .y = pin_y, .pinned = true});
    PHYSICS_update();
    PHYSICS_get_prop_point(pin_id, &pin);
    PHYSICS_get_prop_point(weight_id, &weight);
    dist = SDL_sqrt((weight.x - pin.x) * (weight.x - pin.x) +
                    (weight.y - pin.y) * (weight.y - pin.y));
    expect("Moved the pin to the right and ticked once, checking if the pin "
           "stayed where it was put and the rope dragged the point with it...",
           4,
           (bool[]){pin.x == new_pin_x,
                    pin.y == pin_y,
                    weight.x > weight_x0,
                    dist <= rope_length + 0.01f});

    PHYSICS_del_prop_point(weight_id);
    PHYSICS_del_prop_point(pin_id);

    double dt = GAME_MANAGER_get_current_dt();
    double spring_elasticity = 0.001f;
    double spring_length = 4.0f * SPRITESHEET_CELL_X;
    double spring_stretch = 1.0f * SPRITESHEET_CELL_X;
    double spring_x0 = pin_x + spring_length + spring_stretch;
    size_t spring_pin_id = 0;
    size_t spring_point_id = 0;
    prop_point spring_pin = (prop_point){0};
    prop_point spring_point = (prop_point){0};
    PHYSICS_new_prop_point((prop_point){.x = pin_x, .y = pin_y, .pinned = true},
                           &spring_pin_id);
    PHYSICS_new_prop_point((prop_point){.x = spring_x0, .y = pin_y},
                           &spring_point_id);
    connect_res = PHYSICS_connect_prop_points(spring_pin_id,
                                              spring_point_id,
                                              spring_elasticity,
                                              spring_length,
                                              false);
    PHYSICS_update();
    PHYSICS_get_prop_point(spring_pin_id, &spring_pin);
    PHYSICS_get_prop_point(spring_point_id, &spring_point);
    double spring_x1 = spring_point.x;
    double vel_1 = (spring_x1 - spring_x0) / dt;
    double acc_0 = vel_1 / dt;
    double expected_acc_0 = -spring_stretch / spring_elasticity;
    expect(
        "Created a pinned point and a point connected by a stretched one-way "
        "spring and ticked once, checking if the point accelerated towards "
        "the pin by the right amount...",
        5,
        (bool[]){connect_res == 0,
                 spring_pin.x == pin_x,
                 spring_pin.y == pin_y,
                 spring_point.y == pin_y,
                 SDL_fabs(acc_0 - expected_acc_0) <= 0.01f});

    PHYSICS_update();
    PHYSICS_get_prop_point(spring_pin_id, &spring_pin);
    PHYSICS_get_prop_point(spring_point_id, &spring_point);
    double vel_2 = (spring_point.x - spring_x1) / dt;
    double acc_1 = (vel_2 - vel_1) / dt;
    double expected_acc_1 =
        -(spring_x1 - pin_x - spring_length) / spring_elasticity;
    expect("Ticked again, checking if the point's velocity changed by the "
           "spring's acceleration at its new stretch...",
           4,
           (bool[]){spring_pin.x == pin_x,
                    spring_pin.y == pin_y,
                    spring_point.y == pin_y,
                    SDL_fabs(acc_1 - expected_acc_1) <= 0.01f});

    double spring_compressed_x0 = pin_x + spring_length - spring_stretch;
    PHYSICS_edit_prop_point(
        spring_point_id, (prop_point){.x = spring_compressed_x0, .y = pin_y});
    PHYSICS_update();
    PHYSICS_get_prop_point(spring_point_id, &spring_point);
    expect("Moved the point to compress the one-way spring and ticked once, "
           "checking if it stayed still...",
           2,
           (bool[]){spring_point.x == spring_compressed_x0,
                    spring_point.y == pin_y});

    PHYSICS_del_prop_point(spring_point_id);
    PHYSICS_new_prop_point((prop_point){.x = spring_x0, .y = pin_y},
                           &spring_point_id);
    connect_res = PHYSICS_connect_prop_points(
        spring_pin_id, spring_point_id, spring_elasticity, spring_length, true);
    PHYSICS_update();
    PHYSICS_get_prop_point(spring_pin_id, &spring_pin);
    PHYSICS_get_prop_point(spring_point_id, &spring_point);
    spring_x1 = spring_point.x;
    vel_1 = (spring_x1 - spring_x0) / dt;
    acc_0 = vel_1 / dt;
    expected_acc_0 = -spring_stretch / spring_elasticity;
    expect("Replaced the point with one connected by a stretched two-ways "
           "spring and ticked once, checking if the point accelerated towards "
           "the pin by the right amount...",
           5,
           (bool[]){connect_res == 0,
                    spring_pin.x == pin_x,
                    spring_pin.y == pin_y,
                    spring_point.y == pin_y,
                    SDL_fabs(acc_0 - expected_acc_0) <= 0.01f});

    PHYSICS_update();
    PHYSICS_get_prop_point(spring_pin_id, &spring_pin);
    PHYSICS_get_prop_point(spring_point_id, &spring_point);
    vel_2 = (spring_point.x - spring_x1) / dt;
    acc_1 = (vel_2 - vel_1) / dt;
    expected_acc_1 = -(spring_x1 - pin_x - spring_length) / spring_elasticity;
    expect("Ticked again, checking if the point's velocity changed by the "
           "two-ways spring's acceleration at its new stretch...",
           4,
           (bool[]){spring_pin.x == pin_x,
                    spring_pin.y == pin_y,
                    spring_point.y == pin_y,
                    SDL_fabs(acc_1 - expected_acc_1) <= 0.01f});

    PHYSICS_edit_prop_point(
        spring_point_id, (prop_point){.x = spring_compressed_x0, .y = pin_y});
    PHYSICS_update();
    PHYSICS_get_prop_point(spring_pin_id, &spring_pin);
    PHYSICS_get_prop_point(spring_point_id, &spring_point);
    spring_x1 = spring_point.x;
    vel_1 = (spring_x1 - spring_compressed_x0) / dt;
    acc_0 = vel_1 / dt;
    expected_acc_0 = spring_stretch / spring_elasticity;
    expect("Moved the point to compress the two-ways spring and ticked once, "
           "checking if the point accelerated away from the pin by the right "
           "amount...",
           4,
           (bool[]){spring_pin.x == pin_x,
                    spring_pin.y == pin_y,
                    spring_point.y == pin_y,
                    SDL_fabs(acc_0 - expected_acc_0) <= 0.01f});

    PHYSICS_update();
    PHYSICS_get_prop_point(spring_pin_id, &spring_pin);
    PHYSICS_get_prop_point(spring_point_id, &spring_point);
    vel_2 = (spring_point.x - spring_x1) / dt;
    acc_1 = (vel_2 - vel_1) / dt;
    expected_acc_1 = -(spring_x1 - pin_x - spring_length) / spring_elasticity;
    expect("Ticked again, checking if the point's velocity changed by the "
           "two-ways spring's acceleration at its new compression...",
           4,
           (bool[]){spring_pin.x == pin_x,
                    spring_pin.y == pin_y,
                    spring_point.y == pin_y,
                    SDL_fabs(acc_1 - expected_acc_1) <= 0.01f});

    PHYSICS_del_prop_point(spring_point_id);
    PHYSICS_del_prop_point(spring_pin_id);

    size_t chain_ids[3] = {0};
    size_t lone_id = 0;
    for (int i = 0; i < 3; i++)
        PHYSICS_new_prop_point((prop_point){.x = (i + 1) * SPRITESHEET_CELL_X,
                                            .y = SPRITESHEET_CELL_Y},
                               &chain_ids[i]);
    PHYSICS_new_prop_point(
        (prop_point){.x = SPRITESHEET_CELL_X, .y = 2.0f * SPRITESHEET_CELL_Y},
        &lone_id);
    PHYSICS_connect_prop_points(chain_ids[0], chain_ids[1], 0.0f, 1.0f, true);
    PHYSICS_connect_prop_points(chain_ids[1], chain_ids[2], 0.0f, 1.0f, true);
    dynarr connected = (dynarr){0};
    init_dynarr(&connected, sizeof(size_t));
    int conn_res = PHYSICS_get_connected_points(chain_ids[1], &connected);
    bool has_first = false;
    bool has_last = false;
    for (size_t i = 0; i < connected.len; i++) {
        if (*(size_t*)dynarr_at(i, &connected) == chain_ids[0])
            has_first = true;
        if (*(size_t*)dynarr_at(i, &connected) == chain_ids[2]) has_last = true;
    }
    expect("Made a chain of three points, checking the points connected to "
           "the middle one...",
           4,
           (bool[]){conn_res == 0, connected.len == 2, has_first, has_last});

    conn_res = PHYSICS_get_connected_points(chain_ids[0], &connected);
    expect("Checking the points connected to the chain's first point...",
           3,
           (bool[]){conn_res == 0,
                    connected.len == 1,
                    *(size_t*)dynarr_at(0, &connected) == chain_ids[1]});

    conn_res = PHYSICS_get_connected_points(lone_id, &connected);
    expect("Made a point with no connections, checking its connected "
           "points...",
           2,
           (bool[]){conn_res == 0, connected.len == 0});

    PHYSICS_del_prop_point(chain_ids[2]);
    conn_res = PHYSICS_get_connected_points(chain_ids[1], &connected);
    expect("Deleted the chain's last point, checking the points connected to "
           "the middle one...",
           3,
           (bool[]){conn_res == 0,
                    connected.len == 1,
                    *(size_t*)dynarr_at(0, &connected) == chain_ids[0]});

    int deleted_res = PHYSICS_get_connected_points(chain_ids[2], &connected);
    int range_res = PHYSICS_get_connected_points(
        PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP << 4, &connected);
    dynarr uninit = (dynarr){0};
    int uninit_res = PHYSICS_get_connected_points(chain_ids[0], &uninit);
    expect("Getting connected points of a deleted point, of an index out of "
           "range and into an uninitialized dynarr, checking errors...",
           3,
           (bool[]){deleted_res == 1, range_res == 3, uninit_res == 2});

    deinit_dynarr(&connected);
    PHYSICS_del_prop_point(chain_ids[0]);
    PHYSICS_del_prop_point(chain_ids[1]);
    PHYSICS_del_prop_point(lone_id);

    size_t observed_id = 0;
    size_t observed_neighbor_id = 0;
    const size_t observed_arg = 42;
    PHYSICS_new_prop_point((prop_point){.x = SPRITESHEET_CELL_X,
                                        .y = SPRITESHEET_CELL_Y,
                                        .on_delete_func = record_on_delete,
                                        .on_delete_arg = observed_arg},
                           &observed_id);
    PHYSICS_new_prop_point(
        (prop_point){.x = 2.0f * SPRITESHEET_CELL_X, .y = SPRITESHEET_CELL_Y},
        &observed_neighbor_id);
    PHYSICS_connect_prop_points(
        observed_id, observed_neighbor_id, 0.0f, 1.0f, true);
    PHYSICS_del_prop_point(observed_neighbor_id);
    expect("Created a point with an on delete function and deleted a point "
           "connected to it, checking the function wasn't called...",
           1,
           (bool[]){on_delete_calls == 0});

    int del_res = PHYSICS_del_prop_point(observed_id);
    prop_point observed = (prop_point){0};
    int get_res = PHYSICS_get_prop_point(observed_id, &observed);
    expect("Deleted the point, checking if its on delete function was called "
           "once with its ID and on delete arg after it was gone...",
           5,
           (bool[]){del_res == 0,
                    get_res == 1,
                    on_delete_calls == 1,
                    on_delete_point_id == observed_id,
                    on_delete_arg == observed_arg});

    const size_t new_arg = 17;
    PHYSICS_new_prop_point((prop_point){.x = SPRITESHEET_CELL_X,
                                        .y = SPRITESHEET_CELL_Y,
                                        .acc_y = acc},
                           &observed_id);
    for (int i = 0; i < 3; i++)
        PHYSICS_update();
    PHYSICS_get_prop_point(observed_id, &observed);
    double observed_y0 = observed.y;
    PHYSICS_update();
    PHYSICS_get_prop_point(observed_id, &observed);
    double observed_y1 = observed.y;
    int set_res = PHYSICS_set_prop_point_on_delete(
        observed_id, record_on_delete, new_arg);
    PHYSICS_update();
    PHYSICS_get_prop_point(observed_id, &observed);
    double observed_y2 = observed.y;
    expect("Made a falling point without an on delete function and set one "
           "while falling, checking if it kept its velocity...",
           3,
           (bool[]){set_res == 0,
                    observed.on_delete_func == record_on_delete,
                    observed_y2 - observed_y1 > observed_y1 - observed_y0});

    PHYSICS_del_prop_point(observed_id);
    int set_deleted_res =
        PHYSICS_set_prop_point_on_delete(observed_id, record_on_delete, 0);
    int set_range_res = PHYSICS_set_prop_point_on_delete(
        PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP << 4, record_on_delete, 0);
    expect("Deleted the point, checking if the new on delete function was "
           "called with the new arg, and the errors setting on delete of a "
           "deleted point and of an index out of range...",
           5,
           (bool[]){on_delete_calls == 2,
                    on_delete_point_id == observed_id,
                    on_delete_arg == new_arg,
                    set_deleted_res == 1,
                    set_range_res == 3});



    // Simulation space tests, with the camera on the tilemap's top left so the
    // simulation space starts at tile (0,0)
    CAMERA_center_at(0.0f, 0.0f);
    double edge_x = SIMULATION_SPACE_X * SPRITESHEET_CELL_X;
    double body_vel = 4.0f * SPRITESHEET_CELL_X;
    size_t out_body_id = 0;
    size_t edge_body_id = 0;
    PHYSICS_new_body((body){.type = physics_actor,
                            .x = edge_x + 4.0f * SPRITESHEET_CELL_X,
                            .y = 2.0f * SPRITESHEET_CELL_Y,
                            .w = SPRITESHEET_CELL_X,
                            .h = SPRITESHEET_CELL_Y,
                            .vel_x = body_vel},
                     &out_body_id);
    PHYSICS_new_body((body){.type = physics_actor,
                            .x = edge_x - SPRITESHEET_CELL_X / 2.0f,
                            .y = 2.0f * SPRITESHEET_CELL_Y,
                            .w = SPRITESHEET_CELL_X,
                            .h = SPRITESHEET_CELL_Y,
                            .vel_x = -body_vel},
                     &edge_body_id);
    PHYSICS_update();
    body out_body = (body){0};
    body edge_body = (body){0};
    PHYSICS_get_body(out_body_id, &out_body);
    PHYSICS_get_body(edge_body_id, &edge_body);
    bool out_within = true;
    bool edge_within = false;
    int out_within_res =
        PHYSICS_is_body_within_the_simulation(out_body_id, &out_within);
    int edge_within_res =
        PHYSICS_is_body_within_the_simulation(edge_body_id, &edge_within);
    expect("Created a moving body outside the simulation space and another "
           "partially inside it and ticked once, checking if only the one "
           "partially inside moved and is within the simulation...",
           6,
           (bool[]){
               out_body.x == edge_x + 4.0f * SPRITESHEET_CELL_X,
               edge_body.x < edge_x - SPRITESHEET_CELL_X / 2.0f,
               out_within_res == 0,
               edge_within_res == 0,
               out_within == false,
               edge_within == true,
           });

    // Centering the camera on the simulation space's right edge slides it
    // half its width to the right, over the body outside it
    CAMERA_center_at(edge_x, 0.0f);
    PHYSICS_update();
    PHYSICS_get_body(out_body_id, &out_body);
    PHYSICS_is_body_within_the_simulation(out_body_id, &out_within);
    expect("Moved the camera to the right and ticked once, checking if the "
           "body that was outside the simulation space is now simulated...",
           2,
           (bool[]){
               out_body.x > edge_x + 4.0f * SPRITESHEET_CELL_X,
               out_within == true,
           });

    CAMERA_center_at(0.0f, 0.0f);
    PHYSICS_del_body(out_body_id);
    PHYSICS_del_body(edge_body_id);
    expect("Deleted the bodies, checking if they're not within the simulation "
           "anymore, as well as an index out of range, checking errors...",
           3,
           (bool[]){
               PHYSICS_is_body_within_the_simulation(out_body_id,
                                                     &out_within) == 1,
               PHYSICS_is_body_within_the_simulation(edge_body_id,
                                                     &edge_within) == 1,
               PHYSICS_is_body_within_the_simulation(
                   PHYSICS_STARTING_BODIES_DYNARR_CAP << 4, &out_within) == 3,
           });

    size_t out_point_id = 0;
    size_t in_point_id = 0;
    double out_x = edge_x + SPRITESHEET_CELL_X / 2.0f;
    double in_x = edge_x - SPRITESHEET_CELL_X * 1.5f;
    double hang_y = 2.5f * SPRITESHEET_CELL_Y;
    double hang_length = 2.0f * SPRITESHEET_CELL_X;
    PHYSICS_new_prop_point((prop_point){.x = out_x, .y = hang_y, .acc_y = acc},
                           &out_point_id);
    PHYSICS_new_prop_point((prop_point){.x = in_x, .y = hang_y, .acc_y = acc},
                           &in_point_id);
    PHYSICS_connect_prop_points(
        out_point_id, in_point_id, 0.0f, hang_length, false);
    for (int i = 0; i < 30; i++)
        PHYSICS_update();
    prop_point out_point = (prop_point){0};
    prop_point in_point = (prop_point){0};
    PHYSICS_get_prop_point(out_point_id, &out_point);
    PHYSICS_get_prop_point(in_point_id, &in_point);
    bool out_point_within = true;
    PHYSICS_is_prop_point_within_the_simulation(out_point_id,
                                                &out_point_within);
    dist = SDL_sqrt((in_point.x - out_point.x) * (in_point.x - out_point.x) +
                    (in_point.y - out_point.y) * (in_point.y - out_point.y));
    expect("Connected a falling point inside the simulation space to another "
           "outside it with a rope and ticked, checking if the one outside "
           "stayed still and held the one inside like a pin...",
           5,
           (bool[]){
               out_point.x == out_x,
               out_point.y == hang_y,
               in_point.y > hang_y,
               dist <= hang_length + 0.01f,
               out_point_within == false,
           });

    PHYSICS_del_prop_point(out_point_id);
    PHYSICS_del_prop_point(in_point_id);
    expect("Deleted the points, checking if they're not within the simulation "
           "anymore, as well as an index out of range, checking errors...",
           3,
           (bool[]){
               PHYSICS_is_prop_point_within_the_simulation(
                   out_point_id, &out_point_within) == 1,
               PHYSICS_is_prop_point_within_the_simulation(
                   in_point_id, &out_point_within) == 1,
               PHYSICS_is_prop_point_within_the_simulation(
                   PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP << 4,
                   &out_point_within) == 3,
           });

    PHYSICS_stop();
}
