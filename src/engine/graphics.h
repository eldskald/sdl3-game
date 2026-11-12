#pragma once
#include "defs.h"
#include <SDL3/SDL.h>
#include <stddef.h>

// Blend modes.
//
// ```
// enum graphics_blend {
//     graphics_blend_none,
//     graphics_blend_blend,
//     graphics_blend_add,
// };
// ```
typedef enum {
    graphics_blend_none,
    graphics_blend_blend,
    graphics_blend_add,
} graphics_blend;

// A texture that can be sampled and drawn on.
//
// ```
// struct graphics_texture {
//     SDL_GPUTexture* texture;
//     SDL_GPUSampler* sampler;
//     SDL_GPUTextureFormat format;
//     Uint32 w, h;
// };
// ```
typedef struct {
    SDL_GPUTexture* texture;
    SDL_GPUSampler* sampler;
    SDL_GPUTextureFormat format;
    Uint32 w, h;
} graphics_texture;

// Vertex data, meant for the base vertex shader that passes on only position,
// UVs and color to the fragment.
//
// ```
// struct graphics_vertex {
//     float x, y;
//     float u, v;
//     SDL_FColor col;
// };
// ```
typedef struct {
    float x, y;
    float u, v;
    SDL_FColor col;
} graphics_vertex;

// How draws are made, both of vertices and of instances.
//
// `vertex` is a vertex shader from `GRAPHICS_load_vertex_shader()`, `NULL` for
// the base one, `src/gpu/base.vert.hlsl`. Draws of vertices feed it them as
// `graphics_vertex`, so it must take them like the base one does, while draws
// of instances bind no vertex buffer, so it builds its vertices itself from
// `SV_VertexID`, `SV_InstanceID` and whatever it reads from
// `vertex_storage_buffers`, bound to its `t` registers in order. Its first
// uniform buffer is always the target's size, and the `vertex_uniforms_size`
// bytes at `vertex_uniforms` are its second.
//
// `fragment` is a fragment shader from `GRAPHICS_load_fragment_shader()`,
// `NULL` for the base one, which draws the texture multiplied by the vertex
// color. The texture being drawn is always bound to its first sampler slot,
// and `samplers` to the slots after it. It reads `fragment_storage_buffers`,
// bound to its `t` registers in order after the samplers', see
// `SDL_CreateGPUShader()`. The `fragment_uniforms_size` bytes at
// `fragment_uniforms` are its first uniform buffer.
//
// Uniforms are copied when the state is set.
//
// ```
// struct graphics_state {
//     SDL_GPUShader* vertex;
//     SDL_GPUBuffer* vertex_storage_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
//     Uint32 num_vertex_storage_buffers;
//     const void* vertex_uniforms;
//     Uint32 vertex_uniforms_size;
//     SDL_GPUShader* fragment;
//     graphics_blend blend;
//     SDL_GPUTextureSamplerBinding samplers[GRAPHICS_MAX_EXTRA_SAMPLERS];
//     Uint32 num_samplers;
//     SDL_GPUBuffer* fragment_storage_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
//     Uint32 num_fragment_storage_buffers;
//     const void* fragment_uniforms;
//     Uint32 fragment_uniforms_size;
// };
// ```
typedef struct {
    SDL_GPUShader* vertex;
    SDL_GPUBuffer* vertex_storage_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
    Uint32 num_vertex_storage_buffers;
    const void* vertex_uniforms;
    Uint32 vertex_uniforms_size;
    SDL_GPUShader* fragment;
    graphics_blend blend;
    SDL_GPUTextureSamplerBinding samplers[GRAPHICS_MAX_EXTRA_SAMPLERS];
    Uint32 num_samplers;
    SDL_GPUBuffer* fragment_storage_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
    Uint32 num_fragment_storage_buffers;
    const void* fragment_uniforms;
    Uint32 fragment_uniforms_size;
} graphics_state;

// A shader's code in every format a device might take. Make it with
// `GRAPHICS_SHADER_CODE()` from the arrays the code is embedded on.
//
// ```
// struct graphics_shader_code {
//     const Uint8* spirv;
//     size_t spirv_size;
//     const Uint8* dxil;
//     size_t dxil_size;
// };
// ```
typedef struct {
    const Uint8* spirv;
    size_t spirv_size;
    const Uint8* dxil;
    size_t dxil_size;
} graphics_shader_code;

