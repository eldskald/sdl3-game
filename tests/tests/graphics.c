// The real `src/engine/graphics.c` is included here instead of compiled on its
// own, since the stub at `tests/stubbed/graphics.c` replaces it on every other
// test. Its public functions are renamed so they don't clash with the stub's,
// and since `engine/graphics.h` is first included through it, every
// `GRAPHICS_*()` on this file is the real one. It also gives access to what it
// keeps static, like the recorded commands and vertices and the device. Nothing
// may include `engine/graphics.h` before it.
#define GRAPHICS_start real_GRAPHICS_start
#define GRAPHICS_stop real_GRAPHICS_stop
#define GRAPHICS_create_texture real_GRAPHICS_create_texture
#define GRAPHICS_create_texture_from_surface                                   \
    real_GRAPHICS_create_texture_from_surface
#define GRAPHICS_binding real_GRAPHICS_binding
#define GRAPHICS_destroy_texture real_GRAPHICS_destroy_texture
#define GRAPHICS_upload_surface real_GRAPHICS_upload_surface
#define GRAPHICS_load_fragment_shader real_GRAPHICS_load_fragment_shader
#define GRAPHICS_load_vertex_shader real_GRAPHICS_load_vertex_shader
#define GRAPHICS_release_shader real_GRAPHICS_release_shader
#define GRAPHICS_load_compute_pipeline real_GRAPHICS_load_compute_pipeline
#define GRAPHICS_release_compute_pipeline real_GRAPHICS_release_compute_pipeline
#define GRAPHICS_create_buffer real_GRAPHICS_create_buffer
#define GRAPHICS_destroy_buffer real_GRAPHICS_destroy_buffer
#define GRAPHICS_upload_buffer real_GRAPHICS_upload_buffer
#define GRAPHICS_begin_frame real_GRAPHICS_begin_frame
#define GRAPHICS_end_frame real_GRAPHICS_end_frame
#define GRAPHICS_get_swapchain_size real_GRAPHICS_get_swapchain_size
#define GRAPHICS_set_target real_GRAPHICS_set_target
#define GRAPHICS_clear real_GRAPHICS_clear
#define GRAPHICS_set_state real_GRAPHICS_set_state
#define GRAPHICS_draw_vertices real_GRAPHICS_draw_vertices
#define GRAPHICS_draw_texture real_GRAPHICS_draw_texture
#define GRAPHICS_fill_rect real_GRAPHICS_fill_rect
#define GRAPHICS_draw_instances real_GRAPHICS_draw_instances
#define GRAPHICS_dispatch real_GRAPHICS_dispatch
#define GRAPHICS_draw_point real_GRAPHICS_draw_point
#include "../../src/engine/graphics.c"

#include "tests.h"
#include "tests/graphics.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// By path, since linting puts `-Isrc` before `-Itests`, where `"paths.h"` is
// `src/paths.h`, without the test shaders' paths.
#include "../paths.h"

// Size of the targets drawn on, small enough to check every pixel.
#define TARGET_SIZE 8

// Pixels read back can be off by one from the expected colors, like half
// transparent blends landing between two values.
#define PIXEL_TOLERANCE 1

static const Uint8 rects_vert_spirv[] = {
#embed TEST_GRAPHICS_RECTS_VERT_SPIRV_PATH
};
static const Uint8 rects_vert_dxil[] = {
#embed TEST_GRAPHICS_RECTS_VERT_DXIL_PATH
};
static const Uint8 mask_frag_spirv[] = {
#embed TEST_GRAPHICS_MASK_FRAG_SPIRV_PATH
};
static const Uint8 mask_frag_dxil[] = {
#embed TEST_GRAPHICS_MASK_FRAG_DXIL_PATH
};
static const Uint8 rects_comp_spirv[] = {
#embed TEST_GRAPHICS_RECTS_COMP_SPIRV_PATH
};
static const Uint8 rects_comp_dxil[] = {
#embed TEST_GRAPHICS_RECTS_COMP_DXIL_PATH
};

