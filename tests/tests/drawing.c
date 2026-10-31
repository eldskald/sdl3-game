#include "tests/drawing.h"
#include "defs.h"
#include "engine/camera.h"
#include "engine/fluids.h"
#include "engine/renderer.h"
#include "engine/sprites.h"
#include "engine/tilemap.h"
#include "stubbed/graphics.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// The camera's top left during the tests, in pixels. Everything is placed
// relative to it, so the expected calls are in screen coords. It's far enough
// from the origin that the simulated fluids rect has to follow it.
#define CAM_X 800
#define CAM_Y 400

// Fluid cells are drawn relative to the ripples area's top left, which is the
// camera's minus these margins, see `FLUIDS_RIPPLES_TEXTURE_RES_FACTOR`.
#define RIPPLES_MARGIN_X                                                       \
    ((FLUIDS_RIPPLES_TEXTURE_RES_FACTOR - 1) * WINDOW_SIZE_X / 2)
#define RIPPLES_MARGIN_Y                                                       \
    ((FLUIDS_RIPPLES_TEXTURE_RES_FACTOR - 1) * WINDOW_SIZE_Y / 2)

static size_t anim_finished_calls = 0;
static size_t anim_finished_sprite_id = 0;
static size_t anim_finished_arg = 0;

static void record_anim_finished(size_t sprite_id, size_t arg) {
    anim_finished_calls++;
    anim_finished_sprite_id = sprite_id;
    anim_finished_arg = arg;
}

// Updates the sprites like the fixed thread would, then renders.
static void tick(void) {
    SPRITES_update();
    RENDERER_update();
}

// A sprite decoded back from its 6 vertices, in pixels on the spritesheet and
// on the screen.
typedef struct {
    SDL_FRect src;
    SDL_FRect dst;
    SDL_FlipMode flip_mode;
} drawn_sprite;

typedef struct {
    SDL_FPoint a, b, c;
} drawn_triangle;

// Reads the vertices drawn in order. Each `read_...()` decodes what's at `at`
// and moves past it, reading zeroes past the end, so checking `at == len`
// after reading everything expected checks nothing else was drawn.
typedef struct {
    graphics_vertex* v;
    graphics_texture* texture;
    size_t len, at;
} vertex_reader;

static vertex_reader read_vertices(void) {
    vertex_reader r = {0};
    r.len = get_GRAPHICS_vertices(&r.v, &r.texture);
    return r;
}

static graphics_vertex* read(vertex_reader* r, size_t n) {
    static graphics_vertex zeroes[6] = {0};
    graphics_vertex* v = r->at + n <= r->len ? &r->v[r->at] : zeroes;
    r->at += n;
    return v;
}

static drawn_sprite read_sprite(vertex_reader* r) {
    graphics_vertex* v = read(r, 6);
    float w = r->texture ? r->texture->w : 0.0f;
    float h = r->texture ? r->texture->h : 0.0f;
    float u0 = v[0].u * w, v0 = v[0].v * h, u1 = v[4].u * w, v1 = v[4].v * h;
    return (drawn_sprite){
        .src = {SDL_roundf(SDL_min(u0, u1)),
                SDL_roundf(SDL_min(v0, v1)),
                SDL_roundf(SDL_fabsf(u1 - u0)),
                SDL_roundf(SDL_fabsf(v1 - v0))},
        .dst = {v[0].x, v[0].y, v[4].x - v[0].x, v[4].y - v[0].y},
        .flip_mode = (u0 > u1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE) |
                     (v0 > v1 ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE),
    };
}

// Reads a pixel of a line, returning its top left.
static SDL_FPoint read_pixel(vertex_reader* r) {
    graphics_vertex* v = read(r, 6);
    return (SDL_FPoint){v[0].x, v[0].y};
}

static drawn_triangle read_triangle(vertex_reader* r) {
    graphics_vertex* v = read(r, 3);
    return (drawn_triangle){
        {v[0].x, v[0].y}, {v[1].x, v[1].y}, {v[2].x, v[2].y}};
}

