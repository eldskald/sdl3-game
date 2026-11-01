#include "engine/text.h"
#include "defs.h"
#include "engine/graphics.h"
#include "engine/renderer.h"
#include "paths.h"
#include <SDL3/SDL.h>

// The codepoints loaded from the fonts, as inclusive `{first, last}` ranges in
// ascending order, so the atlas only holds the glyphs the game uses. Everything
// else is drawn as `TEXT_FALLBACK_CODEPOINT`, so it must be in one of them.
// For now it's what English and Portuguese need:
// - `U+0020` to `U+007E`, Basic Latin, ASCII's printable characters.
// - `U+00A0` to `U+00FF`, Latin-1 Supplement, accented letters like ç, é, ã,
//   and symbols like ª, º, «, ».
// - `U+2010` to `U+2027`, General Punctuation's dashes, quotes and ellipsis,
//   like –, —, ‘, ’, “, ”, ….
// - `U+FFFD`, the replacement character, '�', the fallback.
#define CODEPOINT_RANGES                                                       \
    {0x0020, 0x007E}, {0x00A0, 0x00FF}, {0x2010, 0x2027}, {0xFFFD, 0xFFFD}

static const Uint32 ranges[][2] = {CODEPOINT_RANGES};

// A glyph's bitmap is at `x,y` on its font's atlas, `bitmap_w` pixels wide and
// its font's height tall, and it's drawn `x_offset` pixels from where the glyph
// is drawn, which then moves `w` pixels to the right for the next one. `found`
// is false when the font has no glyph for that codepoint.
typedef struct {
    Uint16 x, y;
    Uint8 w, bitmap_w;
    Sint8 x_offset;
    bool found;
} glyph;

// A font loaded from a HEX file by `load_font()`. `glyphs` has the glyph of
// every codepoint in `ranges`, one range after the other, placed on `atlas`,
// all `h` pixels tall.
typedef struct {
    graphics_texture* atlas;
    glyph* glyphs;
    int h;
} loaded_font;

// Each font's name, for logging, and the null terminated contents of its HEX
// file, by `text_font`. They're megabytes long, so they're static to stay off
// the stack.
static const char unifont_hex[] = {
#embed TEXT_UNIFONT_PATH suffix(, '\0')
};
static const char able_5_hex[] = {
#embed TEXT_ABLE_5_PATH suffix(, '\0')
};

static const struct {
    const char* name;
    const char* hex;
} sources[TEXT_FONTS_TOTAL] = {
    [font_unifont] = {"unifont", unifont_hex},
    [font_able_5] = {"able_5", able_5_hex},
};

static loaded_font fonts[TEXT_FONTS_TOTAL] = {0};



static Uint8 hex_digit(char c) {
    return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
}

// Reads the hex number at `*c` until `end`, moving `*c` past `end`.
static Uint32 read_hex(const char** c, char end) {
    Uint32 n = 0;
    while (**c != end)
        n = (n << 4) | hex_digit(*(*c)++);
    (*c)++;
    return n;
}

// The glyph of `codepoint` on `f`, `NULL` when it's not in `ranges`.
static glyph* find_glyph(const loaded_font* f, Uint32 codepoint) {
    size_t offset = 0;
    for (size_t i = 0; i < SDL_arraysize(ranges); i++) {
        if (codepoint < ranges[i][0]) return NULL;
        if (codepoint <= ranges[i][1])
            return &f->glyphs[offset + codepoint - ranges[i][0]];
        offset += ranges[i][1] - ranges[i][0] + 1;
    }
    return NULL;
}

static const glyph* get_glyph(const loaded_font* f, Uint32 codepoint) {
    const glyph* g = find_glyph(f, codepoint);
    return g && g->found ? g : find_glyph(f, TEXT_FALLBACK_CODEPOINT);
}



