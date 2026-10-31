#include "engine/graphics.h"
#include "defs.h"
#include <SDL3/SDL.h>
#include <stddef.h>

typedef enum {
    command_pass,
    command_draw,
} command_type;

// A recorded command, with `type` saying which of the union's structs has its
// data. Passes start drawing on `target`, `NULL` being the swapchain texture,
// and draws draw `num_vertices` vertices from `first_vertex` on the frame's
// vertex buffer.
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
            Uint8 uniforms[GRAPHICS_MAX_UNIFORMS_SIZE];
            Uint32 uniforms_size;
            Uint32 first_vertex, num_vertices;
        } draw;
    };
} command;

// A pipeline drawing with `shader`, blended by `blend` on a target of
// `format`.
typedef struct {
    SDL_GPUShader* shader;
    graphics_blend blend;
    SDL_GPUTextureFormat format;
    SDL_GPUGraphicsPipeline* pipeline;
} pipeline_entry;

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
static graphics_vertex* vertices = NULL;
static size_t vertices_len = 0;
static size_t vertices_cap = 0;
static command* commands = NULL;
static size_t commands_len = 0;
static size_t commands_cap = 0;
static graphics_texture* target = NULL;
static bool has_target = false;
static graphics_state state = {0};
static Uint8 state_uniforms[GRAPHICS_MAX_UNIFORMS_SIZE] = {0};
static bool state_changed = true;

// Vertices are uploaded through these every frame. They're recreated bigger
// when a frame has more vertices than fit.
static SDL_GPUBuffer* vertex_buffer = NULL;
static SDL_GPUTransferBuffer* transfer_buffer = NULL;
static Uint32 vertex_buffer_size = 0;



static SDL_GPUShader* load_shader(const char* name,
                                  graphics_shader_code code,
                                  SDL_GPUShaderStage stage,
                                  Uint32 num_samplers,
                                  Uint32 num_uniform_buffers) {
    SDL_GPUShaderCreateInfo info = (SDL_GPUShaderCreateInfo){
        .entrypoint = "main",
        .stage = stage,
        .num_samplers = num_samplers,
        .num_uniform_buffers = num_uniform_buffers,
    };
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShader* shader = NULL;
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = code.spirv;
        info.code_size = code.spirv_size;
        shader = SDL_CreateGPUShader(device, &info);
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = code.dxil;
        info.code_size = code.dxil_size;
        shader = SDL_CreateGPUShader(device, &info);
    } else {
        SDL_SetError("No supported shader format");
    }

    if (!shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create %s: %s",
                     name,
                     SDL_GetError());
        return NULL;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Shader %s loaded successfully.", name);
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
                       num_uniform_buffers);
}