static const SDL_Color black = {0, 0, 0, 255};
static const SDL_Color white_col = {255, 255, 255, 255};
static const SDL_Color red = {255, 0, 0, 255};
static const SDL_Color green = {0, 255, 0, 255};
static const SDL_Color blue = {0, 0, 255, 255};

// Uniforms of `tests/mocks/shaders/rects.comp.hlsl`.
typedef struct {
    float offset[2];
    Uint32 count;
    Uint32 pad0;
} rects_comp_uniforms;



// The command recorded at `i` this frame, zeroed past the end so checks on it
// fail instead of crashing.
static command* recorded(size_t i) {
    static command none = {0};
    none = (command){0};
    return i < commands.len ? dynarr_at(i, &commands) : &none;
}

// What the pipeline `p` was created from, zeroed when it wasn't.
static pipeline_entry* entry_of(SDL_GPUGraphicsPipeline* p) {
    static pipeline_entry none = {0};
    for (int i = 0; i < pipelines_len; i++)
        if (p && pipelines[i].pipeline == p) return &pipelines[i];
    return &none;
}

static bool fcolor_eq(SDL_FColor a, SDL_FColor b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool is_pass(const command* c, graphics_texture* t, bool clear) {
    return c->type == command_pass && c->pass.target == t &&
           c->pass.clear == clear;
}

// Whether `c` is a draw of `n` vertices from `first` on the vertex buffer,
// with `texture` and no extra samplers, storage buffers or uniforms.
static bool is_vertices_draw(const command* c,
                             graphics_texture* texture,
                             Uint32 first,
                             Uint32 n) {
    return c->type == command_draw &&
           c->draw.samplers[0].texture == texture->texture &&
           c->draw.samplers[0].sampler == texture->sampler &&
           c->draw.first_vertex == first && c->draw.num_vertices == n &&
           c->draw.num_instances == 1 && !c->draw.indirect_buffer &&
           c->draw.num_samplers == 1 && c->draw.num_storage_buffers == 0 &&
           c->draw.vertex_uniforms_size == 0 &&
           c->draw.fragment_uniforms_size == 0;
}

static bool is_entry(const pipeline_entry* e,
                     SDL_GPUShader* vertex,
                     SDL_GPUShader* fragment,
                     graphics_blend blend,
                     bool vertex_input) {
    return e->pipeline && e->vertex == vertex && e->fragment == fragment &&
           e->blend == blend &&
           e->format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM &&
           e->vertex_input == vertex_input;
}

// Whether the vertices recorded from `first` are the quad of `dst` with `uv`
// and `col`, as `GRAPHICS_quad()` makes it.
static bool is_quad(size_t first, SDL_FRect dst, SDL_FRect uv, SDL_FColor col) {
    if (first + 6 > vertices.len) return false;
    graphics_vertex v[6];
    GRAPHICS_quad(v, dst, uv, col);
    return SDL_memcmp(dynarr_at(first, &vertices), v, sizeof(v)) == 0;
}

// Submits `copy_cmd`, waits for it and copies the `size` bytes on `transfer`
// into `out`, releasing it.
static void finish_download(SDL_GPUCommandBuffer* copy_cmd,
                            SDL_GPUTransferBuffer* transfer,
                            void* out,
                            Uint32 size) {
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(copy_cmd);
    SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    SDL_memcpy(out, SDL_MapGPUTransferBuffer(device, transfer, false), size);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}

static SDL_GPUTransferBuffer* create_download(Uint32 size) {
    return SDL_CreateGPUTransferBuffer(
        device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .size = size,
        });
}

// Reads the RGBA pixels of `t`, a `TARGET_SIZE` square, into `out`, waiting
// for every frame drawing on it to finish.
static void read_pixels(graphics_texture* t,
                        Uint8 out[TARGET_SIZE * TARGET_SIZE * 4]) {
    Uint32 size = TARGET_SIZE * TARGET_SIZE * 4;
    SDL_GPUTransferBuffer* transfer = create_download(size);
    SDL_GPUCommandBuffer* copy_cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(copy_cmd);
    SDL_DownloadFromGPUTexture(copy,
                               &(SDL_GPUTextureRegion){
                                   .texture = t->texture,
                                   .w = TARGET_SIZE,
                                   .h = TARGET_SIZE,
                                   .d = 1,
                               },
                               &(SDL_GPUTextureTransferInfo){
                                   .transfer_buffer = transfer,
                               });
    SDL_EndGPUCopyPass(copy);
    finish_download(copy_cmd, transfer, out, size);
}