// A line of a HEX file, a glyph as `CODEPOINT:WIDTH:XOFFSET:BITMAP`, see
// `bin/ttf2hex`. The bitmap is `digits` long, `digits_per_row` of them a row.
typedef struct {
    Uint32 codepoint;
    int w, x_offset;
    const char* bitmap;
    int digits, digits_per_row;
} hex_line;

// Reads the line at `*c`, moving `*c` to the next one.
static hex_line read_line(const char** c) {
    hex_line l = {0};
    l.codepoint = read_hex(c, ':');
    l.w = read_hex(c, ':');
    l.x_offset = read_hex(c, ':');
    if (l.x_offset >= 0x80) l.x_offset -= 0x100; // Two's complement
    l.bitmap = *c;
    while (**c != '\n' && **c != '\0')
        (*c)++;
    l.digits = *c - l.bitmap;
    l.digits_per_row = (l.w - l.x_offset + 7) / 8 * 2;
    if (**c == '\n') (*c)++;
    return l;
}

// The bitmap's rows are padded to whole bytes, and glyphs can draw on the
// padding, so it's however much of them it takes to fit its width from its
// offset and its rightmost pixel.
static int read_bitmap_width(const hex_line* l) {
    int w = l->w - l->x_offset;
    for (int i = 0; i < l->digits; i++) {
        Uint8 nibble = hex_digit(l->bitmap[i]);
        for (int bit = 0; bit < 4; bit++)
            if (nibble & (0x8 >> bit))
                w = SDL_max(w, (i % l->digits_per_row) * 4 + bit + 1);
    }
    return w;
}

// Every glyph has as many rows, so it's the rows of the first one with any.
static int read_height(const char* hex) {
    for (const char* c = hex; *c;) {
        hex_line l = read_line(&c);
        if (l.digits) return l.digits / l.digits_per_row;
    }
    return 0;
}

// Loads the font from `hex`, the null terminated contents of a HEX file, onto
// `f`, `name` being for logging. It's read twice. First, the bitmaps of the
// glyphs in `ranges` are placed on the atlas left to right in the file's order,
// wrapping to the next row of glyphs when one doesn't fit, which gives the
// atlas' height. Then, they're drawn on it, set pixels white, so they're drawn
// in the color they're multiplied by. Returns 0 when successful, 1 otherwise.
// Free it with `unload_font()` either way.
static int load_font(const char* name, const char* hex, loaded_font* f) {
    size_t total = 0;
    for (size_t i = 0; i < SDL_arraysize(ranges); i++)
        total += ranges[i][1] - ranges[i][0] + 1;
    f->glyphs = SDL_calloc(total, sizeof(glyph));
    f->h = read_height(hex);

    int atlas_x = 0;
    int atlas_y = 0;
    for (const char* c = hex; *c;) {
        hex_line l = read_line(&c);
        glyph* g = find_glyph(f, l.codepoint);
        if (!g) continue;
        int bitmap_w = read_bitmap_width(&l);
        if (atlas_x + bitmap_w > TEXT_ATLAS_X) {
            atlas_x = 0;
            atlas_y += f->h;
        }
        *g = (glyph){atlas_x, atlas_y, l.w, bitmap_w, l.x_offset, true};
        atlas_x += bitmap_w;
    }

    SDL_Surface* surface =
        SDL_CreateSurface(TEXT_ATLAS_X, atlas_y + f->h, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't create the atlas surface of font %s: %s",
                     name,
                     SDL_GetError());
        return 1;
    }

    for (const char* c = hex; *c;) {
        hex_line l = read_line(&c);
        const glyph* g = find_glyph(f, l.codepoint);
        if (!g) continue;
        for (int i = 0; i < l.digits; i++) {
            Uint8 nibble = hex_digit(l.bitmap[i]);
            if (!nibble) continue;
            int col = (i % l.digits_per_row) * 4;
            Uint32* row =
                (Uint32*)((Uint8*)surface->pixels +
                          (g->y + i / l.digits_per_row) * surface->pitch);
            for (int bit = 0; bit < 4; bit++)
                if (nibble & (0x8 >> bit)) row[g->x + col + bit] = 0xFFFFFFFF;
        }
    }

    f->atlas = GRAPHICS_create_texture_from_surface(
        name, surface, SDL_GPU_FILTER_NEAREST);
    SDL_DestroySurface(surface);
    if (!f->atlas) return 1;

    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION, "Font %s loaded successfully.", name);
    return 0;
}