void GRAPHICS_release_shader(SDL_GPUShader* shader) {
    if (shader) SDL_ReleaseGPUShader(device, shader);
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
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Failed to create window: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(window, MIN_WINDOW_SIZE_X, MIN_WINDOW_SIZE_Y);
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Window initialized successfully.");
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
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_GPUDevice: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SDL_GPUDevice created successfully.");

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

    static const Uint8 vert_spirv[] = {
#embed GRAPHICS_BASE_VERT_SPIRV_PATH
    };
    static const Uint8 vert_dxil[] = {
#embed GRAPHICS_BASE_VERT_DXIL_PATH
    };
    base_vert = load_shader("base_vert",
                            GRAPHICS_SHADER_CODE(vert_spirv, vert_dxil),
                            SDL_GPU_SHADERSTAGE_VERTEX,
                            0,
                            1);
    if (!base_vert) return 1;

    static const Uint8 frag_spirv[] = {
#embed GRAPHICS_BASE_FRAG_SPIRV_PATH
    };
    static const Uint8 frag_dxil[] = {
#embed GRAPHICS_BASE_FRAG_DXIL_PATH
    };
    base_frag = GRAPHICS_load_fragment_shader(
        "base_frag", GRAPHICS_SHADER_CODE(frag_spirv, frag_dxil), 1, 0);
    if (!base_frag) return 1;

    nearest_sampler = create_sampler(SDL_GPU_FILTER_NEAREST);
    linear_sampler = create_sampler(SDL_GPU_FILTER_LINEAR);
    if (!nearest_sampler || !linear_sampler) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create samplers: %s",
                     SDL_GetError());
        return 1;
    }

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
    SDL_free(vertices);
    SDL_free(commands);
    vertices = NULL;
    commands = NULL;
    vertices_len = vertices_cap = commands_len = commands_cap = 0;

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
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create texture %s: %s",
                     name,
                     SDL_GetError());
        return NULL;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Texture %s created successfully.", name);

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
// `GRAPHICS_stop()`. Returns `NULL` on errors.
static SDL_GPUGraphicsPipeline* get_pipeline(SDL_GPUShader* shader,
                                             graphics_blend blend,
                                             SDL_GPUTextureFormat fmt) {
    for (int i = 0; i < pipelines_len; i++) {
        pipeline_entry* e = &pipelines[i];
        if (e->shader == shader && e->blend == blend && e->format == fmt)
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
    SDL_GPUColorTargetDescription color_target = {
        .format = fmt,
        .blend_state = get_blend_state(blend),
    };
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(
        device,
        &(SDL_GPUGraphicsPipelineCreateInfo){
            .vertex_shader = base_vert,
            .fragment_shader = shader,
            .vertex_input_state =
                {
                    .vertex_buffer_descriptions = &buffer,
                    .num_vertex_buffers = 1,
                    .vertex_attributes = attributes,
                    .num_vertex_attributes = 3,
                },
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .target_info =
                {
                    .color_target_descriptions = &color_target,
                    .num_color_targets = 1,
                },
        });
    if (!pipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create graphics pipeline: %s",
                     SDL_GetError());
        return NULL;
    }
    pipelines[pipelines_len++] = (pipeline_entry){shader, blend, fmt, pipeline};
    return pipeline;
}



static command* push_command(command c) {
    if (commands_len == commands_cap) {
        commands_cap =
            commands_cap ? commands_cap << 1 : GRAPHICS_COMMANDS_STARTING_CAP;
        commands = SDL_realloc(commands, commands_cap * sizeof(command));
    }
    commands[commands_len] = c;
    return &commands[commands_len++];
}