// The `graphics_shader_code` of the arrays `spirv` and `dxil`, which must be
// arrays and not pointers, so their sizes are known.
#define GRAPHICS_SHADER_CODE(spirv, dxil)                                      \
    ((graphics_shader_code){spirv, sizeof(spirv), dxil, sizeof(dxil)})

// A compute shader run on `groups` groups of threads, along x only. It reads
// the `readonly_buffers`, bound to its `t` registers in order, and reads and
// writes the `readwrite_buffers`, bound to its `u` registers in order. The
// `uniforms_size` bytes at `uniforms` are its first uniform buffer, copied when
// it's dispatched.
//
// ```
// struct graphics_dispatch {
//     SDL_GPUComputePipeline* pipeline;
//     SDL_GPUBuffer* readonly_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
//     Uint32 num_readonly_buffers;
//     SDL_GPUBuffer* readwrite_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
//     Uint32 num_readwrite_buffers;
//     const void* uniforms;
//     Uint32 uniforms_size;
//     Uint32 groups;
// };
// ```
typedef struct {
    SDL_GPUComputePipeline* pipeline;
    SDL_GPUBuffer* readonly_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
    Uint32 num_readonly_buffers;
    SDL_GPUBuffer* readwrite_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
    Uint32 num_readwrite_buffers;
    const void* uniforms;
    Uint32 uniforms_size;
    Uint32 groups;
} graphics_dispatch;

// How many instances a draw of instances draws: `num_instances` of
// `num_vertices` vertices each, every 3 of them a triangle. When
// `indirect_buffer` is set, they're instead the `SDL_GPUIndirectDrawCommand`
// at `indirect_offset` bytes on it, read when it's drawn, so the GPU can write
// them, and `num_vertices` and `num_instances` aren't used.
//
// ```
// struct graphics_instances {
//     Uint32 num_vertices;
//     Uint32 num_instances;
//     SDL_GPUBuffer* indirect_buffer;
//     Uint32 indirect_offset;
// };
// ```
typedef struct {
    Uint32 num_vertices;
    Uint32 num_instances;
    SDL_GPUBuffer* indirect_buffer;
    Uint32 indirect_offset;
} graphics_instances;



// Creates the window, the GPU device claiming it and loads the base shaders.
// Returns 0 when successful, 1 otherwise.
int GRAPHICS_start(void);

// Releases everything `GRAPHICS_start()` created. Release everything created
// with the functions below before calling it.
void GRAPHICS_stop(void);



// Creates a texture of `w,h` pixels in `format` that can be sampled with
// `filter` and drawn on, cleared to transparent black. `name` is for logging.
// Returns `NULL` on errors.
graphics_texture* GRAPHICS_create_texture(const char* name,
                                          SDL_GPUTextureFormat format,
                                          Uint32 w,
                                          Uint32 h,
                                          SDL_GPUFilter filter);

// Creates a texture from `surface`, sampled with `filter`. It can't be drawn
// on. `name` is for logging. Returns `NULL` on errors.
graphics_texture* GRAPHICS_create_texture_from_surface(const char* name,
                                                       SDL_Surface* surface,
                                                       SDL_GPUFilter filter);

// Binding of `texture` with its sampler, for `graphics_state.samplers`.
SDL_GPUTextureSamplerBinding GRAPHICS_binding(graphics_texture* texture);

// Destroys a texture created with `GRAPHICS_create_texture...()`. Accepts
// `NULL`.
void GRAPHICS_destroy_texture(graphics_texture* texture);

// Uploads `surface` into `texture`, which must be the same size and be
// `SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM`. It's uploaded right away, before the
// current frame's draws run.
void GRAPHICS_upload_surface(graphics_texture* texture, SDL_Surface* surface);

// Creates a fragment shader from `code`, in whichever format the device
// supports, with `num_samplers` samplers, `num_storage_buffers` storage buffers
// and `num_uniform_buffers` uniform buffers. `name` is for logging. Returns
// `NULL` on errors.
SDL_GPUShader* GRAPHICS_load_fragment_shader(const char* name,
                                             graphics_shader_code code,
                                             Uint32 num_samplers,
                                             Uint32 num_storage_buffers,
                                             Uint32 num_uniform_buffers);

