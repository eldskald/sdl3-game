#pragma once
#include "defs.h"
#include <SDL3/SDL.h>

// Text is drawn with pixel fonts loaded from HEX files, see `bin/ttf2hex` for
// their format. Glyphs are drawn as is, without scaling or antialiasing, on
// lines laid out by the font's metrics, each glyph moving the next one right
// by its own width. Some glyphs draw past their width, overlapping their
// neighbors, like combining marks, which have no width and draw over the
// previous glyph, and some draw past their line's ascent and descent, like
// accents over capitals and cedillas, overlapping the lines around it when
// they're too close.
// Measures come in two kinds, like in most text layout engines. The logical
// ones are what text is laid out by, the glyphs' widths and the font's
// metrics, the same whatever the glyphs draw. The ink ones are the pixels
// actually drawn, for padding text so it isn't clipped or overlapped.
// Fonts are loaded for a language, see `language` on `defs.h`, with only the
// codepoints it needs, listed on `text.c`, plus every Latin one on dev and test
// builds. The rest, and the ones the font has no glyph for, are drawn as
// `TEXT_FALLBACK_CODEPOINT`. Some fonts are drawn with another font in some
// languages, like `font_main` with Fusion Pixel in Japanese, so their metrics
// change with the language, and text should be laid out by them.
// Measuring and setting the language can be done from any thread, while
// drawing and `TEXT_update()` are for the main thread.

// The fonts text can be drawn with.
//
// ```
// enum text_font { TEXT_FONT_TYPES };
// ```
typedef enum { TEXT_FONT_TYPES } text_font;

// A font's metrics, in pixels. Its lines reach `ascent` above their baseline
// and `descent` below it, and they're `line_gap` apart, so each line is
// `ascent + descent + line_gap` below the previous one. Its glyphs can draw up
// to `above` pixels above the baseline and `below` pixels below it, never
// less than `ascent` and `descent`.
typedef struct {
    int ascent, descent, line_gap, above, below;
} text_font_metrics;

// Parses every font in `STARTING_LANGUAGE` into a texture atlas holding its
// glyphs. Call after `RENDERER_start()`. Returns 0 when successful, 1
// otherwise.
int TEXT_start(void);

// Destroys the fonts' atlases. Call before `RENDERER_stop()`.
void TEXT_stop(void);

// Loads the fonts in the language set by `TEXT_set_language()` when it's
// another one than the loaded one, which takes a while, around a frame for
// CJK languages. When it can't, it logs it and keeps the loaded one. Call on
// the main thread, before `RENDERER_update()`, so no draws are queued with the
// fonts it replaces.
void TEXT_update(void);

// Sets the language for `TEXT_update()` to load the fonts in, for options
// menus, say.
void TEXT_set_language(language lang);

// The language set by `TEXT_set_language()`, or `STARTING_LANGUAGE`, which the
// fonts are loaded in after `TEXT_update()`.
language TEXT_get_language(void);

// Queues drawing the UTF-8 `text` with `font` on the renderer with its top left
// at `(x,y)`, in screen pixels like the other `RENDERER_draw_...()` calls, so
// call it on the `..._draw()` functions called by the renderer, on the main
// thread. Its top is the first line's ascent, so glyphs drawing above it, up to
// the font's `above`, draw above `y`. Glyphs are drawn one after the other and
// a `'\n'` moves to the next line, the font's `ascent + descent + line_gap`
// plus `line_spacing` pixels down, back to `x`. Its
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

// Fills `w,h` with the logical size in pixels `text` takes when drawn with
// `font` and `line_spacing` by `TEXT_draw()`, the sum of the widths of the
// glyphs on its widest line by its lines' ascents and descents and the spacing
// between them, so pixels drawn past the glyphs' widths or the lines' ascents
// and descents aren't counted. An empty `text` is `0,0`.
void TEXT_measure(
    const char* text, text_font font, int line_spacing, int* w, int* h);

// The smallest rect holding every pixel `text` draws when drawn with `font` and
// `line_spacing` by `TEXT_draw()` at `(0,0)`, so it's offset from where it's
// drawn, and it can start left or above it, from glyphs drawing to the left of
// where they're drawn or above their line's ascent. Text drawing no pixels,
// like an empty one or one of spaces, is `{0,0,0,0}`.
SDL_Rect TEXT_measure_ink(const char* text, text_font font, int line_spacing);

// The metrics of `font`.
text_font_metrics TEXT_get_font_metrics(text_font font);