// Reads the first `size` bytes of `buffer` into `out`.
static void read_buffer(SDL_GPUBuffer* buffer, void* out, Uint32 size) {
    SDL_GPUTransferBuffer* transfer = create_download(size);
    SDL_GPUCommandBuffer* copy_cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(copy_cmd);
    SDL_DownloadFromGPUBuffer(copy,
                              &(SDL_GPUBufferRegion){
                                  .buffer = buffer,
                                  .size = size,
                              },
                              &(SDL_GPUTransferBufferLocation){
                                  .transfer_buffer = transfer,
                              });
    SDL_EndGPUCopyPass(copy);
    finish_download(copy_cmd, transfer, out, size);
}

// Whether every pixel of `r` on `px`, read with `read_pixels()`, is `col`.
static bool rect_is(const Uint8* px, SDL_Rect r, SDL_Color col) {
    Uint8 c[4] = {col.r, col.g, col.b, col.a};
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            for (int i = 0; i < 4; i++)
                if (SDL_abs(px[(y * TARGET_SIZE + x) * 4 + i] - c[i]) >
                    PIXEL_TOLERANCE)
                    return false;
    return true;
}

// A `w,1` texture of `cols`, left to right.
static graphics_texture*
create_strip(const char* name, int w, const SDL_Color* cols) {
    SDL_Surface* s = SDL_CreateSurface(w, 1, SDL_PIXELFORMAT_RGBA32);
    for (int x = 0; x < w; x++)
        SDL_WriteSurfacePixel(s, x, 0, cols[x].r, cols[x].g, cols[x].b, 255);
    graphics_texture* t =
        GRAPHICS_create_texture_from_surface(name, s, SDL_GPU_FILTER_NEAREST);
    SDL_DestroySurface(s);
    return t;
}

static graphics_texture* create_target(const char* name) {
    return GRAPHICS_create_texture(name,
                                   SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                   TARGET_SIZE,
                                   TARGET_SIZE,
                                   SDL_GPU_FILTER_NEAREST);
}



static void test_vertices(graphics_texture* t) {
    SDL_FColor r = GRAPHICS_fcolor(red), g = GRAPHICS_fcolor(green);
    SDL_FRect uv = {0.0f, 0.0f, 1.0f, 1.0f};

    GRAPHICS_begin_frame();
    GRAPHICS_set_target(t);
    GRAPHICS_clear(GRAPHICS_fcolor(black));
    GRAPHICS_fill_rect((SDL_FRect){0, 0, 4, 8}, r);
    GRAPHICS_fill_rect((SDL_FRect){4, 0, 4, 8}, g);
    GRAPHICS_draw_point(7, 7, r);
    GRAPHICS_draw_vertices(white, NULL, 0);

    command* pass = recorded(0);
    command* draw = recorded(1);
    expect("Filling rects and a point records a cleared pass and a single draw "
           "of all their vertices with the white texture...",
           6,
           (bool[]){
               commands.len == 2,
               is_pass(pass, t, true),
               fcolor_eq(pass->pass.clear_col, GRAPHICS_fcolor(black)),
               is_vertices_draw(draw, white, 0, 18),
               is_entry(entry_of(draw->draw.pipeline),
                        base_vert,
                        base_frag,
                        graphics_blend_none,
                        true),
               vertices.len == 18,
           });
    expect("...with the vertices of their quads, in order...",
           3,
           (bool[]){
               is_quad(0, (SDL_FRect){0, 0, 4, 8}, uv, r),
               is_quad(6, (SDL_FRect){4, 0, 4, 8}, uv, g),
               is_quad(12, (SDL_FRect){7, 7, 1, 1}, uv, r),
           });

    GRAPHICS_end_frame();
    Uint8 px[TARGET_SIZE * TARGET_SIZE * 4];
    read_pixels(t, px);
    expect("...and draws them...",
           3,
           (bool[]){
               rect_is(px, (SDL_Rect){0, 0, 4, 8}, red),
               rect_is(px, (SDL_Rect){4, 0, 4, 7}, green),
               rect_is(px, (SDL_Rect){7, 7, 1, 1}, red),
           });
}

