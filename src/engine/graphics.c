#include "engine/graphics.h"
#include "data-structs/dynarr.h"
#include "defs.h"
#include "paths.h"
#include "utils/logger.h"
#include <SDL3/SDL.h>
#include <stddef.h>

typedef enum {
    command_pass,
    command_draw,
    command_dispatch,
} command_type;

// A recorded command, with `type` saying which of the union's structs has its
// data. Passes start drawing on `target`, `NULL` being the swapchain texture.
// Draws of vertices draw `num_vertices` vertices from `first_vertex` on the
// frame's vertex buffer, and draws of instances draw `num_instances` instances
// of `num_vertices` vertices, or what's on their `indirect_buffer`. Dispatches
// have their uniforms copied.
typedef struct {
    command_type type;
    union {
        struct {
            graphics_texture* target;
            bool clear;
            SDL_FColor clear_col;
        } pass;
        struct {
            SDL_GPUGraphicsPipeline* pipeline;
            SDL_GPUTextureSamplerBinding
                samplers[1 + GRAPHICS_MAX_EXTRA_SAMPLERS];
            Uint32 num_samplers;
            Uint8 fragment_uniforms[GRAPHICS_MAX_UNIFORMS_SIZE];
            Uint32 fragment_uniforms_size;
            SDL_GPUBuffer* storage_buffers[GRAPHICS_MAX_STORAGE_BUFFERS];
            Uint32 num_storage_buffers;
            Uint8 vertex_uniforms[GRAPHICS_MAX_UNIFORMS_SIZE];
            Uint32 vertex_uniforms_size;
            Uint32 first_vertex, num_vertices, num_instances;
            SDL_GPUBuffer* indirect_buffer;
            Uint32 indirect_offset;
        } draw;
        struct {
            graphics_dispatch dispatch;
            Uint8 uniforms[GRAPHICS_MAX_UNIFORMS_SIZE];
        } dispatch;
    };
} command;

// An upload queued during a frame, of the `size` bytes `staging_offset` bytes
// into the frame's staging bytes, into `buffer` at `offset`.
typedef struct {
    SDL_GPUBuffer* buffer;
    Uint32 offset;
    Uint32 size;
    Uint32 staging_offset;
} upload;

// A pipeline drawing with `vertex` and `fragment`, blended by `blend` on a
// target of `format`, taking `graphics_vertex` from the vertex buffer when
// `vertex_input` is set.
typedef struct {
    SDL_GPUShader* vertex;
    SDL_GPUShader* fragment;
    graphics_blend blend;
    SDL_GPUTextureFormat format;
    bool vertex_input;
    SDL_GPUGraphicsPipeline* pipeline;
} pipeline_entry;

static const Uint8 vert_spirv[] = {
#embed GRAPHICS_BASE_VERT_SPIRV_PATH
};
static const Uint8 vert_dxil[] = {
#embed GRAPHICS_BASE_VERT_DXIL_PATH
};
static const Uint8 frag_spirv[] = {
#embed GRAPHICS_BASE_FRAG_SPIRV_PATH
};
static const Uint8 frag_dxil[] = {
#embed GRAPHICS_BASE_FRAG_DXIL_PATH
};

static SDL_Window* window = NULL;
static SDL_GPUDevice* device = NULL;
static SDL_GPUShader* base_vert = NULL;
static SDL_GPUShader* base_frag = NULL;
static SDL_GPUSampler* nearest_sampler = NULL;
static SDL_GPUSampler* linear_sampler = NULL;
static graphics_texture* white = NULL;

static pipeline_entry pipelines[GRAPHICS_MAX_PIPELINES] = {0};
static int pipelines_len = 0;

// The frame being recorded. `state_changed` is set when the state is, so the
// next draw starts a new command instead of extending the last one.
static SDL_GPUCommandBuffer* cmd = NULL;
static SDL_GPUTexture* swapchain = NULL;
static Uint32 swapchain_w = 0;
static Uint32 swapchain_h = 0;
static dynarr vertices = (dynarr){0}; // graphics_vertex dynarr
static dynarr commands = (dynarr){0}; // command dynarr
static dynarr uploads = (dynarr){0};  // upload dynarr
static dynarr staging = (dynarr){0};  // Uint8 dynarr, the uploads' bytes
static graphics_texture* target = NULL;
static bool has_target = false;
static graphics_state state = {0};
static Uint8 state_vertex_uniforms[GRAPHICS_MAX_UNIFORMS_SIZE] = {0};
static Uint8 state_fragment_uniforms[GRAPHICS_MAX_UNIFORMS_SIZE] = {0};
static bool state_changed = true;

