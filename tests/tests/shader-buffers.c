#include "tests/shader-buffers.h"
#include "defs.h"
#include "engine/particles.h"
#include "tests.h"
#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdio.h>

// Checks that the structs passed to shaders are laid out the same on the CPU
// and on the GPU, by parsing their definitions on the C and HLSL sources.
//
// Every struct must have its 4D fields first, then its 2D ones, then its 4-byte
// ones, and be a multiple of 16 bytes, padded with `int`s. That way, every
// field is aligned to its own size and never crosses a 16 bytes boundary, so
// they're laid out the same in C, on cbuffers and on structured buffers, on
// both SPIR-V and DXIL. Arrays are only allowed of 4D fields, since cbuffers
// pad each element of an array to 16 bytes.
//
// Fields are one per line, ending on `;`, with anything after it ignored, so
// it can have a comment. Empty lines and lines starting with `//` are skipped.
// Types are `float`, `int` and `uint` with an optional width of `2` or `4` on
// HLSL, and on C `float`, `int`, `Sint32` and `Uint32` with an optional last
// dimension of `2` or `4` for their width, or `SDL_FColor`. Array sizes can be
// a number or a define on `defines`. Names are compared without underscores and
// case, so `_CamPos` matches `cam_pos`.

#define MAX_FIELDS 64
#define MAX_TOKEN 64
#define MAX_DIMS 4

// Defines array sizes can use, by name. The values are the defines themselves,
// so they're always up to date.
typedef struct {
    const char* name;
    int value;
} define;

static const define defines[] = {
    {"FLUIDS_TOTAL", FLUIDS_TOTAL},
};

// A C struct, the `typedef struct { ... } name;` of `name` on `path`, which
// must match the HLSL struct or cbuffer declared as `decl`, like `struct
// Request` or `cbuffer Uniforms`, on `hlsl_path`.
typedef struct {
    const char* path;
    const char* name;
    const char* hlsl_path;
    const char* decl;
} struct_spec;

// An HLSL struct, declared as `decl` on `path`, that has no C equivalent but
// that C allocates buffers of, `size` bytes each, `size_name` being where that
// size comes from.
typedef struct {
    const char* path;
    const char* decl;
    size_t size;
    const char* size_name;
} size_spec;

// A field of an indirect draw struct, which must have the same name and offset
// as on SDL's struct.
typedef struct {
    const char* name;
    size_t offset;
} sdl_field;

// An HLSL struct, declared as `decl` on `path`, that the GPU writes indirect
// draw arguments on, which must match SDL's `SDL_GPUIndirectDrawCommand`.
typedef struct {
    const char* path;
    const char* decl;
} indirect_spec;

static const struct_spec struct_specs[] = {
    // Particles
    {"src/engine/particles.c",
     "gpu_type",
     "src/gpu/includes/particles-common.hlsli",
     "struct ParticleType"},
    {"src/engine/particles.c",
     "request",
     "src/gpu/includes/particles-common.hlsli",
     "struct Request"},
    {"src/engine/particles.c",
     "compute_uniforms",
     "src/gpu/includes/particles-uniforms.hlsli",
     "cbuffer Uniforms"},
    {"src/engine/particles.c",
     "vertex_uniforms",
     "src/gpu/particles.vert.hlsl",
     "cbuffer Uniforms"},

    // Fluids
    {"src/engine/fluids.c",
     "shader_uniforms",
     "src/gpu/includes/fluids-uniforms.hlsli",
     "cbuffer Uniforms"},
    {"src/engine/fluids.c",
     "ripples_uniform",
     "src/gpu/ripples.frag.hlsl",
     "cbuffer Uniforms"},

    // Bloom
    {"src/engine/renderer.c",
     "bloom_prefilter_uniforms",
     "src/gpu/bloom-prefilter.frag.hlsl",
     "cbuffer Uniforms"},
    {"src/engine/renderer.c",
     "box_sample_uniforms",
     "src/gpu/box-sample.frag.hlsl",
     "cbuffer Uniforms"},

    // Target size, the first uniform buffer of every vertex shader
    {"src/engine/graphics.c",
     "target_uniforms",
     "src/gpu/base.vert.hlsl",
     "cbuffer TargetUniforms"},
    {"src/engine/graphics.c",
     "target_uniforms",
     "src/gpu/particles.vert.hlsl",
     "cbuffer TargetUniforms"},
    {"src/engine/graphics.c",
     "target_uniforms",
     "tests/mocks/shaders/rects.vert.hlsl",
     "cbuffer TargetUniforms"},

    // Graphics tests
    {"tests/tests/graphics.c",
     "rects_comp_uniforms",
     "tests/mocks/shaders/rects.comp.hlsl",
     "cbuffer Uniforms"},
};

