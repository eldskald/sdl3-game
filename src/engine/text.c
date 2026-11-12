#include "engine/text.h"
#include "defs.h"
#include "engine/graphics.h"
#include "engine/renderer.h"
#include "paths.h"
#include "utils/logger.h"
#include <SDL3/SDL.h>

// The codepoints loaded from the fonts are the ones every language needs, the
// ones of the language the game is in and, on dev and test builds, every Latin
// one, so text in any Latin language can be tested on any of them. Everything
// else is drawn as `TEXT_FALLBACK_CODEPOINT`. They're inclusive ranges of
// codepoints, some the fonts don't have, which are drawn as the fallback too.
typedef struct {
    Uint32 first, last;
} codepoint_range;

// What every language needs:
// - `U+0020` to `U+007E`, Basic Latin, ASCII's printable characters.
// - `U+00A0` to `U+017F`, Latin-1 Supplement and Latin Extended-A, accented
//   letters like ç, é, ã, ł, ő, ğ, œ, and symbols like ª, º, «, ».
// - `U+2010` to `U+205E`, General Punctuation's dashes, quotes, ellipsis and
//   spaces, like –, —, ‘, ’, “, ”, …, ‹, ›, and French's narrow no-break space.
// - `U+20A0` to `U+20C0`, Currency Symbols, like €.
// - `U+FFFD`, the replacement character, '�', the fallback.
static const codepoint_range common_ranges[] = {
    {0x0020, 0x007E},
    {0x00A0, 0x017F},
    {0x2010, 0x205E},
    {0x20A0, 0x20C0},
    {0xFFFD, 0xFFFD},
};

// Every other Latin codepoint, for dev and test builds: Latin Extended-B, IPA
// Extensions, Spacing Modifier Letters, Combining Diacritical Marks, Phonetic
// Extensions, Combining Diacritical Marks Supplement, Latin Extended
// Additional, Latin Extended-C, -D and -E.
[[maybe_unused]] static const codepoint_range latin_ranges[] = {
    {0x0180, 0x036F},
    {0x1D00, 0x1DFF},
    {0x1E00, 0x1EFF},
    {0x2C60, 0x2C7F},
    {0xA720, 0xA7FF},
    {0xAB30, 0xAB6F},
};

#define ROMANIAN {0x0218, 0x021B} // Ș, ș, Ț, ț, with commas below
#define GREEK {0x0370, 0x03FF}
#define CYRILLIC {0x0400, 0x04FF}
// Ơ, ơ, Ư, ư, and the vowels with tone marks.
#define VIETNAMESE {0x01A0, 0x01B0}, {0x1EA0, 0x1EF9}
// CJK Symbols and Punctuation, like 「, 」, 。, and Halfwidth and Fullwidth
// Forms, like ！, ？, and halfwidth katakana.
#define CJK_SYMBOLS {0x3000, 0x303F}, {0xFF00, 0xFFEF}
#define IDEOGRAPHS                                                             \
    {0x4E00, 0x9FFF} // CJK Unified Ideographs, without extensions
#define KANA {0x3040, 0x30FF}, {0x31F0, 0x31FF} // Hiragana and Katakana
// Hangul Compatibility Jamo and Hangul Syllables.
#define HANGUL {0x3130, 0x318F}, {0xAC00, 0xD7A3}
#define BOPOMOFO {0x3100, 0x312F} // For Chinese's pronunciation
#define CHINESE CJK_SYMBOLS, BOPOMOFO, IDEOGRAPHS

