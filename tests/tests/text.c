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

static bool irect_eq(SDL_Rect a, SDL_Rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static bool metrics_eq(text_font_metrics a, text_font_metrics b) {
    return a.ascent == b.ascent && a.descent == b.descent &&
           a.line_gap == b.line_gap && a.above == b.above && a.below == b.below;
}

void test_text(void) {
    printf(YELLOW "\nTesting text...\n" RESET);

    RENDERER_start();
    TEXT_start();
    clear_stubbed_calls();

    int w, h;
    text_font_metrics um = TEXT_get_font_metrics(font_unifont);
    text_font_metrics mm = TEXT_get_font_metrics(font_main);
    expect("Checking the metrics of Unifont and the mock font...",
           2,
           (bool[]){metrics_eq(um, (text_font_metrics){14, 2, 0, 14, 2}),
                    metrics_eq(mm, (text_font_metrics){3, 1, 1, 4, 2})});
    int uh = um.ascent + um.descent;

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
    TEXT_measure("A\xc3\xa7j~", font_main, 0, &w, &h);
    expect("Measuring Açj~ with the mock font, of varying widths, not counting "
           "pixels drawn past them or past its ascent and descent...",
           2,
           (bool[]){w == 9, h == 4});
    TEXT_measure("AB", font_main, 0, &w, &h);
    expect("Measuring AB with the mock font, which has no B, falling back to "
           "its replacement character...",
           2,
           (bool[]){w == 7, h == 4});
    TEXT_measure("A\nA", font_main, 2, &w, &h);
    expect("Measuring A\\nA with the mock font and 2 pixels of line spacing, "
           "on top of its line gap...",
           2,
           (bool[]){w == 3, h == 4 + 1 + 2 + 4});

    expect("Measuring the ink of an empty string and of spaces...",
           2,
           (bool[]){
               irect_eq(TEXT_measure_ink("", font_main, 0), (SDL_Rect){0}),
               irect_eq(TEXT_measure_ink("  ", font_main, 0), (SDL_Rect){0}),
           });
    expect("Measuring the ink of \xc3\xa3\xc3\xa7 with the mock font, its "
           "tilde above the ascent and its cedilla below the descent...",
           1,
           (bool[]){irect_eq(TEXT_measure_ink("\xc3\xa3\xc3\xa7", font_main, 0),
                             (SDL_Rect){0, -1, 6, 6})});
    expect("Measuring the ink of ~ with the mock font, past its width...",
           1,
           (bool[]){irect_eq(TEXT_measure_ink("~", font_main, 0),
                             (SDL_Rect){0, 0, 4, 1})});
    expect("Measuring the ink of j\\n \xc3\xa3 with the mock font, j drawing "
           "to the left of where it's drawn...",
           1,
           (bool[]){irect_eq(TEXT_measure_ink("j\n \xc3\xa3", font_main, 0),
                             (SDL_Rect){-1, 0, 6, 8})});

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
    expect("Drew Aç\\nA😀あ with Unifont, its bitmaps trimmed to their set "
           "pixels, checking the glyphs' rects...",
           11,
           (bool[]){
               len == 30,
               texture->w == TEXT_ATLAS_X,
               rect_eq(a0.dst, (SDL_FRect){11, 24, 6, 10}),
               rect_eq(cedilla.dst, (SDL_FRect){19, 26, 6, 10}),
               rect_eq(a1.dst, (SDL_FRect){11, 24 + uh, 6, 10}),
               rect_eq(emoji.dst, (SDL_FRect){19, 23 + uh, 6, 11}),
               rect_eq(unloaded.dst, (SDL_FRect){27, 23 + uh, 6, 11}),
               a0.src.w == 6 && a0.src.h == 10,
               rect_eq(a0.src, a1.src),
               rect_eq(emoji.src, unloaded.src),
               !rect_eq(a0.src, cedilla.src) && !rect_eq(a0.src, emoji.src),
           });
    clear_stubbed_calls();

    TEXT_draw(10.0f, 20.0f, "Aj~\n A", font_main, 2, white, 0);
    RENDERER_update();
    len = get_GRAPHICS_vertices(&v, &texture);
    a0 = read_glyph(&v[0], texture);
    drawn_glyph j = read_glyph(&v[6], texture);
    drawn_glyph tilde = read_glyph(&v[12], texture);
    a1 = read_glyph(&v[18], texture);
    expect("Drew Aj~\\n A with the mock font and 2 pixels of line spacing, j "
           "drawn a pixel to the left, ~ past its width and the space not "
           "drawn, having no pixels, checking the glyphs' rects and the "
           "atlas, all in a row as tall as its tallest bitmap, ç's...",
           8,
           (bool[]){
               len == 24,
               texture->h == 5,
               rect_eq(a0.dst, (SDL_FRect){10, 20, 3, 4}),
               rect_eq(j.dst, (SDL_FRect){12, 20, 3, 4}),
               rect_eq(tilde.dst, (SDL_FRect){15, 20, 4, 1}),
               rect_eq(a1.dst, (SDL_FRect){12, 27, 3, 4}),
               a0.src.w == 3 && j.src.w == 3 && tilde.src.w == 4,
               rect_eq(a0.src, a1.src),
           });
    clear_stubbed_calls();

    TEXT_set_language(language_ja);
    bool requested = TEXT_get_language() == language_ja;
    bool kept = metrics_eq(TEXT_get_font_metrics(font_main), mm);
    TEXT_update();
    TEXT_measure("\xe3\x81\x82"
                 "A",
                 font_main,
                 0,
                 &w,
                 &h);
    expect("Setting the language to Japanese, the main font only changing to "
           "Fusion Pixel on the update, measuring あA with it...",
           4,
           (bool[]){
               requested,
               kept,
               metrics_eq(TEXT_get_font_metrics(font_main),
                          (text_font_metrics){11, 3, 0, 13, 5}),
               w == 10 + 6 && h == 11 + 3,
           });
    TEXT_measure("\xe2\x82\xac", font_main, 0, &w, &h);
    expect("Measuring \xe2\x82\xac in Japanese, which Fusion Pixel lacks, "
           "falling back to Galmuri9's, 7 pixels wide, instead of the "
           "replacement character...",
           1,
           (bool[]){w == 7});
    TEXT_set_language(language_en);
    TEXT_update();
    expect("Setting the language back to English, back to the mock font...",
           1,
           (bool[]){metrics_eq(TEXT_get_font_metrics(font_main), mm)});

    TEXT_stop();
    RENDERER_stop();
}