static const size_spec size_specs[] = {
    {"src/gpu/includes/particles-common.hlsli",
     "struct Particle",
     PARTICLE_SIZE,
     "PARTICLE_SIZE"},
};

static const indirect_spec indirect_specs[] = {
    {"src/gpu/includes/particles-common.hlsli", "struct DrawArgs"},
};

static const sdl_field indirect_fields[] = {
    {"num_vertices", offsetof(SDL_GPUIndirectDrawCommand, num_vertices)},
    {"num_instances", offsetof(SDL_GPUIndirectDrawCommand, num_instances)},
    {"first_vertex", offsetof(SDL_GPUIndirectDrawCommand, first_vertex)},
    {"first_instance", offsetof(SDL_GPUIndirectDrawCommand, first_instance)},
};

typedef enum {
    base_float,
    base_int,
    base_uint,
} base_type;

// A field of a struct, `width` 4-byte values wide, and `count` of them when
// it's an array, `1` otherwise.
typedef struct {
    char name[MAX_TOKEN];
    base_type base;
    int width;
    int count;
    size_t line;
} field;

// A struct whose declaration is on line `line` of `path`.
typedef struct {
    const char* path;
    size_t line;
    field fields[MAX_FIELDS];
    size_t len;
} parsed_struct;

// A file split into its lines, without their trailing whitespace.
typedef struct {
    char* buf;
    char** lines;
    size_t len;
} source;

static bool load_source(report* r, const char* path, source* s) {
    *s = (source){0};
    s->buf = SDL_LoadFile(path, NULL);
    check(r, s->buf, "Couldn't read %s: %s", path, SDL_GetError());
    if (!s->buf) return false;

    size_t n = 1;
    for (char* c = s->buf; *c; c++)
        if (*c == '\n') n++;
    s->lines = SDL_malloc(n * sizeof(char*));
    char* line = s->buf;
    while (line) {
        char* next = SDL_strchr(line, '\n');
        if (next) *next++ = '\0';
        size_t len = SDL_strlen(line);
        while (len > 0 && SDL_isspace(line[len - 1]))
            line[--len] = '\0';
        s->lines[s->len++] = line;
        line = next;
    }
    return true;
}

static void free_source(source* s) {
    SDL_free(s->lines);
    SDL_free(s->buf);
    *s = (source){0};
}

static const char* skip_spaces(const char* s) {
    while (SDL_isspace(*s))
        s++;
    return s;
}

static bool starts_with(const char* s, const char* prefix) {
    return SDL_strncmp(s, prefix, SDL_strlen(prefix)) == 0;
}

static bool is_ident_char(char c) {
    return SDL_isalnum(c) || c == '_';
}

// Index of the line `} name;` closing the C struct `name`, or `0` when it isn't
// found exactly once, which is reported. Line `0` can't close a struct.
static size_t find_c_struct_end(report* r,
                                const source* s,
                                const char* path,
                                const char* name) {
    char closing[MAX_TOKEN * 2] = "";
    SDL_snprintf(closing, sizeof(closing), "} %s;", name);
    size_t found = 0, count = 0;
    for (size_t i = 0; i < s->len; i++)
        if (SDL_strcmp(skip_spaces(s->lines[i]), closing) == 0) {
            found = i;
            count++;
        }
    check(r,
          count == 1,
          "%s: found `%s` %zu times, expected once",
          path,
          closing,
          count);
    return count == 1 ? found : 0;
}