static void test_textures(graphics_texture* t) {
    graphics_texture* strip =
        create_strip("strip", 2, (SDL_Color[]){red, blue});
    SDL_FColor w = GRAPHICS_fcolor(white_col), g = GRAPHICS_fcolor(green);

    GRAPHICS_begin_frame();
    GRAPHICS_set_target(t);
    GRAPHICS_clear(GRAPHICS_fcolor(black));
    GRAPHICS_draw_texture(
        strip, NULL, &(SDL_FRect){0, 0, 8, 4}, SDL_FLIP_NONE, w);
    GRAPHICS_draw_texture(
        strip, NULL, &(SDL_FRect){0, 4, 8, 4}, SDL_FLIP_HORIZONTAL, w);
    GRAPHICS_fill_rect((SDL_FRect){6, 6, 2, 2}, g);
    GRAPHICS_draw_texture(strip,
                          &(SDL_FRect){1, 0, 1, 1},
                          &(SDL_FRect){0, 0, 1, 1},
                          SDL_FLIP_NONE,
                          w);

    command* a = recorded(1);
    command* b = recorded(2);
    command* c = recorded(3);
    expect("Drawing a texture twice, a rect and the texture again records a "
           "draw for each change of texture...",
           5,
           (bool[]){
               commands.len == 4,
               is_vertices_draw(a, strip, 0, 12),
               is_vertices_draw(b, white, 12, 6),
               is_vertices_draw(c, strip, 18, 6),
               a->draw.pipeline == b->draw.pipeline &&
                   b->draw.pipeline == c->draw.pipeline,
           });
    expect("...with the texture's UVs, flipped and of its source rects...",
           3,
           (bool[]){
               is_quad(0, (SDL_FRect){0, 0, 8, 4}, (SDL_FRect){0, 0, 1, 1}, w),
               is_quad(6, (SDL_FRect){0, 4, 8, 4}, (SDL_FRect){1, 0, -1, 1}, w),
               is_quad(
                   18, (SDL_FRect){0, 0, 1, 1}, (SDL_FRect){0.5, 0, 0.5, 1}, w),
           });

    GRAPHICS_end_frame();
    Uint8 px[TARGET_SIZE * TARGET_SIZE * 4];
    read_pixels(t, px);
    expect("...and draws them in order...",
           7,
           (bool[]){
               rect_is(px, (SDL_Rect){0, 0, 1, 1}, blue),
               rect_is(px, (SDL_Rect){1, 0, 3, 4}, red),
               rect_is(px, (SDL_Rect){0, 1, 1, 3}, red),
               rect_is(px, (SDL_Rect){4, 0, 4, 4}, blue),
               rect_is(px, (SDL_Rect){0, 4, 4, 4}, blue),
               rect_is(px, (SDL_Rect){4, 4, 2, 4}, red),
               rect_is(px, (SDL_Rect){6, 6, 2, 2}, green),
           });

    GRAPHICS_destroy_texture(strip);
}