static void unload_font(loaded_font* f) {
    GRAPHICS_destroy_texture(f->atlas);
    SDL_free(f->glyphs);
    *f = (loaded_font){0};
}



int TEXT_start(void) {
    for (int i = 0; i < TEXT_FONTS_TOTAL; i++)
        if (load_font(sources[i].name, sources[i].hex, &fonts[i])) return 1;
    return 0;
}

void TEXT_stop(void) {
    for (int i = 0; i < TEXT_FONTS_TOTAL; i++)
        unload_font(&fonts[i]);
}



// Calls `draw_glyph` with the `src` rect on `f`'s atlas and the `dst` rect on
// the target of every glyph of `text` with a bitmap, laid out as `TEXT_draw()`
// says.
static void layout(const loaded_font* f,
                   float x,
                   float y,
                   const char* text,
                   int line_spacing,
                   void (*draw_glyph)(SDL_FRect src, SDL_FRect dst, void* arg),
                   void* arg) {
    float pen_x = x;
    Uint32 codepoint;
    while ((codepoint = SDL_StepUTF8(&text, NULL))) {
        if (codepoint == '\n') {
            pen_x = x;
            y += f->h + line_spacing;
            continue;
        }
        const glyph* g = get_glyph(f, codepoint);
        if (g->bitmap_w)
            draw_glyph((SDL_FRect){g->x, g->y, g->bitmap_w, f->h},
                       (SDL_FRect){pen_x + g->x_offset, y, g->bitmap_w, f->h},
                       arg);
        pen_x += g->w;
    }
}

typedef struct {
    graphics_texture* atlas;
    SDL_FColor col;
    Sint8 z_index;
} glyph_arg;

static void queue_glyph(SDL_FRect src, SDL_FRect dst, void* arg) {
    glyph_arg* a = arg;
    RENDERER_draw_from_texture(
        a->atlas, src, dst, SDL_FLIP_NONE, a->col, a->z_index);
}

static void draw_glyph_now(SDL_FRect src, SDL_FRect dst, void* arg) {
    glyph_arg* a = arg;
    GRAPHICS_draw_texture(a->atlas, &src, &dst, SDL_FLIP_NONE, a->col);
}

void TEXT_draw(float x,
               float y,
               const char* text,
               text_font font,
               int line_spacing,
               SDL_FColor col,
               Sint8 z_index) {
    layout(&fonts[font],
           x,
           y,
           text,
           line_spacing,
           queue_glyph,
           &(glyph_arg){fonts[font].atlas, col, z_index});
}

void TEXT_draw_now(float x,
                   float y,
                   const char* text,
                   text_font font,
                   int line_spacing,
                   SDL_FColor col) {
    layout(&fonts[font],
           x,
           y,
           text,
           line_spacing,
           draw_glyph_now,
           &(glyph_arg){fonts[font].atlas, col, 0});
}

void TEXT_measure(
    const char* text, text_font font, int line_spacing, int* w, int* h) {
    const loaded_font* f = &fonts[font];
    *w = 0;
    *h = *text ? f->h : 0;
    int line_w = 0;
    Uint32 codepoint;
    while ((codepoint = SDL_StepUTF8(&text, NULL))) {
        if (codepoint == '\n') {
            line_w = 0;
            *h += f->h + line_spacing;
            continue;
        }
        line_w += get_glyph(f, codepoint)->w;
        *w = SDL_max(*w, line_w);
    }
}

int TEXT_get_font_height(text_font font) {
    return fonts[font].h;
}