// Index of the line declaring `decl` on HLSL, or `-1` when it isn't found
// exactly once, which is reported. `decl` must be followed by a space, `{` or
// `:`, so `cbuffer Uniforms` doesn't match `cbuffer UniformsB`.
static long
find_hlsl_decl(report* r, const source* s, const char* path, const char* decl) {
    size_t decl_len = SDL_strlen(decl);
    long found = -1;
    size_t count = 0;
    for (size_t i = 0; i < s->len; i++) {
        const char* line = skip_spaces(s->lines[i]);
        if (!starts_with(line, decl)) continue;
        char after = line[decl_len];
        if (after != ' ' && after != '{' && after != ':') continue;
        found = i;
        count++;
    }
    check(r,
          count == 1,
          "%s: found `%s` %zu times, expected once",
          path,
          decl,
          count);
    return count == 1 ? found : -1;
}

// Reads an identifier at `*s` into `out`, advancing `*s` past it. Returns
// whether there was one.
static bool read_ident(const char** s, char out[MAX_TOKEN]) {
    *s = skip_spaces(*s);
    size_t len = 0;
    while (is_ident_char(**s) && len < MAX_TOKEN - 1)
        out[len++] = *(*s)++;
    out[len] = '\0';
    return len > 0 && !is_ident_char(**s);
}

// Value of an array dimension, a number or a define on `defines`, or `-1` when
// it's neither.
static int dim_value(const char* dim) {
    char* end = NULL;
    long n = SDL_strtol(dim, &end, 10);
    if (dim[0] && *end == '\0') return n > 0 ? (int)n : -1;
    for (size_t i = 0; i < SDL_arraysize(defines); i++)
        if (SDL_strcmp(defines[i].name, dim) == 0) return defines[i].value;
    return -1;
}

// Parses the type of a C field, `type` with dimensions `dims`, into `f`, or
// returns an error message.
static const char* parse_c_type(const char* type,
                                char dims[MAX_DIMS][MAX_TOKEN],
                                size_t n,
                                field* f) {
    f->width = 1;
    f->count = 1;
    if (SDL_strcmp(type, "SDL_FColor") == 0) {
        f->base = base_float;
        f->width = 4;
        if (n > 1) return "has too many dimensions";
        if (n == 1) f->count = dim_value(dims[0]);
        return NULL;
    }
    if (SDL_strcmp(type, "float") == 0)
        f->base = base_float;
    else if (SDL_strcmp(type, "int") == 0 || SDL_strcmp(type, "Sint32") == 0)
        f->base = base_int;
    else if (SDL_strcmp(type, "Uint32") == 0)
        f->base = base_uint;
    else
        return "has an unsupported type, expected float, int, Sint32, Uint32 "
               "or SDL_FColor";

    if (n == 0) return NULL;
    if (n > 2) return "has too many dimensions";
    // The last dimension is the width, as a number and not a define.
    const char* width = dims[n - 1];
    if (SDL_strcmp(width, "2") == 0)
        f->width = 2;
    else if (SDL_strcmp(width, "4") == 0)
        f->width = 4;
    else
        return "has an unsupported last dimension, expected 2 or 4 for its "
               "width, since arrays are only allowed of 4D fields";
    if (n == 2) f->count = dim_value(dims[0]);
    return NULL;
}

// Parses the type of an HLSL field, `type` with dimensions `dims`, into `f`, or
// returns an error message.
static const char* parse_hlsl_type(const char* type,
                                   char dims[MAX_DIMS][MAX_TOKEN],
                                   size_t n,
                                   field* f) {
    const char* rest = type;
    if (starts_with(type, "float")) {
        f->base = base_float;
        rest += 5;
    } else if (starts_with(type, "uint")) {
        f->base = base_uint;
        rest += 4;
    } else if (starts_with(type, "int")) {
        f->base = base_int;
        rest += 3;
    } else {
        return "has an unsupported type, expected float, int or uint";
    }
    if (rest[0] == '\0')
        f->width = 1;
    else if (SDL_strcmp(rest, "2") == 0)
        f->width = 2;
    else if (SDL_strcmp(rest, "4") == 0)
        f->width = 4;
    else
        return "has an unsupported type, expected a width of 2 or 4, or none";

    f->count = 1;
    if (n > 1) return "has too many dimensions";
    if (n == 1) f->count = dim_value(dims[0]);
    return NULL;
}