// The frame's vertices are drawn from `vertex_buffer`, and they and the
// frame's uploads go through `transfer_buffer`, vertices first. Both are
// recreated bigger when a frame needs more than fits.
static SDL_GPUBuffer* vertex_buffer = NULL;
static Uint32 vertex_buffer_size = 0;
static SDL_GPUTransferBuffer* transfer_buffer = NULL;
static Uint32 transfer_buffer_size = 0;



// Picks the format of `code` the device supports, writing it and its code on
// `format`, `bytes` and `size`. Returns false when it supports none.
static bool pick_code(graphics_shader_code code,
                      SDL_GPUShaderFormat* format,
                      const Uint8** bytes,
                      size_t* size) {
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        *format = SDL_GPU_SHADERFORMAT_SPIRV;
        *bytes = code.spirv;
        *size = code.spirv_size;
        return true;
    }
    if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        *format = SDL_GPU_SHADERFORMAT_DXIL;
        *bytes = code.dxil;
        *size = code.dxil_size;
        return true;
    }
    SDL_SetError("No supported shader format");
    return false;
}

static SDL_GPUShader* load_shader(const char* name,
                                  graphics_shader_code code,
                                  SDL_GPUShaderStage stage,
                                  Uint32 num_samplers,
                                  Uint32 num_storage_buffers,
                                  Uint32 num_uniform_buffers) {
    SDL_GPUShaderCreateInfo info = (SDL_GPUShaderCreateInfo){
        .entrypoint = "main",
        .stage = stage,
        .num_samplers = num_samplers,
        .num_storage_buffers = num_storage_buffers,
        .num_uniform_buffers = num_uniform_buffers,
    };
    SDL_GPUShader* shader = NULL;
    if (pick_code(code, &info.format, &info.code, &info.code_size))
        shader = SDL_CreateGPUShader(device, &info);
    log_creation(SDL_LOG_CATEGORY_GPU, shader, "shader %s", name);
    return shader;
}

SDL_GPUShader* GRAPHICS_load_fragment_shader(const char* name,
                                             graphics_shader_code code,
                                             Uint32 num_samplers,
                                             Uint32 num_uniform_buffers) {
    return load_shader(name,
                       code,
                       SDL_GPU_SHADERSTAGE_FRAGMENT,
                       num_samplers,
                       0,
                       num_uniform_buffers);
}

SDL_GPUShader* GRAPHICS_load_vertex_shader(const char* name,
                                           graphics_shader_code code,
                                           Uint32 num_storage_buffers,
                                           Uint32 num_uniform_buffers) {
    return load_shader(name,
                       code,
                       SDL_GPU_SHADERSTAGE_VERTEX,
                       0,
                       num_storage_buffers,
                       num_uniform_buffers);
}

void GRAPHICS_release_shader(SDL_GPUShader* shader) {
    if (shader) SDL_ReleaseGPUShader(device, shader);
}

SDL_GPUComputePipeline*
GRAPHICS_load_compute_pipeline(const char* name,
                               graphics_shader_code code,
                               Uint32 num_readonly_buffers,
                               Uint32 num_readwrite_buffers,
                               Uint32 num_uniform_buffers,
                               Uint32 threads) {
    SDL_GPUComputePipelineCreateInfo info = {
        .entrypoint = "main",
        .num_readonly_storage_buffers = num_readonly_buffers,
        .num_readwrite_storage_buffers = num_readwrite_buffers,
        .num_uniform_buffers = num_uniform_buffers,
        .threadcount_x = threads,
        .threadcount_y = 1,
        .threadcount_z = 1,
    };
    SDL_GPUComputePipeline* pipeline = NULL;
    if (pick_code(code, &info.format, &info.code, &info.code_size))
        pipeline = SDL_CreateGPUComputePipeline(device, &info);
    log_creation(SDL_LOG_CATEGORY_GPU, pipeline, "compute pipeline %s", name);
    return pipeline;
}

void GRAPHICS_release_compute_pipeline(SDL_GPUComputePipeline* pipeline) {
    if (pipeline) SDL_ReleaseGPUComputePipeline(device, pipeline);
}

static SDL_GPUSampler* create_sampler(SDL_GPUFilter filter) {
    return SDL_CreateGPUSampler(
        device,
        &(SDL_GPUSamplerCreateInfo){
            .min_filter = filter,
            .mag_filter = filter,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        });
}