// Each language's name, for logging, and the codepoints it needs besides
// `common_ranges`, unused ones zeroed.
#define LANGUAGE_MAX_RANGES 6
static const struct {
    const char* name;
    codepoint_range ranges[LANGUAGE_MAX_RANGES];
} languages[LANGUAGES_TOTAL] = {
    [language_en] = {"en"},
    [language_pt] = {"pt"},
    [language_es] = {"es"},
    [language_fr] = {"fr"},
    [language_de] = {"de"},
    [language_it] = {"it"},
    [language_nl] = {"nl"},
    [language_sv] = {"sv"},
    [language_da] = {"da"},
    [language_nb] = {"nb"},
    [language_fi] = {"fi"},
    [language_pl] = {"pl"},
    [language_cs] = {"cs"},
    [language_hu] = {"hu"},
    [language_ro] = {"ro", {ROMANIAN}},
    [language_tr] = {"tr"},
    [language_id] = {"id"},
    [language_ms] = {"ms"},
    [language_tl] = {"tl"},
    [language_el] = {"el", {GREEK}},
    [language_ru] = {"ru", {CYRILLIC}},
    [language_uk] = {"uk", {CYRILLIC}},
    [language_vi] = {"vi", {VIETNAMESE}},
    [language_ja] = {"ja", {CJK_SYMBOLS, KANA, IDEOGRAPHS}},
    [language_ko] = {"ko", {CJK_SYMBOLS, HANGUL}},
    [language_zh_hans] = {"zh-hans", {CHINESE}},
    [language_zh_hant] = {"zh-hant", {CHINESE}},
    [language_zh_hk] = {"zh-hk", {CHINESE}},
    [language_zh_tw] = {"zh-tw", {CHINESE}},
};

// The codepoints loaded, sorted and without overlaps, built by
// `build_charset()`.
typedef struct {
    codepoint_range ranges[SDL_arraysize(common_ranges) + LANGUAGE_MAX_RANGES +
                           SDL_arraysize(latin_ranges)];
    size_t total;
} charset;

// A glyph's bitmap is at `x,y` on its font's atlas, `bitmap_w` by `bitmap_h`
// pixels, trimmed to the pixels it sets. Its left edge is `x_offset` pixels to
// the right of where the glyph is drawn and its top edge `y_offset` pixels
// above the baseline, and the glyph moves the next one `w` pixels to the
// right. `found` is false when no font of the chain has a glyph for that
// codepoint, and `source` is the index of the one on the chain it's from.
typedef struct {
    Uint16 x, y;
    Uint8 w, bitmap_w, bitmap_h;
    Sint8 x_offset, y_offset;
    Uint8 source;
    bool found;
} glyph;

// A font loaded from a chain of HEX files by `load_font()`. `glyphs` has the
// glyph of every codepoint in `charset`, one range after the other, placed on
// `atlas`.
typedef struct {
    graphics_texture* atlas;
    glyph* glyphs;
    charset charset;
    text_font_metrics metrics;
} loaded_font;

// The null terminated contents of the HEX files. They're megabytes long, so
// they're static to stay off the stack.
static const char able_5_hex[] = {
#embed TEXT_ABLE_5_PATH suffix(, '\0')
};
static const char galmuri_9_hex[] = {
#embed TEXT_GALMURI_9_PATH suffix(, '\0')
};
static const char fusion_ja_hex[] = {
#embed TEXT_FUSION_JA_PATH suffix(, '\0')
};
static const char fusion_ko_hex[] = {
#embed TEXT_FUSION_KO_PATH suffix(, '\0')
};
static const char fusion_zh_hans_hex[] = {
#embed TEXT_FUSION_ZH_HANS_PATH suffix(, '\0')
};
static const char fusion_zh_hant_hex[] = {
#embed TEXT_FUSION_ZH_HANT_PATH suffix(, '\0')
};
static const char fusion_zh_hk_hex[] = {
#embed TEXT_FUSION_ZH_HK_PATH suffix(, '\0')
};
static const char fusion_zh_tw_hex[] = {
#embed TEXT_FUSION_ZH_TW_PATH suffix(, '\0')
};
#if DEV
static const char unifont_hex[] = {
#embed TEXT_UNIFONT_PATH suffix(, '\0')
};
#endif