// Parses the field on `line` into `f`, or returns an error message.
static const char* parse_field(const char* line, bool hlsl, field* f) {
    if (line[0] == '#') return "is a preprocessor directive, not a field";
    const char* end = SDL_strchr(line, ';');
    if (!end) return "has no `;`, expected a single field per line";

    char decl[FAIL_MESSAGE_MAX] = "";
    size_t len = SDL_min((size_t)(end - line), sizeof(decl) - 1);
    SDL_memcpy(decl, line, len);
    decl[len] = '\0';
    if (SDL_strstr(decl, "/*")) return "has a block comment";
    if (SDL_strchr(decl, ',')) return "declares more than one field";

    const char* s = decl;
    char type[MAX_TOKEN] = "";
    if (!read_ident(&s, type)) return "has no type";
    if (!read_ident(&s, f->name)) return "has no name";

    char dims[MAX_DIMS][MAX_TOKEN] = {0};
    size_t n = 0;
    s = skip_spaces(s);
    while (*s == '[') {
        if (n == MAX_DIMS) return "has too many dimensions";
        const char* close = SDL_strchr(s, ']');
        if (!close) return "has an unclosed `[`";
        const char* from = skip_spaces(s + 1);
        size_t dim_len = SDL_min((size_t)(close - from), MAX_TOKEN - 1);
        SDL_memcpy(dims[n], from, dim_len);
        while (dim_len > 0 && SDL_isspace(dims[n][dim_len - 1]))
            dim_len--;
        dims[n][dim_len] = '\0';
        n++;
        s = skip_spaces(close + 1);
    }
    if (*s != '\0') return "has something unexpected after its name";

    const char* err = hlsl ? parse_hlsl_type(type, dims, n, f)
                           : parse_c_type(type, dims, n, f);
    if (err) return err;
    if (f->count < 0)
        return "has an array size that's neither a number nor a define on "
               "`defines` of `tests/tests/shader-buffers.c`";
    if (f->count > 1 && f->width != 4)
        return "is an array of non-4D values, which cbuffers pad to 16 bytes "
               "each";
    return NULL;
}

// Parses the fields on lines `from` up to the first one starting with `}`, or
// up to `to` when it's not `0`, into `out`. Returns whether there were no
// errors, which are reported.
static bool parse_fields(report* r,
                         const source* s,
                         const char* path,
                         size_t from,
                         size_t to,
                         bool hlsl,
                         parsed_struct* out) {
    bool ok = true;
    bool closed = false;
    size_t end = to ? to : s->len;
    for (size_t i = from; i < end; i++) {
        const char* line = skip_spaces(s->lines[i]);
        if (line[0] == '\0' || starts_with(line, "//")) continue;
        if (!to && line[0] == '}') {
            closed = true;
            break;
        }
        if (out->len == MAX_FIELDS) {
            check(r, false, "%s:%zu: too many fields", path, i + 1);
            return false;
        }
        field* f = &out->fields[out->len];
        *f = (field){.line = i + 1};
        const char* err = parse_field(line, hlsl, f);
        check(r, !err, "%s:%zu: `%s` %s", path, i + 1, line, err);
        if (err)
            ok = false;
        else
            out->len++;
    }
    if (!to) check(r, closed, "%s:%zu: struct is never closed", path, from);
    return ok && (to || closed);
}

// Parses the C struct `name` on `s`. Returns whether it was parsed.
static bool parse_c_struct(report* r,
                           const source* s,
                           const char* path,
                           const char* name,
                           parsed_struct* out) {
    *out = (parsed_struct){.path = path};
    size_t end = find_c_struct_end(r, s, path, name);
    if (!end) return false;
    size_t start = end;
    while (start > 0 &&
           SDL_strcmp(skip_spaces(s->lines[start - 1]), "typedef struct {"))
        start--;
    check(r,
          start > 0,
          "%s:%zu: `} %s;` has no `typedef struct {` before it",
          path,
          end + 1,
          name);
    if (start == 0) return false;
    out->line = start;
    return parse_fields(r, s, path, start, end, false, out);
}

