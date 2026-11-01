#pragma once
#include "defs.h"
#include <SDL3/SDL.h>

// Text is drawn with pixel fonts loaded from HEX files, see `bin/ttf2hex` for
// their format. Glyphs are drawn as is, without scaling or antialiasing, all
// of a font's glyphs as tall as its height, each moving the next one right by
// its own width. Some glyphs draw past their width, overlapping their
// neighbors, like combining marks, which have no width and draw over the
// previous glyph.
// Only the codepoints listed on `text.c` are loaded, the rest, and the ones the
// font has no glyph for, are drawn as `TEXT_FALLBACK_CODEPOINT`.

// The fonts text can be drawn with.
//
// ```
// enum text_font { TEXT_FONT_TYPES };
// ```
typedef enum { TEXT_FONT_TYPES } text_font;

// Parses every font into a texture atlas holding its glyphs. Call after
// `RENDERER_start()`. Returns 0 when successful, 1 otherwise.
int TEXT_start(void);

// Destroys the fonts' atlases. Call before `RENDERER_stop()`.
void TEXT_stop(void);

// Queues drawing the UTF-8 `text` with `font` on the renderer with its top left
// at `(x,y)`, in screen pixels like the other `RENDERER_draw_...()` calls, so
// call it on the `..._draw()` functions called by the renderer, on the main
// thread. Glyphs are drawn one after the other and a `'\n'` moves to the next
// line, the font's height plus `line_spacing` pixels down, back to `x`. Its
// color is multiplied by `col`, which can go beyond `1.0` to make it bloom.
// Invalid UTF-8 is drawn as `TEXT_FALLBACK_CODEPOINT`.
void TEXT_draw(float x,
               float y,
               const char* text,
               text_font font,
               int line_spacing,
               SDL_FColor col,
               Sint8 z_index);

// Same as `TEXT_draw()`, but drawn right away with `GRAPHICS_draw_texture()`
// on the current target with the current state, instead of queued on the
// renderer. For drawing outside the renderer's queues, like over the window
// after post processing.
void TEXT_draw_now(float x,
                   float y,
                   const char* text,
                   text_font font,
                   int line_spacing,
                   SDL_FColor col);

// Fills `w,h` with the size in pixels `text` takes when drawn with `font` and
// `line_spacing` by `TEXT_draw()`, the sum of the widths of the glyphs on its
// widest line by its lines' heights and the spacing between them, so pixels
// drawn past the glyphs' widths aren't counted. An empty `text` is `0,0`.
void TEXT_measure(
    const char* text, text_font font, int line_spacing, int* w, int* h);

// The height of `font`'s glyphs in pixels, which is also its line height.
int TEXT_get_font_height(text_font font);