static void test_targets(graphics_texture* t, graphics_texture* other) {
    GRAPHICS_begin_frame();
    GRAPHICS_set_target(other);
    GRAPHICS_clear(GRAPHICS_fcolor(red));
    GRAPHICS_clear(GRAPHICS_fcolor(black));
    GRAPHICS_fill_rect((SDL_FRect){0, 0, 4, 8}, GRAPHICS_fcolor(red));
    GRAPHICS_clear(GRAPHICS_fcolor(blue));
    GRAPHICS_fill_rect((SDL_FRect){0, 0, 2, 8}, GRAPHICS_fcolor(green));
    GRAPHICS_set_target(t);
    GRAPHICS_clear(GRAPHICS_fcolor(black));
    GRAPHICS_draw_texture(
        other, NULL, NULL, SDL_FLIP_NONE, GRAPHICS_fcolor(white_col));

    expect("Clearing a target twice records a single pass, clearing it after "
           "drawing starts another and drawing a target on another records "
           "a draw with it...",
           9,
           (bool[]){
               commands.len == 6,
               is_pass(recorded(0), other, true),
               fcolor_eq(recorded(0)->pass.clear_col, GRAPHICS_fcolor(black)),
               is_vertices_draw(recorded(1), white, 0, 6),
               is_pass(recorded(2), other, true),
               fcolor_eq(recorded(2)->pass.clear_col, GRAPHICS_fcolor(blue)),
               is_vertices_draw(recorded(3), white, 6, 6),
               is_pass(recorded(4), t, true),
               is_vertices_draw(recorded(5), other, 12, 6),
           });

    GRAPHICS_end_frame();
    Uint8 px[TARGET_SIZE * TARGET_SIZE * 4];
    read_pixels(t, px);
    expect("...and draws what was drawn after the last clear, covering the "
           "whole target...",
           2,
           (bool[]){
               rect_is(px, (SDL_Rect){0, 0, 2, 8}, green),
               rect_is(px, (SDL_Rect){2, 0, 6, 8}, blue),
           });
}

static void test_state(graphics_texture* t) {
    graphics_texture* mask =
        create_strip("mask", 2, (SDL_Color[]){white_col, black});
    SDL_GPUShader* mask_frag = GRAPHICS_load_fragment_shader(
        "mask", GRAPHICS_SHADER_CODE(mask_frag_spirv, mask_frag_dxil), 2, 1);
    SDL_FColor tint = GRAPHICS_fcolor(green);
    SDL_FColor half_red = {1.0f, 0.0f, 0.0f, 0.5f};

    GRAPHICS_begin_frame();
    GRAPHICS_set_target(t);
    GRAPHICS_clear(GRAPHICS_fcolor(blue));
    GRAPHICS_set_state(&(graphics_state){
        .fragment = mask_frag,
        .samplers = {GRAPHICS_binding(mask)},
        .num_samplers = 1,
        .fragment_uniforms = &tint,
        .fragment_uniforms_size = sizeof(tint),
    });
    tint = GRAPHICS_fcolor(red);
    GRAPHICS_fill_rect((SDL_FRect){0, 0, 8, 4}, GRAPHICS_fcolor(white_col));
    GRAPHICS_set_state(&(graphics_state){.blend = graphics_blend_blend});
    GRAPHICS_fill_rect((SDL_FRect){0, 4, 8, 2}, half_red);
    GRAPHICS_set_state(&(graphics_state){.blend = graphics_blend_add});
    GRAPHICS_fill_rect((SDL_FRect){0, 6, 8, 2}, half_red);
    GRAPHICS_set_state(&(graphics_state){.blend = graphics_blend_add});
    GRAPHICS_fill_rect((SDL_FRect){0, 6, 8, 2}, (SDL_FColor){0, 0, 0, 0});

    command* masked = recorded(1);
    SDL_FColor green_f = GRAPHICS_fcolor(green);
    expect("Drawing with a fragment shader, an extra sampler and uniforms "
           "records them, the uniforms as they were when they were set...",
           7,
           (bool[]){
               masked->type == command_draw,
               masked->draw.num_samplers == 2,
               masked->draw.samplers[0].texture == white->texture,
               masked->draw.samplers[1].texture == mask->texture,
               masked->draw.fragment_uniforms_size == sizeof(SDL_FColor),
               SDL_memcmp(masked->draw.fragment_uniforms,
                          &green_f,
                          sizeof(green_f)) == 0,
               is_entry(entry_of(masked->draw.pipeline),
                        base_vert,
                        mask_frag,
                        graphics_blend_none,
                        true),
           });
    expect("...and every state set starts a draw with its own pipeline, "
           "reusing those of the same state...",
           6,
           (bool[]){
               commands.len == 5,
               is_entry(entry_of(recorded(2)->draw.pipeline),
                        base_vert,
                        base_frag,
                        graphics_blend_blend,
                        true),
               is_entry(entry_of(recorded(3)->draw.pipeline),
                        base_vert,
                        base_frag,
                        graphics_blend_add,
                        true),
               recorded(4)->draw.first_vertex == 18,
               recorded(4)->draw.pipeline == recorded(3)->draw.pipeline,
               recorded(2)->draw.pipeline != recorded(3)->draw.pipeline,
           });

    GRAPHICS_end_frame();
    Uint8 px[TARGET_SIZE * TARGET_SIZE * 4];
    read_pixels(t, px);
    expect(
        "...drawing the texture, the extra sampler and the uniforms "
        "multiplied, then blending and adding half transparent red...",
        4,
        (bool[]){
            rect_is(px, (SDL_Rect){0, 0, 4, 4}, green),
            rect_is(px, (SDL_Rect){4, 0, 4, 4}, black),
            rect_is(px, (SDL_Rect){0, 4, 8, 2}, (SDL_Color){128, 0, 128, 255}),
            rect_is(px, (SDL_Rect){0, 6, 8, 2}, (SDL_Color){128, 0, 255, 255}),
        });

    GRAPHICS_release_shader(mask_frag);
    GRAPHICS_destroy_texture(mask);
}