// Each font's name, for logging, and the chain of HEX files it's loaded from,
// by `text_font`, `chain` unless `chain_by_language` has one for the language.
// Each glyph is taken from the first file on the chain having it, so the
// fallbacks after the first only fill in what it lacks, all on the same atlas
// and on the same baseline. Unused ones are `NULL`.
// Fusion Pixel draws ideographs in the shapes of the language's standard, so
// each CJK language has its own. It lacks some symbols, like €, ‹ and ›, taken
// from Galmuri9, as tall as its ideographs, and from Able 5 after it. Galmuri9
// is for Vietnamese, whose stacked tone marks are too cramped on Able 5.
#define CHAIN_MAX 3
#define FUSION_CHAIN(hex) {hex, galmuri_9_hex, able_5_hex}
static const struct {
    const char* name;
    const char* chain[CHAIN_MAX];
    const char* chain_by_language[LANGUAGES_TOTAL][CHAIN_MAX];
} sources[TEXT_FONTS_TOTAL] = {
    [font_main] = {"main",
                   {able_5_hex},
                   {
                       [language_vi] = {galmuri_9_hex, able_5_hex},
                       [language_ja] = FUSION_CHAIN(fusion_ja_hex),
                       [language_ko] = FUSION_CHAIN(fusion_ko_hex),
                       [language_zh_hans] = FUSION_CHAIN(fusion_zh_hans_hex),
                       [language_zh_hant] = FUSION_CHAIN(fusion_zh_hant_hex),
                       [language_zh_hk] = FUSION_CHAIN(fusion_zh_hk_hex),
                       [language_zh_tw] = FUSION_CHAIN(fusion_zh_tw_hex),
                   }},
#if DEV
    [font_unifont] = {"unifont", {unifont_hex}},
#endif
};

// The fonts loaded, in `loaded_language`, and the language requested by
// `TEXT_set_language()`, which `TEXT_update()` loads when it's another one.
// `lock` is held for writing while swapping the fonts, so they can be read from
// any thread.
static loaded_font fonts[TEXT_FONTS_TOTAL] = {0};
static language loaded_language = STARTING_LANGUAGE;
static SDL_AtomicInt requested_language = {STARTING_LANGUAGE};
static SDL_RWLock* lock = NULL;



// Adds the `n` ranges at `r` to `c`, sorted by their first codepoint, stopping
// at a zeroed one.
static void add_ranges(charset* c, const codepoint_range* r, size_t n) {
    for (size_t i = 0; i < n && r[i].last; i++) {
        size_t j = c->total++;
        for (; j > 0 && c->ranges[j - 1].first > r[i].first; j--)
            c->ranges[j] = c->ranges[j - 1];
        c->ranges[j] = r[i];
    }
}

// The codepoints loaded for `lang`, overlapping and adjacent ranges merged.
static charset build_charset(language lang) {
    charset c = {0};
    add_ranges(&c, common_ranges, SDL_arraysize(common_ranges));
    add_ranges(&c, languages[lang].ranges, LANGUAGE_MAX_RANGES);
#if DEV || TEST
    add_ranges(&c, latin_ranges, SDL_arraysize(latin_ranges));
#endif
    size_t n = 0;
    for (size_t i = 0; i < c.total; i++) {
        if (n && c.ranges[i].first <= c.ranges[n - 1].last + 1)
            c.ranges[n - 1].last =
                SDL_max(c.ranges[n - 1].last, c.ranges[i].last);
        else
            c.ranges[n++] = c.ranges[i];
    }
    c.total = n;
    return c;
}



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

// The glyph of `codepoint` on `f`, `NULL` when it's not in its charset.
static glyph* find_glyph(const loaded_font* f, Uint32 codepoint) {
    const codepoint_range* r = f->charset.ranges;
    size_t offset = 0;
    for (size_t i = 0; i < f->charset.total; i++) {
        if (codepoint < r[i].first) return NULL;
        if (codepoint <= r[i].last)
            return &f->glyphs[offset + codepoint - r[i].first];
        offset += r[i].last - r[i].first + 1;
    }
    return NULL;
}

static const glyph* get_glyph(const loaded_font* f, Uint32 codepoint) {
    const glyph* g = find_glyph(f, codepoint);
    return g && g->found ? g : find_glyph(f, TEXT_FALLBACK_CODEPOINT);
}



// A line of a HEX file, a glyph as
// `CODEPOINT:WIDTH:XOFFSET:YOFFSET:BITMAPWIDTH:BITMAP`, see `bin/ttf2hex`. The
// bitmap is `digits` long, `digits_per_row` of them a row.
typedef struct {
    Uint32 codepoint;
    int w, x_offset, y_offset, bitmap_w;
    const char* bitmap;
    int digits, digits_per_row;
} hex_line;