// Creates a vertex shader from `code`, for `graphics_state.vertex`, with
// `num_storage_buffers` storage buffers and `num_uniform_buffers` uniform
// buffers, the first of them always being the target's size. `name` is for
// logging. Returns `NULL` on errors.
SDL_GPUShader* GRAPHICS_load_vertex_shader(const char* name,
                                           graphics_shader_code code,
                                           Uint32 num_storage_buffers,
                                           Uint32 num_uniform_buffers);

// Releases a shader from `GRAPHICS_load_..._shader()`. Accepts `NULL`.
void GRAPHICS_release_shader(SDL_GPUShader* shader);

// Creates a compute pipeline from `code`, with `num_readonly_buffers`
// read-only storage buffers, `num_readwrite_buffers` read-write storage
// buffers, `num_uniform_buffers` uniform buffers and `threads` threads per
// group along x, which must match its `[numthreads()]`. `name` is for logging.
// Returns `NULL` on errors.
SDL_GPUComputePipeline*
GRAPHICS_load_compute_pipeline(const char* name,
                               graphics_shader_code code,
                               Uint32 num_readonly_buffers,
                               Uint32 num_readwrite_buffers,
                               Uint32 num_uniform_buffers,
                               Uint32 threads);

// Releases a pipeline from `GRAPHICS_load_compute_pipeline()`. Accepts `NULL`.
void GRAPHICS_release_compute_pipeline(SDL_GPUComputePipeline* pipeline);



// Creates a GPU buffer of `size` bytes for `usage`, `SDL_GPU_BUFFERUSAGE_*`
// flags, filled with zeros. `name` is for logging. Returns `NULL` on errors.
SDL_GPUBuffer* GRAPHICS_create_buffer(const char* name,
                                      SDL_GPUBufferUsageFlags usage,
                                      Uint32 size);

// Releases a buffer from `GRAPHICS_create_buffer()`. Accepts `NULL`.
void GRAPHICS_destroy_buffer(SDL_GPUBuffer* buffer);

// Uploads the `size` bytes at `data` into `buffer`, `offset` bytes from its
// start, leaving the rest of it as it was. `data` is copied right away. During
// a frame, it's uploaded on `GRAPHICS_end_frame()` before any of the frame's
// dispatches and draws run, with every other upload of the frame, in the order
// they were made. Outside of one, it's uploaded right away.
void GRAPHICS_upload_buffer(SDL_GPUBuffer* buffer,
                            Uint32 offset,
                            const void* data,
                            Uint32 size);



// Draw calls aren't run right away. They are recorded on the CPU, batching
// consecutive ones drawing the same texture with the same state, and run in
// order on `GRAPHICS_end_frame()`. Call everything below between
// `GRAPHICS_begin_frame()` and `GRAPHICS_end_frame()`, on the main thread.

// Starts recording a frame and acquires the window's swapchain texture, which
// can be missing when the window is minimized. Draws on it are then skipped.
void GRAPHICS_begin_frame(void);

// Uploads the frame's vertices, runs its draws and presents it.
void GRAPHICS_end_frame(void);

// Gets the size of the swapchain texture of the frame. Returns false when there
// is none.
bool GRAPHICS_get_swapchain_size(Uint32* w, Uint32* h);

// Sets what's drawn on, `NULL` for the swapchain texture.
void GRAPHICS_set_target(graphics_texture* target);

// Fills the whole target with `col`, ignoring the state.
void GRAPHICS_clear(SDL_FColor col);

// Sets how the next draws are made, `NULL` for the base shader without
// blending. It's copied, uniforms included, so `state` can be changed right
// after.
void GRAPHICS_set_state(const graphics_state* state);

// Draws the `n` vertices at `vertices` with `texture` and the current state,
// every 3 of them a triangle.
void GRAPHICS_draw_vertices(graphics_texture* texture,
                            const graphics_vertex* vertices,
                            size_t n);

// Draws the `src` rect of `texture` on the `dst` rect of the target, flipped by
// `flip_mode` and multiplied by `col`. `NULL` rects are the whole texture and
// the whole target.
void GRAPHICS_draw_texture(graphics_texture* texture,
                           const SDL_FRect* src,
                           const SDL_FRect* dst,
                           SDL_FlipMode flip_mode,
                           SDL_FColor col);