static int create_window(void) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Failed to initialize video! %s",
                     SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(WINDOW_TITLE,
                              STARTING_WINDOW_SIZE_X,
                              STARTING_WINDOW_SIZE_Y,
                              SDL_WINDOW_RESIZABLE);
    if (!log_creation(SDL_LOG_CATEGORY_VIDEO, window, "window")) return 1;
    SDL_SetWindowMinimumSize(window, MIN_WINDOW_SIZE_X, MIN_WINDOW_SIZE_Y);
    return 0;
}

static int create_device(void) {
#ifdef DEV
    bool gpu_debug_mode = true;
#else
    bool gpu_debug_mode = false;
#endif
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                     SDL_GPU_SHADERFORMAT_DXIL,
                                 gpu_debug_mode,
                                 NULL);
    if (!log_creation(SDL_LOG_CATEGORY_GPU, device, "GPU device")) return 1;

#ifdef TEST
    // Tests run on the offscreen video driver, whose windows can't be claimed
    // without `VK_EXT_headless_surface`, and only draw on textures, so there's
    // never a swapchain texture.
    return 0;
#endif
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't claim window for SDL_GPUDevice: %s",
                     SDL_GetError());
        return 1;
    }

    // The game loop caps its own rate, so presenting shouldn't wait for vsync,
    // preferring mailbox since it doesn't tear.
    SDL_GPUPresentMode present_mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(
            device, window, SDL_GPU_PRESENTMODE_MAILBOX))
        present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
    else if (SDL_WindowSupportsGPUPresentMode(
                 device, window, SDL_GPU_PRESENTMODE_IMMEDIATE))
        present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    SDL_SetGPUSwapchainParameters(
        device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Window claimed successfully.");
    return 0;
}

int GRAPHICS_start(void) {
    if (create_window()) return 1;
    if (create_device()) return 1;
    init_dynarr_with(
        &vertices, sizeof(graphics_vertex), GRAPHICS_VERTICES_STARTING_CAP);
    init_dynarr_with(
        &commands, sizeof(command), GRAPHICS_COMMANDS_STARTING_CAP);
    init_dynarr_with(&uploads, sizeof(upload), GRAPHICS_UPLOADS_STARTING_CAP);
    init_dynarr_with(&staging, sizeof(Uint8), GRAPHICS_STAGING_STARTING_CAP);

    base_vert = load_shader("base_vert",
                            GRAPHICS_SHADER_CODE(vert_spirv, vert_dxil),
                            SDL_GPU_SHADERSTAGE_VERTEX,
                            0,
                            0,
                            1);
    if (!base_vert) return 1;

    base_frag = GRAPHICS_load_fragment_shader(
        "base_frag", GRAPHICS_SHADER_CODE(frag_spirv, frag_dxil), 1, 0);
    if (!base_frag) return 1;

    nearest_sampler = create_sampler(SDL_GPU_FILTER_NEAREST);
    if (!log_creation(SDL_LOG_CATEGORY_GPU, nearest_sampler, "sampler nearest"))
        return 1;
    linear_sampler = create_sampler(SDL_GPU_FILTER_LINEAR);
    if (!log_creation(SDL_LOG_CATEGORY_GPU, linear_sampler, "sampler linear"))
        return 1;

    // White pixel that rects and points are drawn with, so they use the same
    // shader as textures.
    SDL_Surface* surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
    SDL_FillSurfaceRect(surface, NULL, 0xFFFFFFFF);
    white = GRAPHICS_create_texture_from_surface(
        "white", surface, SDL_GPU_FILTER_NEAREST);
    SDL_DestroySurface(surface);
    if (!white) return 1;

    return 0;
}

void GRAPHICS_stop(void) {
    deinit_dynarr(&vertices);
    deinit_dynarr(&commands);
    deinit_dynarr(&uploads);
    deinit_dynarr(&staging);

    if (device) {
        SDL_WaitForGPUIdle(device);
        for (int i = 0; i < pipelines_len; i++)
            SDL_ReleaseGPUGraphicsPipeline(device, pipelines[i].pipeline);
        pipelines_len = 0;
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        GRAPHICS_destroy_texture(white);
        SDL_ReleaseGPUSampler(device, linear_sampler);
        SDL_ReleaseGPUSampler(device, nearest_sampler);
        GRAPHICS_release_shader(base_frag);
        GRAPHICS_release_shader(base_vert);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        device = NULL;
    }
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}