void GRAPHICS_begin_frame(void) {
    commands_len = 0;
    vertices_len = 0;
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

// Passes on the swapchain texture are skipped when there is none this frame.
void GRAPHICS_end_frame(void) {
    if (!cmd) return;

    if (vertices_len) {
        Uint32 size = vertices_len * sizeof(graphics_vertex);
        if (size > vertex_buffer_size) {
            SDL_ReleaseGPUBuffer(device, vertex_buffer);
            SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
            vertex_buffer_size = SDL_max(vertex_buffer_size, 1);
            while (vertex_buffer_size < size)
                vertex_buffer_size <<= 1;
            vertex_buffer =
                SDL_CreateGPUBuffer(device,
                                    &(SDL_GPUBufferCreateInfo){
                                        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                        .size = vertex_buffer_size,
                                    });
            transfer_buffer = SDL_CreateGPUTransferBuffer(
                device,
                &(SDL_GPUTransferBufferCreateInfo){
                    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                    .size = vertex_buffer_size,
                });
        }
        void* map = SDL_MapGPUTransferBuffer(device, transfer_buffer, true);
        SDL_memcpy(map, vertices, size);
        SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        SDL_UploadToGPUBuffer(copy,
                              &(SDL_GPUTransferBufferLocation){
                                  .transfer_buffer = transfer_buffer,
                              },
                              &(SDL_GPUBufferRegion){
                                  .buffer = vertex_buffer,
                                  .size = size,
                              },
                              true);
        SDL_EndGPUCopyPass(copy);
    }

    SDL_GPURenderPass* pass = NULL;
    SDL_GPUGraphicsPipeline* bound = NULL;
    for (size_t i = 0; i < commands_len; i++) {
        command* c = &commands[i];
        if (c->type == command_pass) {
            if (pass) SDL_EndGPURenderPass(pass);
            pass = NULL;
            bound = NULL;

            SDL_GPUTexture* texture =
                c->pass.target ? c->pass.target->texture : swapchain;
            if (!texture) continue;
            float size[2] = {
                c->pass.target ? c->pass.target->w : swapchain_w,
                c->pass.target ? c->pass.target->h : swapchain_h,
            };
            SDL_FColor col = c->pass.clear_col;
            pass = SDL_BeginGPURenderPass(
                cmd,
                &(SDL_GPUColorTargetInfo){
                    .texture = texture,
                    .clear_color = {col.r, col.g, col.b, col.a},
                    .load_op = c->pass.clear ? SDL_GPU_LOADOP_CLEAR
                                             : SDL_GPU_LOADOP_LOAD,
                    .store_op = SDL_GPU_STOREOP_STORE,
                },
                1,
                NULL);
            SDL_PushGPUVertexUniformData(cmd, 0, size, sizeof(size));
            if (vertex_buffer)
                SDL_BindGPUVertexBuffers(
                    pass,
                    0,
                    &(SDL_GPUBufferBinding){.buffer = vertex_buffer},
                    1);
            continue;
        }

        if (!pass || !c->draw.pipeline) continue;
        if (c->draw.pipeline != bound) {
            SDL_BindGPUGraphicsPipeline(pass, c->draw.pipeline);
            bound = c->draw.pipeline;
        }
        SDL_BindGPUFragmentSamplers(
            pass, 0, c->draw.samplers, c->draw.num_samplers);
        if (c->draw.uniforms_size)
            SDL_PushGPUFragmentUniformData(
                cmd, 0, c->draw.uniforms, c->draw.uniforms_size);
        SDL_DrawGPUPrimitives(
            pass, c->draw.num_vertices, 1, c->draw.first_vertex, 0);
    }
    if (pass) SDL_EndGPURenderPass(pass);

    SDL_SubmitGPUCommandBuffer(cmd);
    cmd = NULL;
}



void GRAPHICS_set_target(graphics_texture* t) {
    target = t;
    has_target = true;
    push_command((command){.type = command_pass, .pass = {.target = t}});
}

// Clears through the load op of the target's pass, starting a new one when
// something was already drawn on it.
void GRAPHICS_clear(SDL_FColor col) {
    if (!has_target || commands[commands_len - 1].type != command_pass)
        GRAPHICS_set_target(target);
    command* c = &commands[commands_len - 1];
    c->pass.clear = true;
    c->pass.clear_col = col;
}

void GRAPHICS_set_state(const graphics_state* s) {
    state = s ? *s : (graphics_state){0};
    if (state.uniforms_size)
        SDL_memcpy(state_uniforms, state.uniforms, state.uniforms_size);
    state.uniforms = state_uniforms;
    state_changed = true;
}

// Starts a draw of `texture` with the current state on the current target.
static void push_draw(graphics_texture* texture) {
    command c = {.type = command_draw};
    c.draw.pipeline =
        get_pipeline(state.shader ? state.shader : base_frag,
                     state.blend,
                     target ? target->format
                            : SDL_GetGPUSwapchainTextureFormat(device, window));
    c.draw.samplers[0] = GRAPHICS_binding(texture);
    SDL_memcpy(&c.draw.samplers[1],
               state.samplers,
               state.num_samplers * sizeof(SDL_GPUTextureSamplerBinding));
    c.draw.num_samplers = 1 + state.num_samplers;
    SDL_memcpy(c.draw.uniforms, state.uniforms, state.uniforms_size);
    c.draw.uniforms_size = state.uniforms_size;
    c.draw.first_vertex = vertices_len;
    push_command(c);
    state_changed = false;
}

// Draws are only started when something changed since the last one: the
// target, through the pass it starts, the state or the texture. Otherwise the
// vertices are added to the last draw.
void GRAPHICS_draw_vertices(graphics_texture* texture,
                            const graphics_vertex* v,
                            size_t n) {
    if (n == 0) return;
    if (!has_target) GRAPHICS_set_target(NULL);

    command* last = &commands[commands_len - 1];
    if (state_changed || last->type != command_draw ||
        last->draw.samplers[0].texture != texture->texture)
        push_draw(texture);
    commands[commands_len - 1].draw.num_vertices += n;

    if (vertices_len + n > vertices_cap) {
        while (vertices_len + n > vertices_cap)
            vertices_cap = vertices_cap ? vertices_cap << 1
                                        : GRAPHICS_VERTICES_STARTING_CAP;
        vertices =
            SDL_realloc(vertices, vertices_cap * sizeof(graphics_vertex));
    }
    SDL_memcpy(&vertices[vertices_len], v, n * sizeof(graphics_vertex));
    vertices_len += n;
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