// Fills `rect` with `col`.
void GRAPHICS_fill_rect(SDL_FRect rect, SDL_FColor col);

// Draws `instances` with `texture` and the current state, whose `vertex` must
// be set, since the base vertex shader only draws vertices. Does nothing
// otherwise.
void GRAPHICS_draw_instances(graphics_texture* texture,
                             const graphics_instances* instances);

// Dispatches `dispatch`. Dispatches are recorded with the draws and run in
// order with them, each finishing before what's after it starts, so it sees
// what the dispatches and draws before it wrote, and those after it see what it
// wrote. One between draws on the same target ends its render pass, and the
// draws after it continue on a new one.
void GRAPHICS_dispatch(const graphics_dispatch* dispatch);

// Draws the pixel at `(x,y)` with `col`.
void GRAPHICS_draw_point(float x, float y, SDL_FColor col);



// The helpers below build vertices for `GRAPHICS_draw_vertices()`. They're
// defined here so they're the same on tests, where the rest is stubbed.

// The `src` rect of `texture`, in pixels, in `u,v` coords for
// `GRAPHICS_quad()`, flipped by `flip_mode`.
static inline SDL_FRect GRAPHICS_uv(const graphics_texture* texture,
                                    SDL_FRect src,
                                    SDL_FlipMode flip_mode) {
    SDL_FRect uv = {src.x / texture->w,
                    src.y / texture->h,
                    src.w / texture->w,
                    src.h / texture->h};
    if (flip_mode & SDL_FLIP_HORIZONTAL) {
        uv.x += uv.w;
        uv.w = -uv.w;
    }
    if (flip_mode & SDL_FLIP_VERTICAL) {
        uv.y += uv.h;
        uv.h = -uv.h;
    }
    return uv;
}

// Writes on `v` the two triangles covering `dst` with `col`, its top left, top
// right, bottom right and bottom left corners in that order getting the `u,v`
// coords `uv[0]` to `uv[3]`. `v[0]` is the top left and `v[4]` the bottom
// right.
static inline void GRAPHICS_quad_corners(graphics_vertex v[6],
                                         SDL_FRect dst,
                                         const float uv[4][2],
                                         SDL_FColor col) {
    float x0 = dst.x, y0 = dst.y, x1 = dst.x + dst.w, y1 = dst.y + dst.h;
    v[0] = (graphics_vertex){x0, y0, uv[0][0], uv[0][1], col};
    v[1] = (graphics_vertex){x1, y0, uv[1][0], uv[1][1], col};
    v[2] = (graphics_vertex){x0, y1, uv[3][0], uv[3][1], col};
    v[3] = v[1];
    v[4] = (graphics_vertex){x1, y1, uv[2][0], uv[2][1], col};
    v[5] = v[2];
}

// Writes on `v` the two triangles covering `dst` with `col`, mapping the top
// left of `uv` to the top left of `dst` and its bottom right to the bottom
// right of `dst`, so a negative `uv.w` or `uv.h` flips it. `v[0]` is the top
// left and `v[4]` the bottom right.
static inline void GRAPHICS_quad(graphics_vertex v[6],
                                 SDL_FRect dst,
                                 SDL_FRect uv,
                                 SDL_FColor col) {
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.w, v1 = uv.y + uv.h;
    GRAPHICS_quad_corners(
        v,
        dst,
        (const float[4][2]){{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}},
        col);
}

// Same as `GRAPHICS_quad()`, but turning `uv` a quarter turn clockwise onto
// `dst`, so `dst` should have the width and height of `uv` swapped. The left
// of `uv` ends up on the top of `dst`, so a negative `uv.w` or `uv.h` flips it
// before it's turned.
static inline void GRAPHICS_quad_rotated(graphics_vertex v[6],
                                         SDL_FRect dst,
                                         SDL_FRect uv,
                                         SDL_FColor col) {
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.w, v1 = uv.y + uv.h;
    GRAPHICS_quad_corners(
        v,
        dst,
        (const float[4][2]){{u0, v1}, {u0, v0}, {u1, v0}, {u1, v1}},
        col);
}

// Converts `col` from `[0, 255]` to `[0.0, 1.0]`.
static inline SDL_FColor GRAPHICS_fcolor(SDL_Color col) {
    return (SDL_FColor){
        col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f};
}