static void test_instances(graphics_texture* t, SDL_GPUShader* rects_vert) {
    SDL_GPUBuffer* rects =
        GRAPHICS_create_buffer("rects",
                               SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                               3 * sizeof(float[4]));
    float data[2][4] = {{0, 0, 2, 2}, {4, 4, 4, 4}};
    SDL_FColor col = GRAPHICS_fcolor(green);

    GRAPHICS_begin_frame();
    GRAPHICS_set_target(t);
    GRAPHICS_clear(GRAPHICS_fcolor(black));
    GRAPHICS_upload_buffer(rects, sizeof(float[4]), data, sizeof(data));
    expect("Uploading to a buffer during a frame queues it...",
           3,
           (bool[]){
               uploads.len == 1,
               staging.len == sizeof(data),
               ((upload*)dynarr_at(0, &uploads))->offset == sizeof(float[4]),
           });

    GRAPHICS_draw_instances(white, &(graphics_instances){6, 2});
    graphics_state s = {
        .vertex = rects_vert,
        .storage_buffers = {rects},
        .num_storage_buffers = 1,
        .vertex_uniforms = &col,
        .vertex_uniforms_size = sizeof(col),
    };
    GRAPHICS_set_state(&s);
    col = GRAPHICS_fcolor(red);
    GRAPHICS_draw_instances(white, &(graphics_instances){0, 2});
    GRAPHICS_draw_instances(white, &(graphics_instances){6, 0});
    expect("...drawing instances without a vertex shader or with nothing to "
           "draw records nothing...",
           1,
           (bool[]){commands.len == 1});

    GRAPHICS_draw_instances(white, &(graphics_instances){6, 2});
    GRAPHICS_draw_instances(white, &(graphics_instances){6, 3});
    GRAPHICS_set_state(NULL);
    GRAPHICS_fill_rect((SDL_FRect){0, 7, 8, 1}, GRAPHICS_fcolor(blue));

    command* a = recorded(1);
    command* b = recorded(2);
    SDL_FColor green_f = GRAPHICS_fcolor(green);
    expect(
        "...drawing them records a draw of its own for each, with their "
        "storage buffers and uniforms and without the vertex buffer...",
        12,
        (bool[]){
            commands.len == 4,
            a->type == command_draw,
            a->draw.num_vertices == 6 && a->draw.num_instances == 2,
            a->draw.first_vertex == 0 && !a->draw.indirect_buffer,
            a->draw.num_storage_buffers == 1,
            a->draw.storage_buffers[0] == rects,
            a->draw.vertex_uniforms_size == sizeof(SDL_FColor),
            SDL_memcmp(a->draw.vertex_uniforms, &green_f, sizeof(green_f)) == 0,
            is_entry(entry_of(a->draw.pipeline),
                     rects_vert,
                     base_frag,
                     graphics_blend_none,
                     false),
            b->type == command_draw && b->draw.num_instances == 3,
            is_vertices_draw(recorded(3), white, 0, 6),
            vertices.len == 6,
        });

    GRAPHICS_end_frame();
    float out[3][4] = {0};
    read_buffer(rects, out, sizeof(out));
    expect("...uploads what was queued at its offset, leaving the rest...",
           3,
           (bool[]){
               out[0][0] == 0 && out[0][1] == 0 && out[0][2] == 0 &&
                   out[0][3] == 0,
               SDL_memcmp(out[1], data[0], sizeof(data[0])) == 0,
               SDL_memcmp(out[2], data[1], sizeof(data[1])) == 0,
           });

    Uint8 px[TARGET_SIZE * TARGET_SIZE * 4];
    read_pixels(t, px);
    expect("...and draws them after uploading it...",
           6,
           (bool[]){
               rect_is(px, (SDL_Rect){0, 0, 2, 2}, green),
               rect_is(px, (SDL_Rect){2, 0, 6, 4}, black),
               rect_is(px, (SDL_Rect){0, 2, 4, 2}, black),
               rect_is(px, (SDL_Rect){4, 4, 4, 3}, green),
               rect_is(px, (SDL_Rect){0, 4, 4, 3}, black),
               rect_is(px, (SDL_Rect){0, 7, 8, 1}, blue),
           });

    GRAPHICS_destroy_buffer(rects);
}

