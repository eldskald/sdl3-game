#include "engine/renderer.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/graphics.h"
#include "engine/sprites.h"
#include "engine/tilemap.h"
#include "props/vines.h"
#include "services/csv.h"
#include <SDL3/SDL.h>

#ifdef DEV
#include "dev/profiler.h"
#endif

// The vertices queued on a z_index, in the order they were queued, all drawn
// with the spritesheet.
typedef struct {
    graphics_vertex* vertices;
    size_t len, cap;
} draw_queue;

// Matches the uniforms on `src/gpu/bloom-prefilter.frag.hlsl`.
typedef struct {
    float threshold;
    float soft_threshold;
} bloom_prefilter_uniforms;

// Matches the uniforms on `src/gpu/box-sample.frag.hlsl`.
typedef struct {
    float delta;
    float intensity;
} box_sample_uniforms;

// The spritesheet has an extra row of pixels at its bottom, with a white pixel
// on its first one, at `white_u,white_v`. Lines and triangles are drawn with
// it, so everything queued is drawn with the spritesheet.
static graphics_texture* spritesheet = NULL;
static float white_u = 0.0f;
static float white_v = 0.0f;
static graphics_texture* base_screen = NULL;
static graphics_texture* bloom_prefilter = NULL;
static graphics_texture* bloom_chain[RENDERER_BLOOM_MAX_ITERATIONS] = {0};
static graphics_texture* pre_fluids_texture = NULL;
static SDL_GPUShader* tonemap_shader = NULL;
static SDL_GPUShader* bloom_prefilter_shader = NULL;
static SDL_GPUShader* box_sample_shader = NULL;
#ifdef DEV
// The profiler draws with SDL's debug text, so it's drawn by a software
// renderer onto profiler_surface, which is uploaded to profiler_texture and
// drawn over the window.
static SDL_Surface* profiler_surface = NULL;
static SDL_Renderer* profiler_renderer = NULL;
static graphics_texture* profiler_texture = NULL;
#endif
static fluids_canvas fluids = (fluids_canvas){0};
static float bloom_threshold = 1.0f;
static float bloom_soft_threshold = 0.5f;
static int bloom_iterations = 4;
static float bloom_intensities[RENDERER_BLOOM_MAX_ITERATIONS] = {0};

// Each z_index is mapped to z_index + RENDERER_Z_INDEX_TOTAL / 2, so they all
// fit in the array.
static draw_queue queues[RENDERER_Z_INDEX_TOTAL] = {0};

static const SDL_FColor white = {1.0f, 1.0f, 1.0f, 1.0f};
static const SDL_FColor black = {0.0f, 0.0f, 0.0f, 1.0f};



// Float render target, so colors can go beyond 1.0. Nothing is converted
// between colorspaces, colors are sRGB everywhere.
static graphics_texture*
create_hdr_target(const char* name, int w, int h, SDL_GPUFilter filter) {
    return GRAPHICS_create_texture(
        name, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, w, h, filter);
}

static int create_textures(void) {
    // Texture for the base game. It's HDR for post processing, and tone mapped
    // onto the window.
    base_screen = create_hdr_target(
        "base_screen", WINDOW_SIZE_X, WINDOW_SIZE_Y, SDL_GPU_FILTER_NEAREST);
    if (!base_screen) return 1;

    // Texture for the parts of base_screen bright enough to bloom. It's
    // filtered linearly for the downsampling to blur it.
    bloom_prefilter = create_hdr_target(
        "bloom_prefilter", WINDOW_SIZE_X, WINDOW_SIZE_Y, SDL_GPU_FILTER_LINEAR);
    if (!bloom_prefilter) return 1;

    // Textures for the bloom's downsampling chain, each half the size of the
    // previous one, starting at half of bloom_prefilter. Also filtered
    // linearly. The chain is cut short when it would get too small.
    for (int i = 0; i < bloom_iterations; i++) {
        int w = WINDOW_SIZE_X >> (i + 1);
        int h = WINDOW_SIZE_Y >> (i + 1);
        if (w < 1 || h < 1) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU,
                        "Bloom iterations cut from %d to %d, the screen is "
                        "too small for more.",
                        bloom_iterations,
                        i);
            bloom_iterations = i;
            break;
        }
        bloom_chain[i] =
            create_hdr_target("bloom_chain", w, h, SDL_GPU_FILTER_LINEAR);
        if (!bloom_chain[i]) return 1;
    }

    // Texture for everything drawn before the fluids, for them to refract.
    // It's HDR, since it's drawn as is on base_screen where there are no
    // fluids.
    pre_fluids_texture = create_hdr_target("pre_fluids_texture",
                                           WINDOW_SIZE_X,
                                           WINDOW_SIZE_Y,
                                           SDL_GPU_FILTER_NEAREST);
    if (!pre_fluids_texture) return 1;
    fluids.pre_fluids = pre_fluids_texture;

    // Texture for the fluids. It's bigger than the screen so the ripples can
    // be simulated off screen too.
    fluids.texture = GRAPHICS_create_texture(
        "fluids_texture",
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_X,
        FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_Y,
        SDL_GPU_FILTER_NEAREST);
    if (!fluids.texture) return 1;

    // Textures for the ripples, the same size as the fluids'. They are float
    // so heights can be negative.
    for (Uint8 i = 0; i < 2; i++) {
        fluids.ripples[i] =
            create_hdr_target("ripples",
                              FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_X,
                              FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_Y,
                              SDL_GPU_FILTER_NEAREST);
        if (!fluids.ripples[i]) return 1;
    }