static graphics_texture* create_texture(const char* name,
                                        SDL_GPUTextureFormat format,
                                        Uint32 w,
                                        Uint32 h,
                                        SDL_GPUTextureUsageFlags usage,
                                        SDL_GPUFilter filter) {
    SDL_GPUTexture* texture =
        SDL_CreateGPUTexture(device,
                             &(SDL_GPUTextureCreateInfo){
                                 .type = SDL_GPU_TEXTURETYPE_2D,
                                 .format = format,
                                 .usage = usage,
                                 .width = w,
                                 .height = h,
                                 .layer_count_or_depth = 1,
                                 .num_levels = 1,
                             });
    if (!log_creation(SDL_LOG_CATEGORY_GPU, texture, "texture %s", name))
        return NULL;

    graphics_texture* t = SDL_malloc(sizeof(graphics_texture));
    *t = (graphics_texture){
        .texture = texture,
        .sampler =
            filter == SDL_GPU_FILTER_LINEAR ? linear_sampler : nearest_sampler,
        .format = format,
        .w = w,
        .h = h,
    };
    return t;
}

// GPU textures start undefined, so it's cleared on its own command buffer,
// submitted right away.
graphics_texture* GRAPHICS_create_texture(const char* name,
                                          SDL_GPUTextureFormat format,
                                          Uint32 w,
                                          Uint32 h,
                                          SDL_GPUFilter filter) {
    graphics_texture* t = create_texture(name,
                                         format,
                                         w,
                                         h,
                                         SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                             SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
                                         filter);
    if (!t) return NULL;
    SDL_GPUCommandBuffer* clear_cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_EndGPURenderPass(
        SDL_BeginGPURenderPass(clear_cmd,
                               &(SDL_GPUColorTargetInfo){
                                   .texture = t->texture,
                                   .load_op = SDL_GPU_LOADOP_CLEAR,
                                   .store_op = SDL_GPU_STOREOP_STORE,
                               },
                               1,
                               NULL));
    SDL_SubmitGPUCommandBuffer(clear_cmd);
    return t;
}

graphics_texture* GRAPHICS_create_texture_from_surface(const char* name,
                                                       SDL_Surface* surface,
                                                       SDL_GPUFilter filter) {
    graphics_texture* t = create_texture(name,
                                         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                         surface->w,
                                         surface->h,
                                         SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                         filter);
    if (t) GRAPHICS_upload_surface(t, surface);
    return t;
}

SDL_GPUTextureSamplerBinding GRAPHICS_binding(graphics_texture* texture) {
    return (SDL_GPUTextureSamplerBinding){
        .texture = texture->texture,
        .sampler = texture->sampler,
    };
}

void GRAPHICS_destroy_texture(graphics_texture* texture) {
    if (!texture) return;
    SDL_ReleaseGPUTexture(device, texture->texture);
    SDL_free(texture);
}

// Uses its own command buffer, submitted right away, so it runs before the
// frame being recorded, which is only submitted on `GRAPHICS_end_frame()`. The
// texture is cycled, so frames still in flight keep sampling the old contents.
void GRAPHICS_upload_surface(graphics_texture* texture, SDL_Surface* surface) {
    SDL_Surface* converted = surface;
    if (surface->format != SDL_PIXELFORMAT_RGBA32)
        converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);

    Uint32 row = converted->w * 4;
    Uint32 size = row * converted->h;
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(
        device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = size,
        });
    Uint8* map = SDL_MapGPUTransferBuffer(device, transfer, false);
    for (int y = 0; y < converted->h; y++)
        SDL_memcpy(map + y * row,
                   (Uint8*)converted->pixels + y * converted->pitch,
                   row);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(upload_cmd);
    SDL_UploadToGPUTexture(copy,
                           &(SDL_GPUTextureTransferInfo){
                               .transfer_buffer = transfer,
                           },
                           &(SDL_GPUTextureRegion){
                               .texture = texture->texture,
                               .w = converted->w,
                               .h = converted->h,
                               .d = 1,
                           },
                           true);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);

    if (converted != surface) SDL_DestroySurface(converted);
}



SDL_GPUBuffer* GRAPHICS_create_buffer(const char* name,
                                      SDL_GPUBufferUsageFlags usage,
                                      Uint32 size) {
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(
        device, &(SDL_GPUBufferCreateInfo){.usage = usage, .size = size});
    if (!log_creation(SDL_LOG_CATEGORY_GPU, buffer, "buffer %s", name))
        return NULL;

    // GPU buffers start undefined, like textures.
    void* zeros = SDL_calloc(1, size);
    GRAPHICS_upload_buffer(buffer, 0, zeros, size);
    SDL_free(zeros);
    return buffer;
}

void GRAPHICS_destroy_buffer(SDL_GPUBuffer* buffer) {
    if (buffer) SDL_ReleaseGPUBuffer(device, buffer);
}