// Reads the signed 2 digits hex number at `*c` until `end`, moving `*c` past
// `end`.
static int read_signed_hex(const char** c, char end) {
    int n = (int)read_hex(c, end);
    return n >= 0x80 ? n - 0x100 : n; // Two's complement
}

// Reads the line at `*c`, moving `*c` to the next one.
static hex_line read_line(const char** c) {
    hex_line l = {0};
    l.codepoint = read_hex(c, ':');
    l.w = read_hex(c, ':');
    l.x_offset = read_signed_hex(c, ':');
    l.y_offset = read_signed_hex(c, ':');
    l.bitmap_w = read_hex(c, ':');
    l.bitmap = *c;
    while (**c != '\n' && **c != '\0')
        (*c)++;
    l.digits = *c - l.bitmap;
    l.digits_per_row = (l.bitmap_w + 7) / 8 * 2;
    if (**c == '\n') (*c)++;
    return l;
}

// Reads the metrics on the HEX file's first line at `*c`, moving `*c` to the
// next one, where its glyphs start.
static text_font_metrics read_metrics(const char** c) {
    text_font_metrics m;
    m.ascent = read_hex(c, ':');
    m.descent = read_hex(c, ':');
    m.line_gap = read_hex(c, ':');
    m.above = read_hex(c, ':');
    m.below = read_hex(c, '\n');
    return m;
}

// Loads the glyphs of `charset` from `chain`, the null terminated contents of
// HEX files, `NULL` after the last one, onto `f`, `name` being for logging.
// Its metrics are read from the first file's first line, its `above` and
// `below` grown to fit the glyphs taken from the others. Then the glyphs are
// read twice, file after file, each from the first file having it. First,
// their bitmaps are placed on the atlas left to right in the files' order, in
// rows as tall as their tallest bitmap, wrapping to the next row when one
// doesn't fit, which gives the atlas' height. Then, they're drawn on it, set
// pixels white, so they're drawn in the color they're multiplied by. Returns 0
// when successful, 1 otherwise. Free it with `unload_font()` either way.
static int load_font(const char* name,
                     const char* const chain[CHAIN_MAX],
                     const charset* charset,
                     loaded_font* f) {
    f->charset = *charset;
    size_t total = 0;
    for (size_t i = 0; i < charset->total; i++)
        total += charset->ranges[i].last - charset->ranges[i].first + 1;
    f->glyphs = SDL_calloc(total, sizeof(glyph));
    const char* glyphs_start[CHAIN_MAX];
    for (int s = 0; s < CHAIN_MAX && chain[s]; s++) {
        glyphs_start[s] = chain[s];
        text_font_metrics m = read_metrics(&glyphs_start[s]);
        if (!s) f->metrics = m;
    }

    int atlas_x = 0;
    int atlas_y = 0;
    int row_h = 0;
    for (int s = 0; s < CHAIN_MAX && chain[s]; s++)
        for (const char* c = glyphs_start[s]; *c;) {
            hex_line l = read_line(&c);
            glyph* g = find_glyph(f, l.codepoint);
            if (!g || g->found) continue;
            int bitmap_h = l.digits ? l.digits / l.digits_per_row : 0;
            if (atlas_x + l.bitmap_w > TEXT_ATLAS_X) {
                atlas_x = 0;
                atlas_y += row_h;
                row_h = 0;
            }
            *g = (glyph){atlas_x,
                         atlas_y,
                         l.w,
                         l.bitmap_w,
                         bitmap_h,
                         l.x_offset,
                         l.y_offset,
                         s,
                         true};
            atlas_x += l.bitmap_w;
            row_h = SDL_max(row_h, bitmap_h);
            if (s && bitmap_h) {
                f->metrics.above = SDL_max(f->metrics.above, l.y_offset);
                f->metrics.below =
                    SDL_max(f->metrics.below, bitmap_h - l.y_offset);
            }
        }

    SDL_Surface* surface = SDL_CreateSurface(
        TEXT_ATLAS_X, atlas_y + row_h, SDL_PIXELFORMAT_RGBA32);
    if (!log_creation(SDL_LOG_CATEGORY_APPLICATION,
                      surface,
                      "atlas surface of font %s",
                      name))
        return 1;

    for (int s = 0; s < CHAIN_MAX && chain[s]; s++)
        for (const char* c = glyphs_start[s]; *c;) {
            hex_line l = read_line(&c);
            const glyph* g = find_glyph(f, l.codepoint);
            if (!g || g->source != s) continue;
            for (int i = 0; i < l.digits; i++) {
                Uint8 nibble = hex_digit(l.bitmap[i]);
                if (!nibble) continue;
                int col = (i % l.digits_per_row) * 4;
                Uint32* row =
                    (Uint32*)((Uint8*)surface->pixels +
                              (g->y + i / l.digits_per_row) * surface->pitch);
                for (int bit = 0; bit < 4; bit++)
                    if (nibble & (0x8 >> bit))
                        row[g->x + col + bit] = 0xFFFFFFFF;
            }
        }

    f->atlas = GRAPHICS_create_texture_from_surface(
        name, surface, SDL_GPU_FILTER_NEAREST);
    SDL_DestroySurface(surface);
    if (!f->atlas) return 1;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Font %s loaded successfully, on a %dx%d atlas.",
                name,
                f->atlas->w,
                f->atlas->h);
    return 0;
}

