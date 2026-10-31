#include "stubbed/graphics.h"
#include "defs.h"
#include "engine/graphics.h"
#include "tests.h"
#include <SDL3/SDL.h>

// Shaders are opaque pointers that are never dereferenced, so they all point
// here.
static Uint8 dummy_shader = 0;

// Calls past the arrays' capacities aren't recorded.
static graphics_vertex vertices[SUB_CALLS_MAX * 6] = {0};
static size_t vertices_size = 0;
static graphics_texture* vertices_texture = NULL;

static GRAPHICS_draw_point_call draw_point_calls[SUB_CALLS_MAX] = {0};
static size_t draw_point_calls_size = 0;

static GRAPHICS_fill_rect_call fill_rect_calls[SUB_CALLS_MAX] = {0};
static size_t fill_rect_calls_size = 0;



int GRAPHICS_start(void) {
    return 0;
}

void GRAPHICS_stop(void) {
}

graphics_texture*
GRAPHICS_create_texture([[maybe_unused]] const char* name,
                        SDL_GPUTextureFormat format,
                        Uint32 w,
                        Uint32 h,
                        [[maybe_unused]] SDL_GPUFilter filter) {
    graphics_texture* t = SDL_malloc(sizeof(graphics_texture));
    *t = (graphics_texture){.format = format, .w = w, .h = h};
    return t;
}

graphics_texture* GRAPHICS_create_texture_from_surface(const char* name,
                                                       SDL_Surface* surface,
                                                       SDL_GPUFilter filter) {
    return GRAPHICS_create_texture(name,
                                   SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                   surface->w,
                                   surface->h,
                                   filter);
}

SDL_GPUTextureSamplerBinding
GRAPHICS_binding([[maybe_unused]] graphics_texture* texture) {
    return (SDL_GPUTextureSamplerBinding){0};
}

void GRAPHICS_destroy_texture(graphics_texture* texture) {
    SDL_free(texture);
}

void GRAPHICS_upload_surface(graphics_texture* texture, SDL_Surface* surface) {
}

SDL_GPUShader*
GRAPHICS_load_fragment_shader([[maybe_unused]] const char* name,
                              [[maybe_unused]] graphics_shader_code code,
                              [[maybe_unused]] Uint32 num_samplers,
                              [[maybe_unused]] Uint32 num_uniform_buffers) {
    return (SDL_GPUShader*)&dummy_shader;
}

void GRAPHICS_release_shader(SDL_GPUShader* shader) {
}



void GRAPHICS_begin_frame(void) {
}

void GRAPHICS_end_frame(void) {
}

bool GRAPHICS_get_swapchain_size(Uint32* w, Uint32* h) {
    *w = WINDOW_SIZE_X;
    *h = WINDOW_SIZE_Y;
    return true;
}

void GRAPHICS_set_target([[maybe_unused]] graphics_texture* t) {
}

void GRAPHICS_clear(SDL_FColor col) {
}

void GRAPHICS_set_state(const graphics_state* state) {
}

void GRAPHICS_draw_vertices(graphics_texture* texture,
                            const graphics_vertex* v,
                            size_t n) {
    for (size_t i = 0; i < n && vertices_size < SDL_arraysize(vertices); i++)
        vertices[vertices_size++] = v[i];
    if (n) vertices_texture = texture;
}

void GRAPHICS_draw_texture(graphics_texture* texture,
                           const SDL_FRect* src,
                           const SDL_FRect* dst,
                           SDL_FlipMode flip_mode,
                           SDL_FColor col) {
}

void GRAPHICS_fill_rect(SDL_FRect rect, [[maybe_unused]] SDL_FColor col) {
    if (fill_rect_calls_size == SUB_CALLS_MAX) return;
    fill_rect_calls[fill_rect_calls_size++].rect = rect;
}

void GRAPHICS_draw_point(float x, float y, [[maybe_unused]] SDL_FColor col) {
    if (draw_point_calls_size == SUB_CALLS_MAX) return;
    draw_point_calls[draw_point_calls_size++] =
        (GRAPHICS_draw_point_call){x, y};
}



size_t get_GRAPHICS_vertices(graphics_vertex** ptr,
                             graphics_texture** texture) {
    *ptr = vertices;
    *texture = vertices_texture;
    return vertices_size;
}

size_t get_GRAPHICS_draw_point_calls(GRAPHICS_draw_point_call** ptr) {
    *ptr = draw_point_calls;
    return draw_point_calls_size;
}

size_t get_GRAPHICS_fill_rect_calls(GRAPHICS_fill_rect_call** ptr) {
    *ptr = fill_rect_calls;
    return fill_rect_calls_size;
}

void clear_stubbed_calls(void) {
    for (size_t i = 0; i < vertices_size; i++)
        vertices[i] = (graphics_vertex){0};
    vertices_size = 0;
    for (size_t i = 0; i < draw_point_calls_size; i++)
        draw_point_calls[i] = (GRAPHICS_draw_point_call){0};
    draw_point_calls_size = 0;
    for (size_t i = 0; i < fill_rect_calls_size; i++)
        fill_rect_calls[i] = (GRAPHICS_fill_rect_call){0};
    fill_rect_calls_size = 0;
}