static void test_dispatch(graphics_texture* t, SDL_GPUShader* rects_vert) {
    SDL_GPUComputePipeline* comp = GRAPHICS_load_compute_pipeline(
        "rects",
        GRAPHICS_SHADER_CODE(rects_comp_spirv, rects_comp_dxil),
        1,
        2,
        1,
        64);
    SDL_GPUBuffer* src = GRAPHICS_create_buffer(
        "src", SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, 3 * sizeof(float[4]));
    SDL_GPUBuffer* rects =
        GRAPHICS_create_buffer("rects",
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                                   SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                               3 * sizeof(float[4]));
    SDL_GPUBuffer* args =
        GRAPHICS_create_buffer("args",
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                                   SDL_GPU_BUFFERUSAGE_INDIRECT,
                               sizeof(SDL_GPUIndirectDrawCommand));
    float src_data[3][4] = {{0, 0, 1, 1}, {2, 0, 1, 1}, {4, 0, 1, 1}};
    GRAPHICS_upload_buffer(src, 0, src_data, sizeof(src_data));
    rects_comp_uniforms u = {{1, 2}, 2, 0};
    rects_comp_uniforms u_copy = u;
    SDL_FColor col = GRAPHICS_fcolor(green);

    GRAPHICS_begin_frame();
    GRAPHICS_set_target(t);
    GRAPHICS_clear(GRAPHICS_fcolor(black));
    GRAPHICS_fill_rect((SDL_FRect){0, 7, 8, 1}, GRAPHICS_fcolor(red));
    GRAPHICS_dispatch(&(graphics_dispatch){
        .pipeline = comp,
        .readonly_buffers = {src},
        .num_readonly_buffers = 1,
        .readwrite_buffers = {rects, args},
        .num_readwrite_buffers = 2,
        .uniforms = &u,
        .uniforms_size = sizeof(u),
        .groups = 1,
    });
    u.count = 0;
    GRAPHICS_set_state(&(graphics_state){
        .vertex = rects_vert,
        .storage_buffers = {rects},
        .num_storage_buffers = 1,
        .vertex_uniforms = &col,
        .vertex_uniforms_size = sizeof(col),
    });
    GRAPHICS_draw_instances(white,
                            &(graphics_instances){.indirect_buffer = args});

    const graphics_dispatch* d = &recorded(2)->dispatch.dispatch;
    command* indirect = recorded(3);
    expect(
        "Dispatching between draws records it in order with them, with its "
        "buffers and its uniforms as they were when dispatched...",
        11,
        (bool[]){
            commands.len == 4,
            is_vertices_draw(recorded(1), white, 0, 6),
            recorded(2)->type == command_dispatch,
            d->pipeline == comp && d->groups == 1,
            d->num_readonly_buffers == 1 && d->readonly_buffers[0] == src,
            d->num_readwrite_buffers == 2 && d->readwrite_buffers[0] == rects &&
                d->readwrite_buffers[1] == args,
            d->uniforms == NULL && d->uniforms_size == sizeof(u),
            SDL_memcmp(
                recorded(2)->dispatch.uniforms, &u_copy, sizeof(u_copy)) == 0,
            indirect->type == command_draw,
            indirect->draw.indirect_buffer == args,
            indirect->draw.indirect_offset == 0,
        });

    GRAPHICS_end_frame();
    float out[3][4] = {0};
    SDL_GPUIndirectDrawCommand out_args = {0};
    read_buffer(rects, out, sizeof(out));
    read_buffer(args, &out_args, sizeof(out_args));
    expect("...runs it on the buffers uploaded outside of the frame...",
           6,
           (bool[]){
               out[0][0] == 1 && out[0][1] == 2 && out[0][2] == 1 &&
                   out[0][3] == 1,
               out[1][0] == 3 && out[1][1] == 2 && out[1][2] == 1 &&
                   out[1][3] == 1,
               out[2][0] == 0 && out[2][1] == 0 && out[2][2] == 0 &&
                   out[2][3] == 0,
               out_args.num_vertices == 6,
               out_args.num_instances == 2,
               out_args.first_vertex == 0 && out_args.first_instance == 0,
           });

    Uint8 px[TARGET_SIZE * TARGET_SIZE * 4];
    read_pixels(t, px);
    expect("...and the draws after it see what it wrote, on the same target, "
           "keeping what was drawn before it...",
           8,
           (bool[]){
               rect_is(px, (SDL_Rect){0, 2, 1, 1}, black),
               rect_is(px, (SDL_Rect){2, 2, 1, 1}, black),
               rect_is(px, (SDL_Rect){1, 2, 1, 1}, green),
               rect_is(px, (SDL_Rect){3, 2, 1, 1}, green),
               rect_is(px, (SDL_Rect){0, 0, 8, 2}, black),
               rect_is(px, (SDL_Rect){4, 2, 4, 1}, black),
               rect_is(px, (SDL_Rect){0, 3, 8, 4}, black),
               rect_is(px, (SDL_Rect){0, 7, 8, 1}, red),
           });

    GRAPHICS_destroy_buffer(args);
    GRAPHICS_destroy_buffer(rects);
    GRAPHICS_destroy_buffer(src);
    GRAPHICS_release_compute_pipeline(comp);
}



void test_graphics(void) {
    printf(YELLOW "\nTesting graphics...\n" RESET);

    // The dummy video driver can't make GPU devices, the offscreen one can.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    bool started = GRAPHICS_start() == 0;
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
    expect("Starting graphics with no window on the offscreen video driver...",
           1,
           (bool[]){started});
    if (!started) {
        GRAPHICS_stop();
        return;
    }

    graphics_texture* t = create_target("target");
    graphics_texture* other = create_target("other");
    SDL_GPUShader* rects_vert = GRAPHICS_load_vertex_shader(
        "rects", GRAPHICS_SHADER_CODE(rects_vert_spirv, rects_vert_dxil), 1, 2);

    test_vertices(t);
    test_textures(t);
    test_targets(t, other);
    test_state(t);
    test_instances(t, rects_vert);
    test_dispatch(t, rects_vert);

    GRAPHICS_release_shader(rects_vert);
    GRAPHICS_destroy_texture(other);
    GRAPHICS_destroy_texture(t);
    GRAPHICS_stop();
}