// Parses the HLSL struct or cbuffer `decl` on `s`. Returns whether it was
// parsed.
static bool parse_hlsl_struct(report* r,
                              const source* s,
                              const char* path,
                              const char* decl,
                              parsed_struct* out) {
    *out = (parsed_struct){.path = path};
    long start = find_hlsl_decl(r, s, path, decl);
    if (start < 0) return false;
    bool opened = SDL_strchr(s->lines[start], '{');
    check(r,
          opened,
          "%s:%ld: `%s` should open its `{` on the same line",
          path,
          start + 1,
          decl);
    if (!opened) return false;
    out->line = start + 1;
    return parse_fields(r, s, path, start + 1, 0, true, out);
}

static const char* section_name(int width) {
    if (width == 4) return "4D";
    if (width == 2) return "2D";
    return "4-byte";
}

// Writes the type of `f` the way HLSL writes it, with its array size, like
// `float4[2]`, so C and HLSL types read the same on messages.
static void describe(const field* f, char out[MAX_TOKEN]) {
    const char* base = f->base == base_float ? "float"
                       : f->base == base_int ? "int"
                                             : "uint";
    char width[4] = "";
    if (f->width > 1) SDL_snprintf(width, sizeof(width), "%d", f->width);
    if (f->count > 1)
        SDL_snprintf(out, MAX_TOKEN, "%s%s[%d]", base, width, f->count);
    else
        SDL_snprintf(out, MAX_TOKEN, "%s%s", base, width);
}

// Checks `p` has its 4D fields first, then the 2D ones, then the 4-byte ones,
// and that it's a multiple of 16 bytes. Returns its size.
static size_t check_layout(report* r, const parsed_struct* p) {
    int last_width = 4;
    size_t size = 0;
    for (size_t i = 0; i < p->len; i++) {
        const field* f = &p->fields[i];
        check(r,
              f->width <= last_width,
              "%s:%zu: `%s` is %s after %s fields, expected 4D fields first, "
              "then 2D, then 4-byte",
              p->path,
              f->line,
              f->name,
              section_name(f->width),
              section_name(last_width));
        last_width = SDL_min(last_width, f->width);
        size += (size_t)f->width * f->count * 4;
    }
    check(r,
          size > 0 && size % 16 == 0,
          "%s:%zu: struct is %zu bytes, expected a multiple of 16, pad it with "
          "`int`s",
          p->path,
          p->line,
          size);
    return size;
}

// Whether names `a` and `b` are the same, ignoring underscores and case.
static bool same_name(const char* a, const char* b) {
    while (true) {
        while (*a == '_')
            a++;
        while (*b == '_')
            b++;
        if (SDL_tolower(*a) != SDL_tolower(*b)) return false;
        if (*a == '\0') return true;
        a++;
        b++;
    }
}

static void
check_fields_match(report* r, const parsed_struct* c, const parsed_struct* h) {
    check(r,
          c->len == h->len,
          "%s:%zu has %zu fields but %s:%zu has %zu",
          c->path,
          c->line,
          c->len,
          h->path,
          h->line,
          h->len);
    for (size_t i = 0; i < SDL_min(c->len, h->len); i++) {
        const field* cf = &c->fields[i];
        const field* hf = &h->fields[i];
        check(r,
              same_name(cf->name, hf->name),
              "%s:%zu: field %zu is `%s`, but on %s:%zu it's `%s`",
              c->path,
              cf->line,
              i + 1,
              cf->name,
              h->path,
              hf->line,
              hf->name);
        char ct[MAX_TOKEN] = "", ht[MAX_TOKEN] = "";
        describe(cf, ct);
        describe(hf, ht);
        check(r,
              cf->base == hf->base && cf->width == hf->width &&
                  cf->count == hf->count,
              "%s:%zu: `%s` is %s, but on %s:%zu `%s` is %s",
              c->path,
              cf->line,
              cf->name,
              ct,
              h->path,
              hf->line,
              hf->name,
              ht);
    }
}

