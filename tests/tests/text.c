#include "engine/text.h"
#include "defs.h"
#include "engine/renderer.h"
#include "stubbed/graphics.h"
#include "tests.h"
#include "tests/text.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// The `src,dst` rects of a glyph decoded back from its 6 vertices, in pixels on
// the atlas and on the screen.
typedef struct {
    SDL_FRect src, dst;
} drawn_glyph;

static drawn_glyph read_glyph(graphics_vertex* v, graphics_texture* texture) {
    float u0 = v[0].u * texture->w, v0 = v[0].v * texture->h;
    float u1 = v[4].u * texture->w, v1 = v[4].v * texture->h;
    return (drawn_glyph){
        .src = {SDL_roundf(u0),
                SDL_roundf(v0),
                SDL_roundf(u1 - u0),
                SDL_roundf(v1 - v0)},
        .dst = {v[0].x, v[0].y, v[4].x - v[0].x, v[4].y - v[0].y},
    };
}

static bool rect_eq(SDL_FRect a, SDL_FRect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void test_text(void) {
    printf(YELLOW "\nTesting text...\n" RESET);

    RENDERER_start();
    TEXT_start();
    clear_stubbed_calls();

    int w, h;
    int uh = TEXT_get_font_height(font_unifont);
    int mh = TEXT_get_font_height(font_able_5);
    expect("Checking the heights of Unifont and the mock font...",
           2,
           (bool[]){uh == 16, mh == 4});

    TEXT_measure("", font_unifont, 0, &w, &h);
    expect("Measuring an empty string...", 2, (bool[]){w == 0, h == 0});
    TEXT_measure("Hello", font_unifont, 0, &w, &h);
    expect("Measuring Hello with Unifont, all 8 pixels wide...",
           2,
           (bool[]){w == 40, h == uh});
    TEXT_measure("A\xc3\xa7\xc3\xa3o\nAA\nA", font_unifont, 0, &w, &h);
    expect("Measuring Ação\\nAA\\nA with Unifont, with accented letters and "
           "line breaks...",
           2,
           (bool[]){w == 32, h == 3 * uh});
    TEXT_measure("A\xc3\xa7\nA\nA", font_unifont, 3, &w, &h);
    expect("Measuring Aç\\nA\\nA with Unifont and 3 pixels of line spacing...",
           2,
           (bool[]){w == 16, h == 3 * uh + 2 * 3});
    TEXT_measure("\xff\xf0\x9f\x98\x80\xe3\x81\x82", font_unifont, 0, &w, &h);
    expect("Measuring invalid UTF-8, a codepoint outside the font and one the "
           "font has but isn't loaded, all falling back to the replacement "
           "character...",
           2,
           (bool[]){w == 24, h == uh});
    TEXT_measure("A\xc3\xa7j~", font_able_5, 0, &w, &h);
    expect("Measuring Açj~ with the mock font, of varying widths, not counting "
           "pixels drawn past them...",
           2,
           (bool[]){w == 9, h == mh});
    TEXT_measure("AB", font_able_5, 0, &w, &h);
    expect("Measuring AB with the mock font, which has no B, falling back to "
           "its replacement character...",
           2,
           (bool[]){w == 7, h == mh});

    SDL_FColor white = {1.0f, 1.0f, 1.0f, 1.0f};
    TEXT_draw(10.0f,
              20.0f,
              "A\xc3\xa7\nA\xf0\x9f\x98\x80\xe3\x81\x82",
              font_unifont,
              0,
              white,
              0);
    RENDERER_update();
    graphics_vertex* v;
    graphics_texture* texture;
    size_t len = get_GRAPHICS_vertices(&v, &texture);
    drawn_glyph a0 = read_glyph(&v[0], texture);
    drawn_glyph cedilla = read_glyph(&v[6], texture);
    drawn_glyph a1 = read_glyph(&v[12], texture);
    drawn_glyph emoji = read_glyph(&v[18], texture);
    drawn_glyph unloaded = read_glyph(&v[24], texture);
    expect("Drew Aç\\nA😀あ with Unifont, checking the glyphs' rects...",
           11,
           (bool[]){
               len == 30,
               texture->w == TEXT_ATLAS_X,
               rect_eq(a0.dst, (SDL_FRect){10, 20, 8, uh}),
               rect_eq(cedilla.dst, (SDL_FRect){18, 20, 8, uh}),
               rect_eq(a1.dst, (SDL_FRect){10, 36, 8, uh}),
               rect_eq(emoji.dst, (SDL_FRect){18, 36, 8, uh}),
               rect_eq(unloaded.dst, (SDL_FRect){26, 36, 8, uh}),
               a0.src.w == 8 && a0.src.h == uh,
               rect_eq(a0.src, a1.src),
               rect_eq(emoji.src, unloaded.src),
               !rect_eq(a0.src, cedilla.src) && !rect_eq(a0.src, emoji.src),
           });
    clear_stubbed_calls();

    TEXT_draw(10.0f, 20.0f, "Aj~\n A", font_able_5, 2, white, 0);
    RENDERER_update();
    len = get_GRAPHICS_vertices(&v, &texture);
    a0 = read_glyph(&v[0], texture);
    drawn_glyph j = read_glyph(&v[6], texture);
    drawn_glyph tilde = read_glyph(&v[12], texture);
    drawn_glyph space = read_glyph(&v[18], texture);
    a1 = read_glyph(&v[24], texture);
    expect("Drew Aj~\\n A with the mock font and 2 pixels of line spacing, j "
           "drawn a pixel to the left and ~ past its width, checking the "
           "glyphs' rects...",
           9,
           (bool[]){
               len == 30,
               rect_eq(a0.dst, (SDL_FRect){10, 20, 3, mh}),
               rect_eq(j.dst, (SDL_FRect){12, 20, 3, mh}),
               rect_eq(tilde.dst, (SDL_FRect){15, 20, 4, mh}),
               rect_eq(space.dst, (SDL_FRect){10, 26, 2, mh}),
               rect_eq(a1.dst, (SDL_FRect){12, 26, 3, mh}),
               a0.src.w == 3 && j.src.w == 3 && tilde.src.w == 4,
               a0.src.h == mh && j.src.h == mh && tilde.src.h == mh,
               rect_eq(a0.src, a1.src),
           });
    clear_stubbed_calls();

    TEXT_stop();
    RENDERER_stop();
}
