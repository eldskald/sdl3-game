#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <defs.h>
#include <engine/renderer.h>
#include <engine/sprites.h>
#include <engine/tilemap.h>
#include <stdio.h>
#include <stubs.h>
#include <tests.h>
#include <tests/drawing.h>

void test_drawing(void) {
    printf(YELLOW "\nTesting render texture calls...\n" RESET);

    RENDERER_start();

    size_t id0 = 0;
    size_t id1 = 0;
    size_t id2 = 0;
    size_t id3 = 0;

    SPRITES_new_sprite(
        (sprite){
            .pos_x = 120,
            .pos_y = 80,
            .coords_x = 3,
            .coords_y = 10,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 5,
            .flip_mode = SDL_FLIP_NONE,
        },
        &id0);
    SPRITES_new_sprite(
        (sprite){
            .pos_x = 55,
            .pos_y = 30,
            .coords_x = 8,
            .coords_y = 5,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 4,
            .flip_mode = SDL_FLIP_HORIZONTAL,
        },
        &id1);
    SPRITES_new_sprite(
        (sprite){
            .pos_x = 75,
            .pos_y = 20,
            .coords_x = 5,
            .coords_y = 6,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 11,
            .flip_mode = SDL_FLIP_VERTICAL,
        },
        &id2);
    SPRITES_new_sprite(
        (sprite){
            .pos_x = 25,
            .pos_y = 10,
            .coords_x = 2,
            .coords_y = 1,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = -3,
            .flip_mode = SDL_FLIP_HORIZONTAL_AND_VERTICAL,
        },
        &id3);


    RENDERER_update();
    SDL_RenderTexture_call* calls = NULL;
    int calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Added 4 sprites, should draw them correctly...",
           37,
           (bool[]){
               calls_n == 4,
               calls[0].src.x == 2 * SPRITESHEET_CELL_X,
               calls[0].src.y == 1 * SPRITESHEET_CELL_Y,
               calls[0].src.w == SPRITESHEET_CELL_X,
               calls[0].src.h == SPRITESHEET_CELL_Y,
               calls[0].dst.x == 25,
               calls[0].dst.y == 10,
               calls[0].dst.w == SPRITESHEET_CELL_X,
               calls[0].dst.h == SPRITESHEET_CELL_Y,
               calls[0].flip_mode == SDL_FLIP_HORIZONTAL_AND_VERTICAL,
               calls[1].src.x == 8 * SPRITESHEET_CELL_X,
               calls[1].src.y == 5 * SPRITESHEET_CELL_Y,
               calls[1].src.w == SPRITESHEET_CELL_X,
               calls[1].src.h == SPRITESHEET_CELL_Y,
               calls[1].dst.x == 55,
               calls[1].dst.y == 30,
               calls[1].dst.w == SPRITESHEET_CELL_X,
               calls[1].dst.h == SPRITESHEET_CELL_Y,
               calls[1].flip_mode == SDL_FLIP_HORIZONTAL,
               calls[2].src.x == 3 * SPRITESHEET_CELL_X,
               calls[2].src.y == 10 * SPRITESHEET_CELL_Y,
               calls[2].src.w == SPRITESHEET_CELL_X,
               calls[2].src.h == SPRITESHEET_CELL_Y,
               calls[2].dst.x == 120,
               calls[2].dst.y == 80,
               calls[2].dst.w == SPRITESHEET_CELL_X,
               calls[2].dst.h == SPRITESHEET_CELL_Y,
               calls[2].flip_mode == SDL_FLIP_NONE,
               calls[3].src.x == 5 * SPRITESHEET_CELL_X,
               calls[3].src.y == 6 * SPRITESHEET_CELL_Y,
               calls[3].src.w == SPRITESHEET_CELL_X,
               calls[3].src.h == SPRITESHEET_CELL_Y,
               calls[3].dst.x == 75,
               calls[3].dst.y == 20,
               calls[3].dst.w == SPRITESHEET_CELL_X,
               calls[3].dst.h == SPRITESHEET_CELL_Y,
               calls[3].flip_mode == SDL_FLIP_VERTICAL,
           });
    clear_SDL_RenderTexture_calls();


    SPRITES_update_sprite(id3,
                          (sprite){
                              .pos_x = 27,
                              .pos_y = 12,
                              .coords_x = 2,
                              .coords_y = 1,
                              .coords_w = 1,
                              .coords_h = 1,
                              .z_index = 10,
                              .flip_mode = SDL_FLIP_NONE,
                          });
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Updated a sprite, checking calls...",
           37,
           (bool[]){
               calls_n == 4,
               calls[0].src.x == 8 * SPRITESHEET_CELL_X,
               calls[0].src.y == 5 * SPRITESHEET_CELL_Y,
               calls[0].src.w == SPRITESHEET_CELL_X,
               calls[0].src.h == SPRITESHEET_CELL_Y,
               calls[0].dst.x == 55,
               calls[0].dst.y == 30,
               calls[0].dst.w == SPRITESHEET_CELL_X,
               calls[0].dst.h == SPRITESHEET_CELL_Y,
               calls[0].flip_mode == SDL_FLIP_HORIZONTAL,
               calls[1].src.x == 3 * SPRITESHEET_CELL_X,
               calls[1].src.y == 10 * SPRITESHEET_CELL_Y,
               calls[1].src.w == SPRITESHEET_CELL_X,
               calls[1].src.h == SPRITESHEET_CELL_Y,
               calls[1].dst.x == 120,
               calls[1].dst.y == 80,
               calls[1].dst.w == SPRITESHEET_CELL_X,
               calls[1].dst.h == SPRITESHEET_CELL_Y,
               calls[1].flip_mode == SDL_FLIP_NONE,
               calls[2].src.x == 2 * SPRITESHEET_CELL_X,
               calls[2].src.y == 1 * SPRITESHEET_CELL_Y,
               calls[2].src.w == SPRITESHEET_CELL_X,
               calls[2].src.h == SPRITESHEET_CELL_Y,
               calls[2].dst.x == 27,
               calls[2].dst.y == 12,
               calls[2].dst.w == SPRITESHEET_CELL_X,
               calls[2].dst.h == SPRITESHEET_CELL_Y,
               calls[2].flip_mode == SDL_FLIP_NONE,
               calls[3].src.x == 5 * SPRITESHEET_CELL_X,
               calls[3].src.y == 6 * SPRITESHEET_CELL_Y,
               calls[3].src.w == SPRITESHEET_CELL_X,
               calls[3].src.h == SPRITESHEET_CELL_Y,
               calls[3].dst.x == 75,
               calls[3].dst.y == 20,
               calls[3].dst.w == SPRITESHEET_CELL_X,
               calls[3].dst.h == SPRITESHEET_CELL_Y,
               calls[3].flip_mode == SDL_FLIP_VERTICAL,
           });
    clear_SDL_RenderTexture_calls();


    SPRITES_del_sprite(id0);
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Deleted a sprite, checking calls...",
           28,
           (bool[]){
               calls_n == 3,
               calls[0].src.x == 8 * SPRITESHEET_CELL_X,
               calls[0].src.y == 5 * SPRITESHEET_CELL_Y,
               calls[0].src.w == SPRITESHEET_CELL_X,
               calls[0].src.h == SPRITESHEET_CELL_Y,
               calls[0].dst.x == 55,
               calls[0].dst.y == 30,
               calls[0].dst.w == SPRITESHEET_CELL_X,
               calls[0].dst.h == SPRITESHEET_CELL_Y,
               calls[0].flip_mode == SDL_FLIP_HORIZONTAL,
               calls[1].src.x == 2 * SPRITESHEET_CELL_X,
               calls[1].src.y == 1 * SPRITESHEET_CELL_Y,
               calls[1].src.w == SPRITESHEET_CELL_X,
               calls[1].src.h == SPRITESHEET_CELL_Y,
               calls[1].dst.x == 27,
               calls[1].dst.y == 12,
               calls[1].dst.w == SPRITESHEET_CELL_X,
               calls[1].dst.h == SPRITESHEET_CELL_Y,
               calls[1].flip_mode == SDL_FLIP_NONE,
               calls[2].src.x == 5 * SPRITESHEET_CELL_X,
               calls[2].src.y == 6 * SPRITESHEET_CELL_Y,
               calls[2].src.w == SPRITESHEET_CELL_X,
               calls[2].src.h == SPRITESHEET_CELL_Y,
               calls[2].dst.x == 75,
               calls[2].dst.y == 20,
               calls[2].dst.w == SPRITESHEET_CELL_X,
               calls[2].dst.h == SPRITESHEET_CELL_Y,
               calls[2].flip_mode == SDL_FLIP_VERTICAL,
           });
    clear_SDL_RenderTexture_calls();


    tile data[1][1] = {
        {(tile){.mat = tile_stone, .coords_x = 6, .coords_y = 7}}};
    TILEMAP_set_at(3, 2, 1, 1, &data);
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Added a tile on the tilemap, checking calls...",
           37,
           (bool[]){
               calls_n == 4,
               calls[0].src.x == 8 * SPRITESHEET_CELL_X,
               calls[0].src.y == 5 * SPRITESHEET_CELL_Y,
               calls[0].src.w == SPRITESHEET_CELL_X,
               calls[0].src.h == SPRITESHEET_CELL_Y,
               calls[0].dst.x == 55,
               calls[0].dst.y == 30,
               calls[0].dst.w == SPRITESHEET_CELL_X,
               calls[0].dst.h == SPRITESHEET_CELL_Y,
               calls[0].flip_mode == SDL_FLIP_HORIZONTAL,
               calls[1].src.x == 2 * SPRITESHEET_CELL_X,
               calls[1].src.y == 1 * SPRITESHEET_CELL_Y,
               calls[1].src.w == SPRITESHEET_CELL_X,
               calls[1].src.h == SPRITESHEET_CELL_Y,
               calls[1].dst.x == 27,
               calls[1].dst.y == 12,
               calls[1].dst.w == SPRITESHEET_CELL_X,
               calls[1].dst.h == SPRITESHEET_CELL_Y,
               calls[1].flip_mode == SDL_FLIP_NONE,
               calls[2].src.x == 5 * SPRITESHEET_CELL_X,
               calls[2].src.y == 6 * SPRITESHEET_CELL_Y,
               calls[2].src.w == SPRITESHEET_CELL_X,
               calls[2].src.h == SPRITESHEET_CELL_Y,
               calls[2].dst.x == 75,
               calls[2].dst.y == 20,
               calls[2].dst.w == SPRITESHEET_CELL_X,
               calls[2].dst.h == SPRITESHEET_CELL_Y,
               calls[2].flip_mode == SDL_FLIP_VERTICAL,
               calls[3].src.x == 6 * SPRITESHEET_CELL_X,
               calls[3].src.y == 7 * SPRITESHEET_CELL_Y,
               calls[3].src.w == SPRITESHEET_CELL_X,
               calls[3].src.h == SPRITESHEET_CELL_Y,
               calls[3].dst.x == 3 * SPRITESHEET_CELL_X,
               calls[3].dst.y == 2 * SPRITESHEET_CELL_Y,
               calls[3].dst.w == SPRITESHEET_CELL_X,
               calls[3].dst.h == SPRITESHEET_CELL_Y,
               calls[3].flip_mode == SDL_FLIP_NONE,
           });
    clear_SDL_RenderTexture_calls();


    SPRITES_stop();
    TILEMAP_clear();
    SPRITES_start();
    SPRITES_new_sprite(
        (sprite){
            .pos_x = 0,
            .pos_y = 0,
            .coords_x = 0,
            .coords_y = 0,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 0,
        },
        &id0);
    SPRITES_play_anim(
        id0,
        (animation){
            .looped = false,
            .frames = {
                (keyframe){.coords_x = 1, .coords_y = 0, .duration = 0.25f},
                (keyframe){.coords_x = 2, .coords_y = 0, .duration = 0.25f},
                (keyframe){.coords_x = 3, .coords_y = 0, .duration = 0.25f},
            }});

    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Reset the renderer, added a new sprite with a non-looping "
           "animation, checking calls...",
           9,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 1 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y});
    clear_SDL_RenderTexture_calls();

    for (int i = 0; i < 14; i++)
        RENDERER_update();
    clear_SDL_RenderTexture_calls();
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect(
        "Ticked enough to get to the next animation frame, checking calls...",
        9,
        (bool[]){calls_n == 1,
                 calls[0].src.x == 2 * SPRITESHEET_CELL_X,
                 calls[0].src.y == 0,
                 calls[0].src.w == SPRITESHEET_CELL_X,
                 calls[0].src.h == SPRITESHEET_CELL_Y,
                 calls[0].dst.x == 0,
                 calls[0].dst.y == 0,
                 calls[0].dst.w == SPRITESHEET_CELL_X,
                 calls[0].dst.h == SPRITESHEET_CELL_Y});
    clear_SDL_RenderTexture_calls();

    for (int i = 0; i < 14; i++)
        RENDERER_update();
    clear_SDL_RenderTexture_calls();
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect(
        "Ticked enough to get to the last animation frame, checking calls...",
        9,
        (bool[]){calls_n == 1,
                 calls[0].src.x == 3 * SPRITESHEET_CELL_X,
                 calls[0].src.y == 0,
                 calls[0].src.w == SPRITESHEET_CELL_X,
                 calls[0].src.h == SPRITESHEET_CELL_Y,
                 calls[0].dst.x == 0,
                 calls[0].dst.y == 0,
                 calls[0].dst.w == SPRITESHEET_CELL_X,
                 calls[0].dst.h == SPRITESHEET_CELL_Y});
    clear_SDL_RenderTexture_calls();

    for (int i = 0; i < 12; i++)
        RENDERER_update();
    clear_SDL_RenderTexture_calls();
    RENDERER_update();
    bool trigger = false;
    SPRITES_anim_finished(id0, &trigger);
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Advanced to the second to last tick to check that no animation "
           "finished trigger was sent and the calls...",
           9,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 3 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y,
                    !trigger});
    clear_SDL_RenderTexture_calls();

    RENDERER_update();
    SPRITES_anim_finished(id0, &trigger);
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Ticked enough to get to the end of the animation, checking calls "
           "and the animation finished trigger...",
           10,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 3 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y,
                    trigger});
    clear_SDL_RenderTexture_calls();

    RENDERER_update();
    SPRITES_anim_finished(id0, &trigger);
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("One more tick to see if it remains on the same frame and no end of "
           "animation triggers, checking calls...",
           9,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 3 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y,
                    !trigger});
    clear_SDL_RenderTexture_calls();

    SPRITES_play_anim(
        id0,
        (animation){
            .looped = true,
            .frames = {
                (keyframe){.coords_x = 1, .coords_y = 0, .duration = 0.25f},
                (keyframe){.coords_x = 2, .coords_y = 0, .duration = 0.25f},
                (keyframe){.coords_x = 3, .coords_y = 0, .duration = 0.25f},
            }});
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Play looping animation, checking initial calls...",
           9,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 1 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y});
    clear_SDL_RenderTexture_calls();

    for (int i = 0; i < 14; i++)
        RENDERER_update();
    clear_SDL_RenderTexture_calls();
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect(
        "Ticked enough to get to the next animation frame, checking calls...",
        9,
        (bool[]){calls_n == 1,
                 calls[0].src.x == 2 * SPRITESHEET_CELL_X,
                 calls[0].src.y == 0,
                 calls[0].src.w == SPRITESHEET_CELL_X,
                 calls[0].src.h == SPRITESHEET_CELL_Y,
                 calls[0].dst.x == 0,
                 calls[0].dst.y == 0,
                 calls[0].dst.w == SPRITESHEET_CELL_X,
                 calls[0].dst.h == SPRITESHEET_CELL_Y});
    clear_SDL_RenderTexture_calls();

    for (int i = 0; i < 14; i++)
        RENDERER_update();
    clear_SDL_RenderTexture_calls();
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect(
        "Ticked enough to get to the last animation frame, checking calls...",
        9,
        (bool[]){calls_n == 1,
                 calls[0].src.x == 3 * SPRITESHEET_CELL_X,
                 calls[0].src.y == 0,
                 calls[0].src.w == SPRITESHEET_CELL_X,
                 calls[0].src.h == SPRITESHEET_CELL_Y,
                 calls[0].dst.x == 0,
                 calls[0].dst.y == 0,
                 calls[0].dst.w == SPRITESHEET_CELL_X,
                 calls[0].dst.h == SPRITESHEET_CELL_Y});
    clear_SDL_RenderTexture_calls();

    for (int i = 0; i < 14; i++)
        RENDERER_update();
    clear_SDL_RenderTexture_calls();
    RENDERER_update();
    SPRITES_anim_finished(id0, &trigger);
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Ticked enough to loop back to the first, checking calls and if no "
           "animation trigger was sent...",
           10,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 1 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y,
                    !trigger});
    clear_SDL_RenderTexture_calls();

    SPRITES_stop_anim(id0);
    for (int i = 0; i < 14; i++)
        RENDERER_update();
    clear_SDL_RenderTexture_calls();
    RENDERER_update();
    calls_n = get_SDL_RenderTexture_calls(&calls);
    expect("Stopped animation and ticked enough to get to what would be the "
           "next frame, checking calls...",
           9,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 1 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y});
    clear_SDL_RenderTexture_calls();


    RENDERER_stop();
}