static void unload_font(loaded_font* f) {
    GRAPHICS_destroy_texture(f->atlas);
    SDL_free(f->glyphs);
    *f = (loaded_font){0};
}



// Loads every font in `lang` onto `f`. Returns 0 when successful, 1 otherwise,
// with none loaded.
static int load_fonts(language lang, loaded_font f[TEXT_FONTS_TOTAL]) {
    charset c = build_charset(lang);
    for (int i = 0; i < TEXT_FONTS_TOTAL; i++) {
        const char* const* chain = sources[i].chain_by_language[lang];
        char name[64];
        SDL_snprintf(name,
                     sizeof(name),
                     "%s (%s)",
                     sources[i].name,
                     languages[lang].name);
        if (load_font(name, chain[0] ? chain : sources[i].chain, &c, &f[i])) {
            for (int j = 0; j <= i; j++)
                unload_font(&f[j]);
            return 1;
        }
    }
    return 0;
}

int TEXT_start(void) {
    lock = SDL_CreateRWLock();
    if (!log_creation(SDL_LOG_CATEGORY_APPLICATION, lock, "fonts' lock"))
        return 1;
    loaded_language = STARTING_LANGUAGE;
    SDL_SetAtomicInt(&requested_language, STARTING_LANGUAGE);
    return load_fonts(STARTING_LANGUAGE, fonts);
}

void TEXT_stop(void) {
    for (int i = 0; i < TEXT_FONTS_TOTAL; i++)
        unload_font(&fonts[i]);
    SDL_DestroyRWLock(lock);
    lock = NULL;
}

void TEXT_update(void) {
    language lang = SDL_GetAtomicInt(&requested_language);
    if (lang == loaded_language) return;

    loaded_font next[TEXT_FONTS_TOTAL] = {0};
    if (load_fonts(lang, next)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't load the fonts of %s, keeping %s.",
                     languages[lang].name,
                     languages[loaded_language].name);
        SDL_CompareAndSwapAtomicInt(&requested_language, lang, loaded_language);
        return;
    }

    loaded_font old[TEXT_FONTS_TOTAL];
    SDL_LockRWLockForWriting(lock);
    SDL_memcpy(old, fonts, sizeof(fonts));
    SDL_memcpy(fonts, next, sizeof(fonts));
    loaded_language = lang;
    SDL_UnlockRWLock(lock);
    for (int i = 0; i < TEXT_FONTS_TOTAL; i++)
        unload_font(&old[i]);
}

void TEXT_set_language(language lang) {
    SDL_SetAtomicInt(&requested_language, lang);
}

language TEXT_get_language(void) {
    return SDL_GetAtomicInt(&requested_language);
}



// How far apart `f`'s lines are with `line_spacing`.
static int line_advance(const loaded_font* f, int line_spacing) {
    const text_font_metrics* m = &f->metrics;
    return m->ascent + m->descent + m->line_gap + line_spacing;
}