// Outside of a frame, it's uploaded like `GRAPHICS_upload_surface()`, on its
// own command buffer submitted right away. During one, its bytes are queued on
// `staging` instead, and `GRAPHICS_end_frame()` uploads them all on its own
// copy pass, before its dispatches and draws. Unlike textures, the buffer
// isn't cycled either way: cycling swaps it for a fresh one, which would lose
// what's outside the uploaded region, like particles that live on the GPU
// across frames. Frames still in flight finish using it before it's written
// instead.
void GRAPHICS_upload_buffer(SDL_GPUBuffer* buffer,
                            Uint32 offset,
                            const void* data,
                            Uint32 size) {
    if (size == 0) return;
    if (cmd) {
        push_to_dynarr(&(upload){buffer, offset, size, staging.len}, &uploads);
        SDL_memcpy(grow_dynarr(size, &staging), data, size);
        return;
    }

    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(
        device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = size,
        });
    void* map = SDL_MapGPUTransferBuffer(device, transfer, false);
    SDL_memcpy(map, data, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(upload_cmd);
    SDL_UploadToGPUBuffer(copy,
                          &(SDL_GPUTransferBufferLocation){
                              .transfer_buffer = transfer,
                          },
                          &(SDL_GPUBufferRegion){
                              .buffer = buffer,
                              .offset = offset,
                              .size = size,
                          },
                          false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}



static SDL_GPUColorTargetBlendState get_blend_state(graphics_blend blend) {
    switch (blend) {
    case graphics_blend_blend:
        return (SDL_GPUColorTargetBlendState){
            .enable_blend = true,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        };
    case graphics_blend_add:
        return (SDL_GPUColorTargetBlendState){
            .enable_blend = true,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        };
    default:
        return (SDL_GPUColorTargetBlendState){0};
    }
}

// Pipelines are created the first time they're needed and kept until
// `GRAPHICS_stop()`. Only those of draws of vertices take the vertex buffer,
// those of draws of instances build their vertices themselves. Returns `NULL`
// on errors.
static SDL_GPUGraphicsPipeline* get_pipeline(SDL_GPUShader* vertex,
                                             SDL_GPUShader* fragment,
                                             graphics_blend blend,
                                             SDL_GPUTextureFormat fmt,
                                             bool vertex_input) {
    for (int i = 0; i < pipelines_len; i++) {
        pipeline_entry* e = &pipelines[i];
        if (e->vertex == vertex && e->fragment == fragment &&
            e->blend == blend && e->format == fmt &&
            e->vertex_input == vertex_input)
            return e->pipeline;
    }
    if (pipelines_len == GRAPHICS_MAX_PIPELINES) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Too many pipelines, increase GRAPHICS_MAX_PIPELINES.");
        return NULL;
    }

    SDL_GPUVertexBufferDescription buffer = {
        .slot = 0,
        .pitch = sizeof(graphics_vertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    SDL_GPUVertexAttribute attributes[3] = {
        {0,
         0,
         SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
         offsetof(graphics_vertex, x)},
        {1,
         0,
         SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
         offsetof(graphics_vertex, u)},
        {2,
         0,
         SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         offsetof(graphics_vertex, col)},
    };
    SDL_GPUVertexInputState input = {0};
    if (vertex_input)
        input = (SDL_GPUVertexInputState){
            .vertex_buffer_descriptions = &buffer,
            .num_vertex_buffers = 1,
            .vertex_attributes = attributes,
            .num_vertex_attributes = 3,
        };
    SDL_GPUColorTargetDescription color_target = {
        .format = fmt,
        .blend_state = get_blend_state(blend),
    };
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(
        device,
        &(SDL_GPUGraphicsPipelineCreateInfo){
            .vertex_shader = vertex,
            .fragment_shader = fragment,
            .vertex_input_state = input,
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .target_info =
                {
                    .color_target_descriptions = &color_target,
                    .num_color_targets = 1,
                },
        });
    if (!log_creation(SDL_LOG_CATEGORY_GPU,
                      pipeline,
                      "graphics pipeline %d",
                      pipelines_len))
        return NULL;
    pipelines[pipelines_len++] =
        (pipeline_entry){vertex, fragment, blend, fmt, vertex_input, pipeline};
    return pipeline;
}



static command* last_command(void) {
    return dynarr_at(commands.len - 1, &commands);
}

void GRAPHICS_begin_frame(void) {
    clear_dynarr(&commands);
    clear_dynarr(&vertices);
    clear_dynarr(&uploads);
    clear_dynarr(&staging);
    target = NULL;
    has_target = false;
    GRAPHICS_set_state(NULL);
    swapchain = NULL;
    swapchain_w = swapchain_h = 0;

    cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't acquire command buffer: %s",
                     SDL_GetError());
        return;
    }
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmd, window, &swapchain, &swapchain_w, &swapchain_h))
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't acquire swapchain texture: %s",
                     SDL_GetError());
}