// Decodes every vertex drawn as sprites, for when only sprites are drawn.
// Points `ptr` to them and returns how many there are.
static size_t get_drawn_sprites(drawn_sprite** ptr) {
    static drawn_sprite sprites[SUB_CALLS_MAX] = {0};
    vertex_reader r = read_vertices();
    size_t n = 0;
    while (r.at < r.len && n < SUB_CALLS_MAX)
        sprites[n++] = read_sprite(&r);
    *ptr = sprites;
    return n;
}

void test_drawing(void) {
    printf(YELLOW "\nTesting render texture calls...\n" RESET);

    RENDERER_start();
    SPRITES_start();
    CAMERA_center_at(CAM_X + WINDOW_SIZE_X / 2, CAM_Y + WINDOW_SIZE_Y / 2);

    size_t id0 = 0;
    size_t id1 = 0;
    size_t id2 = 0;
    size_t id3 = 0;

    SPRITES_new_sprite(
        (sprite){
            .pos_x = CAM_X + 120,
            .pos_y = CAM_Y + 80,
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
            .pos_x = CAM_X + 55,
            .pos_y = CAM_Y + 30,
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
            .pos_x = CAM_X + 75,
            .pos_y = CAM_Y + 20,
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
            .pos_x = CAM_X + 25,
            .pos_y = CAM_Y + 10,
            .coords_x = 2,
            .coords_y = 1,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = -3,
            .flip_mode = SDL_FLIP_HORIZONTAL_AND_VERTICAL,
        },
        &id3);


    tick();
    drawn_sprite* calls = NULL;
    size_t calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();


    SPRITES_edit_sprite(id3,
                        (sprite){
                            .pos_x = CAM_X + 27,
                            .pos_y = CAM_Y + 12,
                            .coords_x = 2,
                            .coords_y = 1,
                            .coords_w = 1,
                            .coords_h = 1,
                            .z_index = 10,
                            .flip_mode = SDL_FLIP_NONE,
                        });
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();


    SPRITES_del_sprite(id0);
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();


    size_t off_left = 0;
    size_t off_bottom = 0;
    size_t partial = 0;
    SPRITES_new_sprite(
        (sprite){
            .pos_x = CAM_X - SPRITESHEET_CELL_X,
            .pos_y = CAM_Y + 40,
            .coords_x = 1,
            .coords_y = 1,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 20,
        },
        &off_left);
    SPRITES_new_sprite(
        (sprite){
            .pos_x = CAM_X + 40,
            .pos_y = CAM_Y + WINDOW_SIZE_Y,
            .coords_x = 1,
            .coords_y = 1,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 20,
        },
        &off_bottom);
    SPRITES_new_sprite(
        (sprite){
            .pos_x = CAM_X - SPRITESHEET_CELL_X / 2,
            .pos_y = CAM_Y + 40,
            .coords_x = 4,
            .coords_y = 2,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 20,
        },
        &partial);
    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("Added 2 sprites just off screen and 1 partially on screen, should "
           "only draw the partial one...",
           10,
           (bool[]){
               calls_n == 4,
               calls[3].src.x == 4 * SPRITESHEET_CELL_X,
               calls[3].src.y == 2 * SPRITESHEET_CELL_Y,
               calls[3].src.w == SPRITESHEET_CELL_X,
               calls[3].src.h == SPRITESHEET_CELL_Y,
               calls[3].dst.x == -SPRITESHEET_CELL_X / 2,
               calls[3].dst.y == 40,
               calls[3].dst.w == SPRITESHEET_CELL_X,
               calls[3].dst.h == SPRITESHEET_CELL_Y,
               calls[3].flip_mode == SDL_FLIP_NONE,
           });
    clear_stubbed_calls();
    SPRITES_del_sprite(off_left);
    SPRITES_del_sprite(off_bottom);
    SPRITES_del_sprite(partial);


    tile data[1][1] = {
        {(tile){.type = tile_stone, .coords_x = 6, .coords_y = 12}}};
    TILEMAP_set_at(CAM_X / SPRITESHEET_CELL_X + 3,
                   CAM_Y / SPRITESHEET_CELL_Y + 2,
                   1,
                   1,
                   &data);
    tick();
    calls_n = get_drawn_sprites(&calls);
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
               calls[3].src.y == 12 * SPRITESHEET_CELL_Y,
               calls[3].src.w == SPRITESHEET_CELL_X,
               calls[3].src.h == SPRITESHEET_CELL_Y,
               calls[3].dst.x == 3 * SPRITESHEET_CELL_X,
               calls[3].dst.y == 2 * SPRITESHEET_CELL_Y,
               calls[3].dst.w == SPRITESHEET_CELL_X,
               calls[3].dst.h == SPRITESHEET_CELL_Y,
               calls[3].flip_mode == SDL_FLIP_NONE,
           });
    clear_stubbed_calls();

    tile platform[1][1] = {{(tile){
        .type = tile_wood, .platform = true, .coords_x = 7, .coords_y = 13}}};
    TILEMAP_set_at(CAM_X / SPRITESHEET_CELL_X + 4,
                   CAM_Y / SPRITESHEET_CELL_Y + 2,
                   1,
                   1,
                   &platform);
    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("Added a platform tile next to the tile, checking if it's drawn "
           "before the sprites and the tile after them...",
           9,
           (bool[]){
               calls_n == 5,
               calls[0].src.x == 7 * SPRITESHEET_CELL_X,
               calls[0].src.y == 13 * SPRITESHEET_CELL_Y,
               calls[0].dst.x == 4 * SPRITESHEET_CELL_X,
               calls[0].dst.y == 2 * SPRITESHEET_CELL_Y,
               calls[1].src.x == 8 * SPRITESHEET_CELL_X,
               calls[3].src.x == 5 * SPRITESHEET_CELL_X,
               calls[4].src.x == 6 * SPRITESHEET_CELL_X,
               calls[4].dst.x == 3 * SPRITESHEET_CELL_X,
           });
    clear_stubbed_calls();
    tile no_platform[1][1] = {{(tile){0}}};
    TILEMAP_set_at(CAM_X / SPRITESHEET_CELL_X + 4,
                   CAM_Y / SPRITESHEET_CELL_Y + 2,
                   1,
                   1,
                   &no_platform);

    // The screen is 22.5 tiles tall, so the tiles on row 22 are partially on
    // screen and the ones on row 23 are off it.
    Uint16 cam_tx = CAM_X / SPRITESHEET_CELL_X;
    Uint16 cam_ty = CAM_Y / SPRITESHEET_CELL_Y;
    Uint16 screen_tw = WINDOW_SIZE_X / SPRITESHEET_CELL_X;
    Uint16 screen_th = WINDOW_SIZE_Y / SPRITESHEET_CELL_Y;
    tile empty[1][1] = {{(tile){0}}};
    TILEMAP_set_at(cam_tx - 1, cam_ty + 2, 1, 1, &data);
    TILEMAP_set_at(cam_tx + screen_tw, cam_ty + 2, 1, 1, &data);
    TILEMAP_set_at(cam_tx + 5, cam_ty - 1, 1, 1, &data);
    TILEMAP_set_at(cam_tx + 5, cam_ty + screen_th + 1, 1, 1, &data);
    TILEMAP_set_at(cam_tx + 6, cam_ty + screen_th, 1, 1, &data);
    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("Added 4 tiles just off screen and 1 partially on screen, should "
           "only draw the partial one...",
           10,
           (bool[]){
               calls_n == 5,
               calls[3].dst.x == 3 * SPRITESHEET_CELL_X,
               calls[3].dst.y == 2 * SPRITESHEET_CELL_Y,
               calls[4].src.x == 6 * SPRITESHEET_CELL_X,
               calls[4].src.y == 12 * SPRITESHEET_CELL_Y,
               calls[4].src.w == SPRITESHEET_CELL_X,
               calls[4].src.h == SPRITESHEET_CELL_Y,
               calls[4].dst.x == 6 * SPRITESHEET_CELL_X,
               calls[4].dst.y == screen_th * SPRITESHEET_CELL_Y,
               calls[4].flip_mode == SDL_FLIP_NONE,
           });
    clear_stubbed_calls();
    TILEMAP_set_at(cam_tx - 1, cam_ty + 2, 1, 1, &empty);
    TILEMAP_set_at(cam_tx + screen_tw, cam_ty + 2, 1, 1, &empty);
    TILEMAP_set_at(cam_tx + 5, cam_ty - 1, 1, 1, &empty);
    TILEMAP_set_at(cam_tx + 5, cam_ty + screen_th + 1, 1, 1, &empty);
    TILEMAP_set_at(cam_tx + 6, cam_ty + screen_th, 1, 1, &empty);

    FLUIDS_start();
    Uint32 cam_cx = CAM_X / FLUIDS_CELL_W;
    Uint32 cam_cy = CAM_Y / FLUIDS_CELL_H;
    FLUIDS_set_at(cam_cx + 2, cam_cy + 2, 1, 1, fluid_water, 1.0f);
    FLUIDS_update();
    tick();
    GRAPHICS_fill_rect_call* rect_calls = NULL;
    size_t rect_calls_n = get_GRAPHICS_fill_rect_calls(&rect_calls);
    SDL_Rect sim = FLUIDS_get_simulated_rect();
    // The tile set earlier at (3,2) is drawn as wall cells after the water. The
    // camera's center is at tile (120,61.25), so the simulated rect, centered
    // on it and snapped to tiles, has its top left at tile (60,28).
    expect("Added a cell of water, checking calls and the simulated rect...",
           25,
           (bool[]){
               sim.x == 60 * FLUIDS_SPLIT_X,
               sim.y == 28 * FLUIDS_SPLIT_Y,
               sim.w == SIMULATION_SPACE_X * FLUIDS_SPLIT_X,
               sim.h == SIMULATION_SPACE_Y * FLUIDS_SPLIT_Y,
               rect_calls_n == 5,
               rect_calls[0].rect.x == 8.0f + RIPPLES_MARGIN_X,
               rect_calls[0].rect.y == 8.0f + RIPPLES_MARGIN_Y,
               rect_calls[0].rect.w == 4.0f,
               rect_calls[0].rect.h == 4.0f,
               rect_calls[1].rect.x == 24.0f + RIPPLES_MARGIN_X,
               rect_calls[1].rect.y == 16.0f + RIPPLES_MARGIN_Y,
               rect_calls[1].rect.w == 4.0f,
               rect_calls[1].rect.h == 4.0f,
               rect_calls[2].rect.x == 24.0f + RIPPLES_MARGIN_X,
               rect_calls[2].rect.y == 20.0f + RIPPLES_MARGIN_Y,
               rect_calls[2].rect.w == 4.0f,
               rect_calls[2].rect.h == 4.0f,
               rect_calls[3].rect.x == 28.0f + RIPPLES_MARGIN_X,
               rect_calls[3].rect.y == 16.0f + RIPPLES_MARGIN_Y,
               rect_calls[3].rect.w == 4.0f,
               rect_calls[3].rect.h == 4.0f,
               rect_calls[4].rect.x == 28.0f + RIPPLES_MARGIN_X,
               rect_calls[4].rect.y == 20.0f + RIPPLES_MARGIN_Y,
               rect_calls[4].rect.w == 4.0f,
               rect_calls[4].rect.h == 4.0f,
           });
    clear_stubbed_calls();

    // The ripples area spans pixels (640,310) to (1280,670), so cells 160 to
    // 319 horizontally and 77 to 167 vertically, rows 77 and 167 only
    // partially. These are all inside the simulated rect, just outside the
    // ripples area, except for the last one, which is inside it but off screen.
    FLUIDS_set_at(159, cam_cy + 2, 1, 1, fluid_water, 1.0f);
    FLUIDS_set_at(320, cam_cy + 2, 1, 1, fluid_water, 1.0f);
    FLUIDS_set_at(cam_cx + 10, 76, 1, 1, fluid_water, 1.0f);
    FLUIDS_set_at(cam_cx + 10, 168, 1, 1, fluid_water, 1.0f);
    FLUIDS_set_at(cam_cx - 2, cam_cy + 2, 1, 1, fluid_water, 1.0f);
    FLUIDS_update();
    tick();
    rect_calls_n = get_GRAPHICS_fill_rect_calls(&rect_calls);
    expect("Added 4 cells of water just outside the ripples area and 1 off "
           "screen inside it, should only draw the one inside it...",
           9,
           (bool[]){
               rect_calls_n == 6,
               rect_calls[0].rect.x == RIPPLES_MARGIN_X - 8.0f,
               rect_calls[0].rect.y == RIPPLES_MARGIN_Y + 8.0f,
               rect_calls[0].rect.w == 4.0f,
               rect_calls[0].rect.h == 4.0f,
               rect_calls[1].rect.x == RIPPLES_MARGIN_X + 8.0f,
               rect_calls[1].rect.y == RIPPLES_MARGIN_Y + 8.0f,
               rect_calls[1].rect.w == 4.0f,
               rect_calls[1].rect.h == 4.0f,
           });
    clear_stubbed_calls();

    // Water particles are under the size threshold and drawn as 1 point, lava
    // ones are over it and drawn as 2x2 points. Particles start at rest, so
    // they only fall a fraction of a pixel per step.
    bool added = true;
    if (FLUIDS_add_particle(
            fluid_water, CAM_X + 60.5f, CAM_Y + 40.1f, 0.0f, 0.0f) != 0)
        added = false;
    if (FLUIDS_add_particle(
            fluid_lava, CAM_X + 100.5f, CAM_Y + 40.1f, 0.0f, 0.0f) != 0)
        added = false;
    FLUIDS_update();
    tick();
    GRAPHICS_draw_point_call* point_calls = NULL;
    size_t point_calls_n = get_GRAPHICS_draw_point_calls(&point_calls);
    expect("Added a particle of water and one of lava, checking calls...",
           12,
           (bool[]){
               added,
               point_calls_n == 5,
               point_calls[0].x == 60.0f,
               point_calls[0].y == 40.0f,
               point_calls[1].x == 100.0f,
               point_calls[1].y == 40.0f,
               point_calls[2].x == 101.0f,
               point_calls[2].y == 40.0f,
               point_calls[3].x == 100.0f,
               point_calls[3].y == 41.0f,
               point_calls[4].x == 101.0f,
               point_calls[4].y == 41.0f,
           });
    clear_stubbed_calls();

    // Inside the simulated rect, but off screen, except for the last one.
    float off_x[4] = {-2.5f, WINDOW_SIZE_X + 0.5f, 60.5f, 60.5f};
    float off_y[4] = {80.1f, 80.1f, -2.9f, WINDOW_SIZE_Y + 0.1f};
    added = true;
    for (int i = 0; i < 4; i++) {
        if (FLUIDS_add_particle(
                fluid_lava, CAM_X + off_x[i], CAM_Y + off_y[i], 0.0f, 0.0f) !=
            0)
            added = false;
    }
    if (FLUIDS_add_particle(
            fluid_lava, CAM_X - 0.5f, CAM_Y + 80.1f, 0.0f, 0.0f) != 0)
        added = false;
    FLUIDS_update();
    tick();
    point_calls_n = get_GRAPHICS_draw_point_calls(&point_calls);
    expect("Added 4 particles of lava just off screen and 1 partially on "
           "screen, should only draw the partial one...",
           12,
           (bool[]){
               added,
               point_calls_n == 9,
               point_calls[0].x == 60.0f,
               point_calls[0].y == 40.0f,
               point_calls[5].x == -1.0f,
               point_calls[5].y == 80.0f,
               point_calls[6].x == 0.0f,
               point_calls[6].y == 80.0f,
               point_calls[7].x == -1.0f,
               point_calls[7].y == 81.0f,
               point_calls[8].x == 0.0f,
               point_calls[8].y == 81.0f,
           });
    clear_stubbed_calls();

    TILEMAP_clear();
    FLUIDS_stop();
    SPRITES_stop();
    SPRITES_start();
    const size_t anim_arg = 42;
    SPRITES_new_sprite(
        (sprite){
            .pos_x = CAM_X,
            .pos_y = CAM_Y,
            .coords_x = 0,
            .coords_y = 0,
            .coords_w = 1,
            .coords_h = 1,
            .z_index = 0,
            .on_anim_finished_func = record_anim_finished,
            .on_anim_finished_arg = anim_arg,
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

    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();

    for (int i = 0; i < 14; i++)
        tick();
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();

    for (int i = 0; i < 14; i++)
        tick();
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();

    for (int i = 0; i < 12; i++)
        tick();
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("Advanced to the second to last tick to check that no animation "
           "finished function was called and the calls...",
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
                    anim_finished_calls == 0});
    clear_stubbed_calls();

    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("Ticked enough to get to the end of the animation, checking calls "
           "and if the animation finished function was called once with the "
           "sprite's ID and arg...",
           12,
           (bool[]){calls_n == 1,
                    calls[0].src.x == 3 * SPRITESHEET_CELL_X,
                    calls[0].src.y == 0,
                    calls[0].src.w == SPRITESHEET_CELL_X,
                    calls[0].src.h == SPRITESHEET_CELL_Y,
                    calls[0].dst.x == 0,
                    calls[0].dst.y == 0,
                    calls[0].dst.w == SPRITESHEET_CELL_X,
                    calls[0].dst.h == SPRITESHEET_CELL_Y,
                    anim_finished_calls == 1,
                    anim_finished_sprite_id == id0,
                    anim_finished_arg == anim_arg});
    clear_stubbed_calls();

    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("One more tick to see if it remains on the same frame and no end of "
           "animation finished function call, checking calls...",
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
                    anim_finished_calls == 1});
    clear_stubbed_calls();

    SPRITES_play_anim(
        id0,
        (animation){
            .looped = true,
            .frames = {
                (keyframe){.coords_x = 1, .coords_y = 0, .duration = 0.25f},
                (keyframe){.coords_x = 2, .coords_y = 0, .duration = 0.25f},
                (keyframe){.coords_x = 3, .coords_y = 0, .duration = 0.25f},
            }});
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();

    for (int i = 0; i < 14; i++)
        tick();
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();

    for (int i = 0; i < 14; i++)
        tick();
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();

    for (int i = 0; i < 14; i++)
        tick();
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("Ticked enough to loop back to the first, checking calls and if no "
           "animation finished function was called...",
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
                    anim_finished_calls == 1});
    clear_stubbed_calls();

    SPRITES_stop_anim(id0);
    for (int i = 0; i < 14; i++)
        tick();
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
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
    clear_stubbed_calls();

    animation short_anim = (animation){
        .looped = false,
        .frames = {
            (keyframe){.coords_x = 1, .coords_y = 0, .duration = 0.25f},
            (keyframe){.coords_x = 2, .coords_y = 0, .duration = 0.25f},
        }};
    SPRITES_play_anim(id0, short_anim);
    for (int i = 0; i < 20; i++)
        tick();
    SPRITES_stop_anim(id0);
    for (int i = 0; i < 30; i++)
        tick();
    SPRITES_play_anim(id0, short_anim);
    for (int i = 0; i < 20; i++)
        tick();
    SPRITES_play_anim(id0, short_anim);
    for (int i = 0; i < 20; i++)
        tick();
    expect("Stopped a non-looping animation and interrupted another with a new "
           "one before they ended, checking if the animation finished function "
           "wasn't called...",
           1,
           (bool[]){anim_finished_calls == 1});
    clear_stubbed_calls();

    // The simulation space is centered on the camera, ending
    // `SIMULATION_SPACE_X / 2` tiles to the right of its center
    SDL_Rect space = CAMERA_get_simulation_rect();
    Uint32 space_r = (space.x + space.w) * SPRITESHEET_CELL_X;
    size_t out_id = 0;
    size_t edge_id = 0;
    SPRITES_new_sprite(
        (sprite){
            .pos_x = space_r + SPRITESHEET_CELL_X,
            .pos_y = CAM_Y,
            .coords_x = 0,
            .coords_y = 0,
            .coords_w = 1,
            .coords_h = 1,
        },
        &out_id);
    SPRITES_new_sprite(
        (sprite){
            .pos_x = space_r - SPRITESHEET_CELL_X / 2,
            .pos_y = CAM_Y,
            .coords_x = 0,
            .coords_y = 0,
            .coords_w = 1,
            .coords_h = 1,
        },
        &edge_id);
    SPRITES_play_anim(out_id, short_anim);
    SPRITES_play_anim(edge_id, short_anim);
    tick();
    sprite out_sprite = (sprite){0};
    sprite edge_sprite = (sprite){0};
    SPRITES_get_sprite(out_id, &out_sprite);
    SPRITES_get_sprite(edge_id, &edge_sprite);
    calls_n = get_drawn_sprites(&calls);
    expect("Animated a sprite outside the simulation space and another "
           "partially inside it, both off screen, and ticked once, checking if "
           "only the one partially inside is animating and neither was "
           "drawn...",
           3,
           (bool[]){
               out_sprite.coords_x == 0,
               edge_sprite.coords_x == 1,
               calls_n == 1,
           });
    clear_stubbed_calls();
    SPRITES_del_sprite(out_id);
    SPRITES_del_sprite(edge_id);

    // The sprite at the camera's top left is still there on z_index 0
    SDL_FColor white = (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f};
    RENDERER_draw_from_spritesheet(10, 10, 0, 0, 1, 1, 0, white, 3);
    RENDERER_draw_line(1, 2, 3, 4, white, -5);
    RENDERER_draw_from_spritesheet(20, 20, 0, 0, 1, 1, 0, white, 3);
    RENDERER_draw_from_spritesheet(30, 30, 0, 0, 1, 1, 0, white, -5);
    RENDERER_draw_from_spritesheet(
        40, 40, 0, 0, 1, 1, 0, white, FLUIDS_Z_INDEX);
    tick();
    vertex_reader r = read_vertices();
    SDL_FPoint line0 = read_pixel(&r);
    SDL_FPoint line1 = read_pixel(&r);
    SDL_FPoint line2 = read_pixel(&r);
    float dst_x[5] = {0};
    for (int i = 0; i < 5; i++)
        dst_x[i] = read_sprite(&r).dst.x;
    expect("Queued draw calls on several z_indexes, checking if they're drawn "
           "in z_index order and in queue order within the same z_index, the "
           "line pixel by pixel...",
           12,
           (bool[]){
               r.at == r.len,
               line0.x == 1,
               line0.y == 2,
               line1.x == 2,
               line1.y == 3,
               line2.x == 3,
               line2.y == 4,
               dst_x[0] == 30,
               dst_x[1] == 0,
               dst_x[2] == 10,
               dst_x[3] == 20,
               dst_x[4] == 40,
           });
    clear_stubbed_calls();
    tick();
    calls_n = get_drawn_sprites(&calls);
    expect("Rendered again, checking if the queues were emptied...",
           1,
           (bool[]){calls_n == 1});
    clear_stubbed_calls();

    RENDERER_draw_line(7, 9, 7, 9, white, 0);
    tick();
    r = read_vertices();
    SDL_FPoint pixel = read_pixel(&r);
    read_sprite(&r);
    expect("Queued a line from a point to itself, checking if it's drawn as a "
           "single pixel...",
           3,
           (bool[]){
               r.at == r.len,
               pixel.x == 7,
               pixel.y == 9,
           });
    clear_stubbed_calls();

    int attach_r0 = SPRITES_attach_triangle_to_sprite(id0,
                                                      CAM_X + 1,
                                                      CAM_Y + 2,
                                                      CAM_X + 3,
                                                      CAM_Y + 4,
                                                      CAM_X + 1,
                                                      CAM_Y + 4,
                                                      white);
    int attach_r1 = SPRITES_attach_triangle_to_sprite(id0,
                                                      CAM_X + 5,
                                                      CAM_Y + 6,
                                                      CAM_X + 7,
                                                      CAM_Y + 8,
                                                      CAM_X + 5,
                                                      CAM_Y + 8,
                                                      white);
    int attach_r2 =
        SPRITES_attach_triangle_to_sprite(id1, 0, 0, 1, 1, 0, 1, white);
    int attach_r3 =
        SPRITES_attach_triangle_to_sprite(1 << 20, 0, 0, 1, 1, 0, 1, white);
    int attach_r4 = SPRITES_attach_line_to_sprite(
        id0, CAM_X + 2, CAM_Y + 3, CAM_X + 6, CAM_Y + 7, white);
    int attach_r5 = SPRITES_attach_line_to_sprite(id1, 0, 0, 1, 1, white);
    int attach_r6 = SPRITES_attach_line_to_sprite(1 << 20, 0, 0, 1, 1, white);
    tick();
    r = read_vertices();
    drawn_triangle tri0 = read_triangle(&r);
    drawn_triangle tri1 = read_triangle(&r);
    SDL_FPoint line_from = read_pixel(&r);
    for (int i = 0; i < 3; i++)
        read_pixel(&r);
    SDL_FPoint line_to = read_pixel(&r);
    read_sprite(&r);
    expect(
        "Attached 2 triangles and a line to a sprite, checking if they're "
        "drawn in order with it and the error codes...",
        22,
        (bool[]){
            attach_r0 == 0,   attach_r1 == 0,   attach_r2 == 1, attach_r3 == 3,
            attach_r4 == 0,   attach_r5 == 1,   attach_r6 == 3, r.at == r.len,
            line_from.x == 2, line_from.y == 3, line_to.x == 6, line_to.y == 7,
            tri0.a.x == 1,    tri0.a.y == 2,    tri0.b.x == 3,  tri0.b.y == 4,
            tri0.c.x == 1,    tri0.c.y == 4,    tri1.a.x == 5,  tri1.a.y == 6,
            tri1.b.x == 7,    tri1.c.y == 8,
        });
    clear_stubbed_calls();

    int clear_r0 = SPRITES_clear_attached_shapes(id0);
    int clear_r1 = SPRITES_clear_attached_shapes(id1);
    int clear_r2 = SPRITES_clear_attached_shapes(1 << 20);
    tick();
    r = read_vertices();
    read_sprite(&r);
    expect("Cleared the sprite's attached lines and triangles, checking if "
           "only the sprite is drawn and the error codes...",
           4,
           (bool[]){
               clear_r0 == 0,
               clear_r1 == 1,
               clear_r2 == 3,
               r.at == r.len,
           });
    clear_stubbed_calls();

    size_t off_id = 0;
    SPRITES_new_sprite(
        (sprite){
            .pos_x = CAM_X - 4 * SPRITESHEET_CELL_X,
            .pos_y = CAM_Y + 40,
            .coords_x = 1,
            .coords_y = 1,
            .coords_w = 1,
            .coords_h = 1,
        },
        &off_id);
    SPRITES_attach_triangle_to_sprite(off_id,
                                      CAM_X - 3 * SPRITESHEET_CELL_X,
                                      CAM_Y + 44,
                                      CAM_X + 10,
                                      CAM_Y + 44,
                                      CAM_X + 10,
                                      CAM_Y + 46,
                                      white);
    tick();
    r = read_vertices();
    read_sprite(&r);
    drawn_triangle tri = read_triangle(&r);
    drawn_sprite off = read_sprite(&r);
    expect("Added a sprite off screen with an attached triangle reaching on "
           "screen, checking if both are drawn...",
           4,
           (bool[]){
               r.at == r.len,
               tri.b.x == 10,
               tri.c.y == 46,
               off.dst.x == -4 * SPRITESHEET_CELL_X,
           });
    clear_stubbed_calls();

    SPRITES_clear_attached_shapes(off_id);
    tick();
    r = read_vertices();
    read_sprite(&r);
    expect("Cleared its attached triangle, checking if neither is drawn "
           "anymore...",
           1,
           (bool[]){r.at == r.len});
    clear_stubbed_calls();
    SPRITES_del_sprite(off_id);

    SPRITES_new_sprite(
        (sprite){
            .pos_x = space_r + SPRITESHEET_CELL_X,
            .pos_y = CAM_Y,
            .coords_x = 0,
            .coords_y = 0,
            .coords_w = 1,
            .coords_h = 1,
        },
        &out_id);
    SPRITES_attach_triangle_to_sprite(out_id,
                                      space_r + SPRITESHEET_CELL_X,
                                      CAM_Y,
                                      space_r - 1,
                                      CAM_Y,
                                      space_r - 1,
                                      CAM_Y + 1,
                                      white);
    SPRITES_play_anim(out_id, short_anim);
    tick();
    SPRITES_get_sprite(out_id, &out_sprite);
    expect("Added a sprite outside the simulation space with an attached "
           "triangle reaching inside it, checking if it's animating...",
           1,
           (bool[]){out_sprite.coords_x == 1});
    clear_stubbed_calls();
    SPRITES_del_sprite(out_id);

    CAMERA_center_at(WINDOW_SIZE_X / 2, WINDOW_SIZE_Y / 2);
    SPRITES_stop();
    RENDERER_stop();
}