// Calls `each` with every glyph of `text` with a bitmap and the rect its bitmap
// is drawn on, laid out as `TEXT_draw()` says.
static void layout(const loaded_font* f,
                   float x,
                   float y,
                   const char* text,
                   int line_spacing,
                   void (*each)(const glyph* g, SDL_FRect dst, void* arg),
                   void* arg) {
    float pen_x = x;
    float baseline = y + f->metrics.ascent;
    Uint32 codepoint;
    while ((codepoint = SDL_StepUTF8(&text, NULL))) {
        if (codepoint == '\n') {
            pen_x = x;
            baseline += line_advance(f, line_spacing);
            continue;
        }
        const glyph* g = get_glyph(f, codepoint);
        if (g->bitmap_w)
            each(g,
                 (SDL_FRect){pen_x + g->x_offset,
                             baseline - g->y_offset,
                             g->bitmap_w,
                             g->bitmap_h},
                 arg);
        pen_x += g->w;
    }
}

typedef struct {
    graphics_texture* atlas;
    SDL_FColor col;
    Sint8 z_index;
} glyph_arg;

static void queue_glyph(const glyph* g, SDL_FRect dst, void* arg) {
    glyph_arg* a = arg;
    RENDERER_draw_from_texture(
        a->atlas,
        (SDL_FRect){g->x, g->y, g->bitmap_w, g->bitmap_h},
        dst,
        SDL_FLIP_NONE,
        false,
        a->col,
        a->z_index);
}

static void draw_glyph_now(const glyph* g, SDL_FRect dst, void* arg) {
    glyph_arg* a = arg;
    GRAPHICS_draw_texture(a->atlas,
                          &(SDL_FRect){g->x, g->y, g->bitmap_w, g->bitmap_h},
                          &dst,
                          SDL_FLIP_NONE,
                          a->col);
}

// Adds `dst`, the rect of a glyph's bitmap, which is trimmed to the pixels it
// sets, to the `SDL_Rect` at `arg`.
static void add_ink([[maybe_unused]] const glyph* g, SDL_FRect dst, void* arg) {
    SDL_Rect* ink = arg;
    SDL_Rect r = {(int)dst.x, (int)dst.y, (int)dst.w, (int)dst.h};
    if (SDL_RectEmpty(ink))
        *ink = r;
    else
        SDL_GetRectUnion(ink, &r, ink);
}

void TEXT_draw(float x,
               float y,
               const char* text,
               text_font font,
               int line_spacing,
               SDL_FColor col,
               Sint8 z_index) {
    SDL_LockRWLockForReading(lock);
    layout(&fonts[font],
           x,
           y,
           text,
           line_spacing,
           queue_glyph,
           &(glyph_arg){fonts[font].atlas, col, z_index});
    SDL_UnlockRWLock(lock);
}

void TEXT_draw_now(float x,
                   float y,
                   const char* text,
                   text_font font,
                   int line_spacing,
                   SDL_FColor col) {
    SDL_LockRWLockForReading(lock);
    layout(&fonts[font],
           x,
           y,
           text,
           line_spacing,
           draw_glyph_now,
           &(glyph_arg){fonts[font].atlas, col, 0});
    SDL_UnlockRWLock(lock);
}

void TEXT_measure(
    const char* text, text_font font, int line_spacing, int* w, int* h) {
    SDL_LockRWLockForReading(lock);
    const loaded_font* f = &fonts[font];
    *w = 0;
    *h = *text ? f->metrics.ascent + f->metrics.descent : 0;
    int line_w = 0;
    Uint32 codepoint;
    while ((codepoint = SDL_StepUTF8(&text, NULL))) {
        if (codepoint == '\n') {
            line_w = 0;
            *h += line_advance(f, line_spacing);
            continue;
        }
        line_w += get_glyph(f, codepoint)->w;
        *w = SDL_max(*w, line_w);
    }
    SDL_UnlockRWLock(lock);
}

SDL_Rect TEXT_measure_ink(const char* text, text_font font, int line_spacing) {
    SDL_Rect ink = {0};
    SDL_LockRWLockForReading(lock);
    layout(&fonts[font], 0.0f, 0.0f, text, line_spacing, add_ink, &ink);
    SDL_UnlockRWLock(lock);
    return ink;
}

text_font_metrics TEXT_get_font_metrics(text_font font) {
    SDL_LockRWLockForReading(lock);
    text_font_metrics m = fonts[font].metrics;
    SDL_UnlockRWLock(lock);
    return m;
}