bool GRAPHICS_get_swapchain_size(Uint32* w, Uint32* h) {
    *w = swapchain_w;
    *h = swapchain_h;
    return swapchain != NULL;
}

// Smallest power of 2 that's at least `size`, and at least `current`.
static Uint32 grow_size(Uint32 current, Uint32 size) {
    current = SDL_max(current, 1);
    while (current < size)
        current <<= 1;
    return current;
}

// Copies the frame's vertices and uploads into the transfer buffer, vertices
// first, and uploads them all on one copy pass, recreating the buffers bigger
// when they don't fit.
static void upload_frame(void) {
    Uint32 vertices_size = vertices.len * sizeof(graphics_vertex);
    Uint32 total = vertices_size + staging.len;
    if (total == 0) return;

    if (vertices_size > vertex_buffer_size) {
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
        vertex_buffer_size = grow_size(vertex_buffer_size, vertices_size);
        vertex_buffer =
            SDL_CreateGPUBuffer(device,
                                &(SDL_GPUBufferCreateInfo){
                                    .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                    .size = vertex_buffer_size,
                                });
    }
    if (total > transfer_buffer_size) {
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        transfer_buffer_size = grow_size(transfer_buffer_size, total);
        transfer_buffer = SDL_CreateGPUTransferBuffer(
            device,
            &(SDL_GPUTransferBufferCreateInfo){
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                .size = transfer_buffer_size,
            });
    }

    Uint8* map = SDL_MapGPUTransferBuffer(device, transfer_buffer, true);
    SDL_memcpy(map, vertices.data, vertices_size);
    SDL_memcpy(map + vertices_size, staging.data, staging.len);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    if (vertices_size)
        SDL_UploadToGPUBuffer(copy,
                              &(SDL_GPUTransferBufferLocation){
                                  .transfer_buffer = transfer_buffer,
                              },
                              &(SDL_GPUBufferRegion){
                                  .buffer = vertex_buffer,
                                  .size = vertices_size,
                              },
                              true);
    for (size_t i = 0; i < uploads.len; i++) {
        upload* u = dynarr_at(i, &uploads);
        SDL_UploadToGPUBuffer(copy,
                              &(SDL_GPUTransferBufferLocation){
                                  .transfer_buffer = transfer_buffer,
                                  .offset = vertices_size + u->staging_offset,
                              },
                              &(SDL_GPUBufferRegion){
                                  .buffer = u->buffer,
                                  .offset = u->offset,
                                  .size = u->size,
                              },
                              false);
    }
    SDL_EndGPUCopyPass(copy);
}

// Begins a render pass on the target of `c`, a pass command, clearing it when
// `clear` is set. Returns `NULL` when it's the swapchain texture and there is
// none this frame.
static SDL_GPURenderPass* begin_pass(const command* c, bool clear) {
    SDL_GPUTexture* texture =
        c->pass.target ? c->pass.target->texture : swapchain;
    if (!texture) return NULL;
    float size[2] = {
        c->pass.target ? c->pass.target->w : swapchain_w,
        c->pass.target ? c->pass.target->h : swapchain_h,
    };
    SDL_FColor col = c->pass.clear_col;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(
        cmd,
        &(SDL_GPUColorTargetInfo){
            .texture = texture,
            .clear_color = {col.r, col.g, col.b, col.a},
            .load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD,
            .store_op = SDL_GPU_STOREOP_STORE,
        },
        1,
        NULL);
    SDL_PushGPUVertexUniformData(cmd, 0, size, sizeof(size));
    if (vertex_buffer)
        SDL_BindGPUVertexBuffers(
            pass, 0, &(SDL_GPUBufferBinding){.buffer = vertex_buffer}, 1);
    return pass;
}