#ifdef DEV
    profiler_surface = SDL_CreateSurface(RENDERER_PROFILER_OVERLAY_X,
                                         RENDERER_PROFILER_OVERLAY_Y,
                                         SDL_PIXELFORMAT_RGBA32);
    profiler_renderer = SDL_CreateSoftwareRenderer(profiler_surface);
    if (!profiler_renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create profiler renderer: %s",
                     SDL_GetError());
        return 1;
    }
    profiler_texture =
        GRAPHICS_create_texture("profiler_texture",
                                SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                RENDERER_PROFILER_OVERLAY_X,
                                RENDERER_PROFILER_OVERLAY_Y,
                                SDL_GPU_FILTER_NEAREST);
    if (!profiler_texture) return 1;
#endif

    return 0;
}

static void load_bloom_data_from_csv(void) {
    char csv[] = {
#embed RENDERER_BLOOM_CSV_PATH suffix(, '\0')
    };

    for (int i = 0; i < RENDERER_BLOOM_MAX_ITERATIONS; i++)
        bloom_intensities[i] = 1.0f;

    dynarr parsed = CSV_parse(csv);

    for (int i = 0; i < parsed.len; i++) {
        char* key = (char*)((dynarr*)parsed.at[i])->at[0];
        char* val = (char*)((dynarr*)parsed.at[i])->at[1];
        if (SDL_strcmp(key, "threshold") == 0)
            bloom_threshold = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "soft_threshold") == 0)
            bloom_soft_threshold = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "intensities") == 0) {
            dynarr* row = (dynarr*)parsed.at[i];
            bloom_iterations = row->len - 1;
            for (int j = 1; j < row->len && j <= RENDERER_BLOOM_MAX_ITERATIONS;
                 j++)
                bloom_intensities[j - 1] = SDL_strtod((char*)row->at[j], NULL);
        }
    }

    CSV_free(&parsed);

    bloom_iterations =
        SDL_clamp(bloom_iterations, 1, RENDERER_BLOOM_MAX_ITERATIONS);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Bloom data loaded successfully.");
}