static void check_struct(const struct_spec* spec) {
    report r = {0};
    source cs = {0}, hs = {0};
    parsed_struct c = {0}, h = {0};
    bool c_ok = load_source(&r, spec->path, &cs) &&
                parse_c_struct(&r, &cs, spec->path, spec->name, &c);
    bool h_ok = load_source(&r, spec->hlsl_path, &hs) &&
                parse_hlsl_struct(&r, &hs, spec->hlsl_path, spec->decl, &h);
    if (c_ok) check_layout(&r, &c);
    if (h_ok) check_layout(&r, &h);
    if (c_ok && h_ok) check_fields_match(&r, &c, &h);
    free_source(&cs);
    free_source(&hs);

    char desc[FAIL_MESSAGE_MAX] = "";
    SDL_snprintf(desc,
                 sizeof(desc),
                 "Checking `%s` on %s against `%s` on %s...",
                 spec->name,
                 spec->path,
                 spec->decl,
                 spec->hlsl_path);
    finish_report(desc, &r);
}

static void check_size(const size_spec* spec) {
    report r = {0};
    source s = {0};
    parsed_struct h = {0};
    if (load_source(&r, spec->path, &s) &&
        parse_hlsl_struct(&r, &s, spec->path, spec->decl, &h)) {
        size_t size = check_layout(&r, &h);
        check(&r,
              size == spec->size,
              "%s:%zu: struct is %zu bytes, but `%s` is %zu",
              spec->path,
              h.line,
              size,
              spec->size_name,
              spec->size);
    }
    free_source(&s);

    char desc[FAIL_MESSAGE_MAX] = "";
    SDL_snprintf(desc,
                 sizeof(desc),
                 "Checking `%s` on %s against `%s`...",
                 spec->decl,
                 spec->path,
                 spec->size_name);
    finish_report(desc, &r);
}

static void check_indirect(const indirect_spec* spec) {
    report r = {0};
    source s = {0};
    parsed_struct h = {0};
    if (load_source(&r, spec->path, &s) &&
        parse_hlsl_struct(&r, &s, spec->path, spec->decl, &h)) {
        size_t size = check_layout(&r, &h);
        check(&r,
              size == sizeof(SDL_GPUIndirectDrawCommand),
              "%s:%zu: struct is %zu bytes, but `SDL_GPUIndirectDrawCommand` "
              "is %zu",
              spec->path,
              h.line,
              size,
              sizeof(SDL_GPUIndirectDrawCommand));
        check(&r,
              h.len == SDL_arraysize(indirect_fields),
              "%s:%zu: struct has %zu fields, but "
              "`SDL_GPUIndirectDrawCommand` has %zu",
              spec->path,
              h.line,
              h.len,
              SDL_arraysize(indirect_fields));
        size_t offset = 0;
        for (size_t i = 0; i < h.len; i++) {
            const field* f = &h.fields[i];
            const sdl_field* sf = NULL;
            for (size_t j = 0; j < SDL_arraysize(indirect_fields); j++)
                if (same_name(f->name, indirect_fields[j].name))
                    sf = &indirect_fields[j];
            check(&r,
                  sf,
                  "%s:%zu: `%s` isn't on `SDL_GPUIndirectDrawCommand`",
                  spec->path,
                  f->line,
                  f->name);
            if (sf)
                check(&r,
                      sf->offset == offset,
                      "%s:%zu: `%s` is at byte %zu, but on "
                      "`SDL_GPUIndirectDrawCommand` it's at %zu",
                      spec->path,
                      f->line,
                      f->name,
                      offset,
                      sf->offset);
            offset += (size_t)f->width * f->count * 4;
        }
    }
    free_source(&s);

    char desc[FAIL_MESSAGE_MAX] = "";
    SDL_snprintf(desc,
                 sizeof(desc),
                 "Checking `%s` on %s against `SDL_GPUIndirectDrawCommand`...",
                 spec->decl,
                 spec->path);
    finish_report(desc, &r);
}

void test_shader_buffers(void) {
    printf(YELLOW "\nTesting shader buffers integrity...\n" RESET);

    for (size_t i = 0; i < SDL_arraysize(struct_specs); i++)
        check_struct(&struct_specs[i]);
    for (size_t i = 0; i < SDL_arraysize(size_specs); i++)
        check_size(&size_specs[i]);
    for (size_t i = 0; i < SDL_arraysize(indirect_specs); i++)
        check_indirect(&indirect_specs[i]);
}