// Each dispatch gets its own compute pass, since SDL only makes what a pass
// wrote visible to the passes after it.
static void run_dispatch(const command* c) {
    const graphics_dispatch* d = &c->dispatch.dispatch;
    SDL_GPUStorageBufferReadWriteBinding rw[GRAPHICS_MAX_STORAGE_BUFFERS];
    for (Uint32 b = 0; b < d->num_readwrite_buffers; b++)
        rw[b] = (SDL_GPUStorageBufferReadWriteBinding){
            .buffer = d->readwrite_buffers[b],
        };
    SDL_GPUComputePass* compute =
        SDL_BeginGPUComputePass(cmd, NULL, 0, rw, d->num_readwrite_buffers);
    SDL_BindGPUComputePipeline(compute, d->pipeline);
    if (d->num_readonly_buffers)
        SDL_BindGPUComputeStorageBuffers(
            compute, 0, d->readonly_buffers, d->num_readonly_buffers);
    if (d->uniforms_size)
        SDL_PushGPUComputeUniformData(
            cmd, 0, c->dispatch.uniforms, d->uniforms_size);
    SDL_DispatchGPUCompute(compute, d->groups, 1, 1);
    SDL_EndGPUComputePass(compute);
}

static void run_draw(SDL_GPURenderPass* pass, const command* c) {
    SDL_BindGPUFragmentSamplers(
        pass, 0, c->draw.samplers, c->draw.num_samplers);
    if (c->draw.fragment_uniforms_size)
        SDL_PushGPUFragmentUniformData(
            cmd, 0, c->draw.fragment_uniforms, c->draw.fragment_uniforms_size);
    if (c->draw.num_storage_buffers)
        SDL_BindGPUVertexStorageBuffers(
            pass, 0, c->draw.storage_buffers, c->draw.num_storage_buffers);
    if (c->draw.vertex_uniforms_size)
        SDL_PushGPUVertexUniformData(
            cmd, 1, c->draw.vertex_uniforms, c->draw.vertex_uniforms_size);
    if (c->draw.indirect_buffer)
        SDL_DrawGPUPrimitivesIndirect(
            pass, c->draw.indirect_buffer, c->draw.indirect_offset, 1);
    else
        SDL_DrawGPUPrimitives(pass,
                              c->draw.num_vertices,
                              c->draw.num_instances,
                              c->draw.first_vertex,
                              0);
}

// Passes on the swapchain texture are skipped when there is none this frame.
// A dispatch ends the render pass it's in, and the draws after it resume on
// the same target, loading what was drawn before it.
void GRAPHICS_end_frame(void) {
    if (!cmd) return;
    upload_frame();

    SDL_GPURenderPass* pass = NULL;
    SDL_GPUGraphicsPipeline* bound = NULL;
    const command* current = NULL; // Last pass command, to resume it.
    for (size_t i = 0; i < commands.len; i++) {
        const command* c = dynarr_at(i, &commands);
        switch (c->type) {
        case command_pass:
            if (pass) SDL_EndGPURenderPass(pass);
            current = c;
            pass = begin_pass(c, c->pass.clear);
            bound = NULL;
            break;

        case command_dispatch:
            if (pass) SDL_EndGPURenderPass(pass);
            pass = NULL;
            run_dispatch(c);
            break;

        case command_draw:
            if (!pass && current) {
                pass = begin_pass(current, false);
                bound = NULL;
            }
            if (!pass || !c->draw.pipeline) break;
            if (c->draw.pipeline != bound) {
                SDL_BindGPUGraphicsPipeline(pass, c->draw.pipeline);
                bound = c->draw.pipeline;
            }
            run_draw(pass, c);
            break;
        }
    }
    if (pass) SDL_EndGPURenderPass(pass);

    SDL_SubmitGPUCommandBuffer(cmd);
    cmd = NULL;
}



void GRAPHICS_set_target(graphics_texture* t) {
    target = t;
    has_target = true;
    push_to_dynarr(&(command){.type = command_pass, .pass = {.target = t}},
                   &commands);
}

// Clears through the load op of the target's pass, starting a new one when
// something was already drawn on it.
void GRAPHICS_clear(SDL_FColor col) {
    if (!has_target || last_command()->type != command_pass)
        GRAPHICS_set_target(target);
    command* c = last_command();
    c->pass.clear = true;
    c->pass.clear_col = col;
}

void GRAPHICS_set_state(const graphics_state* s) {
    state = s ? *s : (graphics_state){0};
    if (state.vertex_uniforms_size)
        SDL_memcpy(state_vertex_uniforms,
                   state.vertex_uniforms,
                   state.vertex_uniforms_size);
    if (state.fragment_uniforms_size)
        SDL_memcpy(state_fragment_uniforms,
                   state.fragment_uniforms,
                   state.fragment_uniforms_size);
    state.vertex_uniforms = state_vertex_uniforms;
    state.fragment_uniforms = state_fragment_uniforms;
    state_changed = true;
}