static int load_spritesheet(void) {
    Uint8 mem[] = {
#embed SPRITESHEET_PATH
    };
    SDL_IOStream* stream = SDL_IOFromConstMem(mem, sizeof(mem));
    SDL_Surface* png = SDL_LoadPNG_IO(stream, true);
    SDL_Surface* surface =
        SDL_CreateSurface(png->w, png->h + 1, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(png, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(png, NULL, surface, NULL);
    SDL_FillSurfaceRect(surface, &(SDL_Rect){0, png->h, 1, 1}, 0xFFFFFFFF);
    white_u = 0.5f / surface->w;
    white_v = (png->h + 0.5f) / surface->h;
    SDL_DestroySurface(png);

    spritesheet = GRAPHICS_create_texture_from_surface(
        "spritesheet", surface, SDL_GPU_FILTER_NEAREST);
    SDL_DestroySurface(surface);
    return spritesheet ? 0 : 1;
}

static int load_shaders(void) {
    // Fluids shader, sampling one of the ripples textures and
    // pre_fluids_texture as well, see FLUIDS_draw()
    static const Uint8 fluids_spirv[] = {
#embed FLUIDS_FRAG_SPIRV_PATH
    };
    static const Uint8 fluids_dxil[] = {
#embed FLUIDS_FRAG_DXIL_PATH
    };
    fluids.shader = GRAPHICS_load_fragment_shader(
        "fluids_shader", GRAPHICS_SHADER_CODE(fluids_spirv, fluids_dxil), 3, 1);
    if (!fluids.shader) return 1;

    // Ripples shader, sampling the fluids texture as well
    static const Uint8 ripples_spirv[] = {
#embed FLUIDS_RIPPLES_FRAG_SPIRV_PATH
    };
    static const Uint8 ripples_dxil[] = {
#embed FLUIDS_RIPPLES_FRAG_DXIL_PATH
    };
    fluids.ripples_shader = GRAPHICS_load_fragment_shader(
        "ripples_shader",
        GRAPHICS_SHADER_CODE(ripples_spirv, ripples_dxil),
        2,
        1);
    if (!fluids.ripples_shader) return 1;

    static const Uint8 tonemap_spirv[] = {
#embed RENDERER_TONEMAP_FRAG_SPIRV_PATH
    };
    static const Uint8 tonemap_dxil[] = {
#embed RENDERER_TONEMAP_FRAG_DXIL_PATH
    };
    tonemap_shader = GRAPHICS_load_fragment_shader(
        "tonemap_shader",
        GRAPHICS_SHADER_CODE(tonemap_spirv, tonemap_dxil),
        1,
        0);
    if (!tonemap_shader) return 1;

    static const Uint8 bloom_prefilter_spirv[] = {
#embed RENDERER_BLOOM_PREFILTER_FRAG_SPIRV_PATH
    };
    static const Uint8 bloom_prefilter_dxil[] = {
#embed RENDERER_BLOOM_PREFILTER_FRAG_DXIL_PATH
    };
    bloom_prefilter_shader = GRAPHICS_load_fragment_shader(
        "bloom_prefilter_shader",
        GRAPHICS_SHADER_CODE(bloom_prefilter_spirv, bloom_prefilter_dxil),
        1,
        1);
    if (!bloom_prefilter_shader) return 1;

    static const Uint8 box_sample_spirv[] = {
#embed RENDERER_BOX_SAMPLE_FRAG_SPIRV_PATH
    };
    static const Uint8 box_sample_dxil[] = {
#embed RENDERER_BOX_SAMPLE_FRAG_DXIL_PATH
    };
    box_sample_shader = GRAPHICS_load_fragment_shader(
        "box_sample_shader",
        GRAPHICS_SHADER_CODE(box_sample_spirv, box_sample_dxil),
        1,
        1);
    if (!box_sample_shader) return 1;

    return 0;
}



static int get_queue_index(Sint8 z_index) {
    return z_index + (RENDERER_Z_INDEX_TOTAL >> 1);
}

// Makes room for `n` vertices at the end of the `z_index` queue and returns
// them, for the caller to write.
static graphics_vertex* queue_vertices(Sint8 z_index, size_t n) {
    draw_queue* q = &queues[get_queue_index(z_index)];
    if (q->len + n > q->cap) {
        while (q->len + n > q->cap)
            q->cap = q->cap ? q->cap << 1 : RENDERER_VERTICES_STARTING_CAP;
        q->vertices =
            SDL_realloc(q->vertices, q->cap * sizeof(graphics_vertex));
    }
    graphics_vertex* v = &q->vertices[q->len];
    q->len += n;
    return v;
}

static void queue_pixel(int x, int y, SDL_FColor col, Sint8 z_index) {
    GRAPHICS_quad(queue_vertices(z_index, 6),
                  (SDL_FRect){x, y, 1.0f, 1.0f},
                  (SDL_FRect){white_u, white_v, 0.0f, 0.0f},
                  col);
}

void RENDERER_draw_from_spritesheet(float x,
                                    float y,
                                    Uint8 coords_x,
                                    Uint8 coords_y,
                                    Uint8 coords_w,
                                    Uint8 coords_h,
                                    SDL_FlipMode flip_mode,
                                    SDL_FColor tint,
                                    Sint8 z_index) {
    SDL_FRect src = (SDL_FRect){
        .x = SPRITESHEET_CELL_X * coords_x,
        .y = SPRITESHEET_CELL_Y * coords_y,
        .w = SPRITESHEET_CELL_X * coords_w,
        .h = SPRITESHEET_CELL_Y * coords_h,
    };
    GRAPHICS_quad(queue_vertices(z_index, 6),
                  (SDL_FRect){x, y, src.w, src.h},
                  GRAPHICS_uv(spritesheet, src, flip_mode),
                  tint);
}

void RENDERER_draw_triangle(float x1,
                            float y1,
                            float x2,
                            float y2,
                            float x3,
                            float y3,
                            SDL_FColor col,
                            Sint8 z_index) {
    graphics_vertex* v = queue_vertices(z_index, 3);
    v[0] = (graphics_vertex){x1, y1, white_u, white_v, col};
    v[1] = (graphics_vertex){x2, y2, white_u, white_v, col};
    v[2] = (graphics_vertex){x3, y3, white_u, white_v, col};
}

// Same as SDL's `RenderLineBresenham()`, which `SDL_RenderLine()` uses by
// default, with the ends rounded to whole pixels, so both ends are drawn and a
// line from a pixel to itself is that pixel.
void RENDERER_draw_line(
    float x1, float y1, float x2, float y2, SDL_FColor col, Sint8 z_index) {
    int ax = SDL_roundf(x1);
    int ay = SDL_roundf(y1);
    int bx = SDL_roundf(x2);
    int by = SDL_roundf(y2);
    int dx = SDL_abs(bx - ax);
    int dy = SDL_abs(by - ay);

    int n, d, d_inc1, d_inc2, x_inc1, x_inc2, y_inc1, y_inc2;
    if (dx >= dy) {
        n = dx + 1;
        d = 2 * dy - dx;
        d_inc1 = 2 * dy;
        d_inc2 = 2 * (dy - dx);
        x_inc1 = 1;
        x_inc2 = 1;
        y_inc1 = 0;
        y_inc2 = 1;
    } else {
        n = dy + 1;
        d = 2 * dx - dy;
        d_inc1 = 2 * dx;
        d_inc2 = 2 * (dx - dy);
        x_inc1 = 0;
        x_inc2 = 1;
        y_inc1 = 1;
        y_inc2 = 1;
    }
    if (ax > bx) {
        x_inc1 = -x_inc1;
        x_inc2 = -x_inc2;
    }
    if (ay > by) {
        y_inc1 = -y_inc1;
        y_inc2 = -y_inc2;
    }

    int x = ax;
    int y = ay;
    for (int i = 0; i < n; i++) {
        queue_pixel(x, y, col, z_index);
        if (d < 0) {
            d += d_inc1;
            x += x_inc1;
            y += y_inc1;
        } else {
            d += d_inc2;
            x += x_inc2;
            y += y_inc2;
        }
    }
}

// Draws and empties the queues from index `from` up to but not including `to`,
// blended by alpha. Everything is drawn with the spritesheet and the same
// state, so it's all a single draw.
static void flush_queues(int from, int to) {
    GRAPHICS_set_state(&(graphics_state){.blend = graphics_blend_blend});
    for (int i = from; i < to; i++) {
        GRAPHICS_draw_vertices(spritesheet, queues[i].vertices, queues[i].len);
        queues[i].len = 0;
    }
    GRAPHICS_set_state(NULL);
}

// Everything queues its draw calls and the fluids are prepared on their own
// textures first. Then the draw calls under the fluids are drawn on
// pre_fluids_texture for the fluids to refract, and the rest are drawn over the
// fluids on base_screen. The fluids cover all of base_screen, so it isn't
// cleared.
static void render_base_screen(void) {
    TILEMAP_draw();
    VINES_draw();
    SPRITES_draw();
    FLUIDS_prepare(&fluids);

    GRAPHICS_set_target(pre_fluids_texture);
    GRAPHICS_clear(black);
    flush_queues(0, get_queue_index(FLUIDS_Z_INDEX));

    GRAPHICS_set_target(base_screen);
    FLUIDS_draw(&fluids);
    flush_queues(get_queue_index(FLUIDS_Z_INDEX), RENDERER_Z_INDEX_TOTAL);
}



// Bloom: the parts of base_screen bright enough to bloom are filtered onto
// bloom_prefilter, which is downsampled down bloom_chain, blurring it more each
// time. Then it's upsampled back up, each level multiplied by its intensity and
// added onto the one above it, the first one onto base_screen.
static void render_post_processing(void) {
    GRAPHICS_set_target(bloom_prefilter);
    GRAPHICS_set_state(&(graphics_state){
        .shader = bloom_prefilter_shader,
        .uniforms =
            &(bloom_prefilter_uniforms){bloom_threshold, bloom_soft_threshold},
        .uniforms_size = sizeof(bloom_prefilter_uniforms),
    });
    GRAPHICS_draw_texture(base_screen, NULL, NULL, SDL_FLIP_NONE, white);

    GRAPHICS_set_state(&(graphics_state){
        .shader = box_sample_shader,
        .uniforms = &(box_sample_uniforms){1.0f, 1.0f},
        .uniforms_size = sizeof(box_sample_uniforms),
    });
    graphics_texture* source = bloom_prefilter;
    for (int i = 0; i < bloom_iterations; i++) {
        GRAPHICS_set_target(bloom_chain[i]);
        GRAPHICS_draw_texture(source, NULL, NULL, SDL_FLIP_NONE, white);
        source = bloom_chain[i];
    }

    for (int i = bloom_iterations - 1; i >= 0; i--) {
        GRAPHICS_set_target(i > 0 ? bloom_chain[i - 1] : base_screen);
        GRAPHICS_set_state(&(graphics_state){
            .shader = box_sample_shader,
            .blend = graphics_blend_add,
            .uniforms = &(box_sample_uniforms){0.5f, bloom_intensities[i]},
            .uniforms_size = sizeof(box_sample_uniforms),
        });
        GRAPHICS_draw_texture(bloom_chain[i], NULL, NULL, SDL_FLIP_NONE, white);
    }
    GRAPHICS_set_state(NULL);
}



// base_screen is tone mapped straight onto the window, centered and stretched.
// It's sampled with nearest filtering, so it's the same as tone mapping it onto
// a texture of its size and stretching that.
static void render_final_screen(void) {
    GRAPHICS_set_target(NULL);
    GRAPHICS_clear(black);

    Uint32 window_w, window_h;
    if (!GRAPHICS_get_swapchain_size(&window_w, &window_h)) return;
    float window_ar = (float)window_w / (float)window_h;
    float base_ar = (float)WINDOW_SIZE_X / (float)WINDOW_SIZE_Y;

    int x, y, w, h;
    if (window_ar >= base_ar) {
        h = window_h;
        w = window_h * base_ar;
        y = 0;
        x = (window_w - w) / 2;
    } else {
        w = window_w;
        h = window_w / base_ar;
        x = 0;
        y = (window_h - h) / 2;
    }

    GRAPHICS_set_state(&(graphics_state){.shader = tonemap_shader});
    GRAPHICS_draw_texture(
        base_screen, NULL, &(SDL_FRect){x, y, w, h}, SDL_FLIP_NONE, white);
    GRAPHICS_set_state(NULL);

#ifdef DEV
    SDL_SetRenderDrawColor(profiler_renderer, 0, 0, 0, 0);
    SDL_RenderClear(profiler_renderer);
    PROFILER_draw(profiler_renderer);
    SDL_FlushRenderer(profiler_renderer);
    GRAPHICS_upload_surface(profiler_texture, profiler_surface);
    GRAPHICS_set_state(&(graphics_state){.blend = graphics_blend_blend});
    GRAPHICS_draw_texture(profiler_texture,
                          NULL,
                          &(SDL_FRect){0.0f,
                                       0.0f,
                                       RENDERER_PROFILER_OVERLAY_X,
                                       RENDERER_PROFILER_OVERLAY_Y},
                          SDL_FLIP_NONE,
                          white);
    GRAPHICS_set_state(NULL);
#endif
}



int RENDERER_start(void) {
    load_bloom_data_from_csv();
    if (GRAPHICS_start()) return 1;
    if (create_textures()) return 1;
    if (load_spritesheet()) return 1;
    if (load_shaders()) return 1;
    return 0;
}


void RENDERER_stop(void) {
    for (int i = 0; i < RENDERER_Z_INDEX_TOTAL; i++) {
        SDL_free(queues[i].vertices);
        queues[i] = (draw_queue){0};
    }

#ifdef DEV
    GRAPHICS_destroy_texture(profiler_texture);
    SDL_DestroyRenderer(profiler_renderer);
    SDL_DestroySurface(profiler_surface);
#endif
    GRAPHICS_release_shader(box_sample_shader);
    GRAPHICS_release_shader(bloom_prefilter_shader);
    GRAPHICS_release_shader(tonemap_shader);
    GRAPHICS_release_shader(fluids.ripples_shader);
    GRAPHICS_release_shader(fluids.shader);
    for (Uint8 i = 0; i < 2; i++)
        GRAPHICS_destroy_texture(fluids.ripples[i]);
    GRAPHICS_destroy_texture(fluids.texture);
    GRAPHICS_destroy_texture(pre_fluids_texture);
    for (int i = 0; i < bloom_iterations; i++)
        GRAPHICS_destroy_texture(bloom_chain[i]);
    GRAPHICS_destroy_texture(bloom_prefilter);
    GRAPHICS_destroy_texture(base_screen);
    GRAPHICS_destroy_texture(spritesheet);
    GRAPHICS_stop();
}



void RENDERER_update(void) {
    GRAPHICS_begin_frame();
    render_base_screen();
    render_post_processing();
    render_final_screen();
    GRAPHICS_end_frame();
}
