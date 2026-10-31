#pragma once
#include "defs.h"
#include <SDL3/SDL.h>

// How what's drawn is blended with what's already on the target, the same as
// SDL's blend modes of the same names.
//
// ```
// enum graphics_blend {
//     graphics_blend_none,  // dstRGBA = srcRGBA
//     graphics_blend_blend, // dstRGB = srcRGB * srcA + dstRGB * (1 - srcA)
//                           // dstA = srcA + dstA * (1 - srcA)
//     graphics_blend_add,   // dstRGB = srcRGB * srcA + dstRGB, dstA = dstA
// };
// ```
typedef enum {
    graphics_blend_none,
    graphics_blend_blend,
    graphics_blend_add,
} graphics_blend;

// A texture that can be sampled and drawn on, sampled with `sampler`.
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

// A vertex of the base vertex shader, `src/gpu/base.vert.hlsl`. `x,y` is in
// pixels on the target from its top left, `u,v` is from `0.0` to `1.0` on the
// texture being drawn and `col` multiplies its color.
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

// How draws are made. `shader` is a fragment shader from
// `GRAPHICS_load_fragment_shader()`, `NULL` for the base one, which draws the
// texture multiplied by the vertex color. The texture being drawn is always
// bound to the first sampler slot, and `samplers` to the slots after it. The
// `uniforms_size` bytes at `uniforms` are its first uniform buffer, copied
// when the state is set.
//
// ```
// struct graphics_state {
//     SDL_GPUShader* shader;
//     graphics_blend blend;
//     SDL_GPUTextureSamplerBinding samplers[GRAPHICS_MAX_EXTRA_SAMPLERS];
//     Uint32 num_samplers;
//     const void* uniforms;
//     Uint32 uniforms_size;
// };
// ```
typedef struct {
    SDL_GPUShader* shader;
    graphics_blend blend;
    SDL_GPUTextureSamplerBinding samplers[GRAPHICS_MAX_EXTRA_SAMPLERS];
    Uint32 num_samplers;
    const void* uniforms;
    Uint32 uniforms_size;
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
// supports, with `num_samplers` samplers and `num_uniform_buffers` uniform
// buffers. `name` is for logging. Returns `NULL` on errors.
SDL_GPUShader* GRAPHICS_load_fragment_shader(const char* name,
                                             graphics_shader_code code,
                                             Uint32 num_samplers,
                                             Uint32 num_uniform_buffers);

// Releases a shader from `GRAPHICS_load_fragment_shader()`. Accepts `NULL`.
void GRAPHICS_release_shader(SDL_GPUShader* shader);



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

// Draws the `n` vertices at `vertices` with `texture`, every 3 of them a
// triangle.
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

// Writes on `v` the two triangles covering `dst` with `col`, mapping the top
// left of `uv` to the top left of `dst` and its bottom right to the bottom
// right of `dst`, so a negative `uv.w` or `uv.h` flips it. `v[0]` is the top
// left and `v[4]` the bottom right.
static inline void GRAPHICS_quad(graphics_vertex v[6],
                                 SDL_FRect dst,
                                 SDL_FRect uv,
                                 SDL_FColor col) {
    float x0 = dst.x, y0 = dst.y, x1 = dst.x + dst.w, y1 = dst.y + dst.h;
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.w, v1 = uv.y + uv.h;
    v[0] = (graphics_vertex){x0, y0, u0, v0, col};
    v[1] = (graphics_vertex){x1, y0, u1, v0, col};
    v[2] = (graphics_vertex){x0, y1, u0, v1, col};
    v[3] = (graphics_vertex){x1, y0, u1, v0, col};
    v[4] = (graphics_vertex){x1, y1, u1, v1, col};
    v[5] = (graphics_vertex){x0, y1, u0, v1, col};
}

// Converts `col` from `[0, 255]` to `[0.0, 1.0]`.
static inline SDL_FColor GRAPHICS_fcolor(SDL_Color col) {
    return (SDL_FColor){
        col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f};
}