// Starts a draw of `texture` with the current state on the current target,
// with nothing to draw yet. Draws of vertices take them from the vertex
// buffer, with `vertex_input`.
static command* push_draw(graphics_texture* texture, bool vertex_input) {
    if (!has_target) GRAPHICS_set_target(NULL);
    command c = {.type = command_draw};
    c.draw.num_instances = 1;
    c.draw.pipeline =
        get_pipeline(state.vertex ? state.vertex : base_vert,
                     state.fragment ? state.fragment : base_frag,
                     state.blend,
                     target ? target->format
                            : SDL_GetGPUSwapchainTextureFormat(device, window),
                     vertex_input);
    c.draw.samplers[0] = GRAPHICS_binding(texture);
    SDL_memcpy(&c.draw.samplers[1],
               state.samplers,
               state.num_samplers * sizeof(SDL_GPUTextureSamplerBinding));
    c.draw.num_samplers = 1 + state.num_samplers;
    SDL_memcpy(c.draw.fragment_uniforms,
               state.fragment_uniforms,
               state.fragment_uniforms_size);
    c.draw.fragment_uniforms_size = state.fragment_uniforms_size;
    SDL_memcpy(c.draw.storage_buffers,
               state.storage_buffers,
               state.num_storage_buffers * sizeof(SDL_GPUBuffer*));
    c.draw.num_storage_buffers = state.num_storage_buffers;
    SDL_memcpy(c.draw.vertex_uniforms,
               state.vertex_uniforms,
               state.vertex_uniforms_size);
    c.draw.vertex_uniforms_size = state.vertex_uniforms_size;
    c.draw.first_vertex = vertices.len;
    state_changed = false;
    return push_to_dynarr(&c, &commands);
}

// Draws are only started when something changed since the last one: the
// target, through the pass it starts, the state or the texture. Otherwise the
// vertices are added to the last draw.
void GRAPHICS_draw_vertices(graphics_texture* texture,
                            const graphics_vertex* v,
                            size_t n) {
    if (n == 0) return;
    command* last = has_target ? last_command() : NULL;
    if (!last || state_changed || last->type != command_draw ||
        last->draw.samplers[0].texture != texture->texture)
        last = push_draw(texture, true);
    last->draw.num_vertices += n;

    SDL_memcpy(grow_dynarr(n, &vertices), v, n * sizeof(graphics_vertex));
}

void GRAPHICS_draw_texture(graphics_texture* texture,
                           const SDL_FRect* src,
                           const SDL_FRect* dst,
                           SDL_FlipMode flip_mode,
                           SDL_FColor col) {
    SDL_FRect s = src ? *src : (SDL_FRect){0, 0, texture->w, texture->h};
    SDL_FRect d = {0};
    if (dst) {
        d = *dst;
    } else {
        d.w = target ? target->w : swapchain_w;
        d.h = target ? target->h : swapchain_h;
    }
    graphics_vertex v[6];
    GRAPHICS_quad(v, d, GRAPHICS_uv(texture, s, flip_mode), col);
    GRAPHICS_draw_vertices(texture, v, 6);
}

void GRAPHICS_fill_rect(SDL_FRect rect, SDL_FColor col) {
    graphics_vertex v[6];
    GRAPHICS_quad(v, rect, (SDL_FRect){0.0f, 0.0f, 1.0f, 1.0f}, col);
    GRAPHICS_draw_vertices(white, v, 6);
}

void GRAPHICS_draw_point(float x, float y, SDL_FColor col) {
    GRAPHICS_fill_rect((SDL_FRect){x, y, 1.0f, 1.0f}, col);
}

// Always a draw of its own, so the next `GRAPHICS_draw_vertices()` starts a
// new one instead of extending it.
void GRAPHICS_draw_instances(graphics_texture* texture,
                             const graphics_instances* instances) {
    if (!state.vertex) return;
    if (!instances->indirect_buffer &&
        (instances->num_vertices == 0 || instances->num_instances == 0))
        return;

    command* c = push_draw(texture, false);
    c->draw.first_vertex = 0;
    c->draw.num_vertices = instances->num_vertices;
    c->draw.num_instances = instances->num_instances;
    c->draw.indirect_buffer = instances->indirect_buffer;
    c->draw.indirect_offset = instances->indirect_offset;
    state_changed = true;
}

void GRAPHICS_dispatch(const graphics_dispatch* dispatch) {
    command* c = push_to_dynarr(
        &(command){.type = command_dispatch, .dispatch = {*dispatch}},
        &commands);
    SDL_memcpy(
        c->dispatch.uniforms, dispatch->uniforms, dispatch->uniforms_size);
    c->dispatch.dispatch.uniforms = NULL;
}
