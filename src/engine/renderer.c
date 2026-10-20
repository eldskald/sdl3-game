#include "engine/renderer.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/sprites.h"
#include "engine/tilemap.h"
#include <SDL3/SDL.h>

#ifndef TEST
static SDL_Window* window = NULL;
static SDL_Texture* base_screen = NULL;
static SDL_Texture* pre_fluids_texture = NULL;
static SDL_GPUDevice* device = NULL;
static SDL_GPUShader* fluids_shader = NULL;
static SDL_GPUShader* ripples_shader = NULL;
static SDL_GPUSampler* nearest_sampler = NULL;
#endif
static SDL_Renderer* renderer = NULL;
static SDL_Texture* spritesheet = NULL;
static fluids_canvas fluids = (fluids_canvas){0};



static int start_graphics_pipeline() {
#ifndef TEST
    // Initialize video
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Failed to initialize video! %s",
                     SDL_GetError());
        return 1;
    }

    // Create window
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

    // Creating SDL_Renderer on top of the GPU device. It claims the window
    // for the device, so we must not call SDL_ClaimWindowForGPUDevice().
    renderer = SDL_CreateGPURenderer(device, window);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Couldn't create SDL_Renderer: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "SDL_Renderer created successfully.");

    // Creating SDL_Texture for the base game
    base_screen = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ABGR8888,
                                    SDL_TEXTUREACCESS_TARGET,
                                    WINDOW_SIZE_X,
                                    WINDOW_SIZE_Y);
    if (!base_screen) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture base_screen: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(base_screen, SDL_SCALEMODE_NEAREST);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture base_screen created successfully.");

    // Creating SDL_Texture for everything drawn before the fluids, for them to
    // refract
    pre_fluids_texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_ABGR8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           WINDOW_SIZE_X,
                                           WINDOW_SIZE_Y);
    if (!pre_fluids_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture pre_fluids_texture: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(pre_fluids_texture, SDL_SCALEMODE_NEAREST);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture pre_fluids_texture created successfully.");

    // Creating SDL_Texture for the fluids
    fluids.texture = SDL_CreateTexture(renderer,
                                       SDL_PIXELFORMAT_ABGR8888,
                                       SDL_TEXTUREACCESS_TARGET,
                                       WINDOW_SIZE_X,
                                       WINDOW_SIZE_Y);
    if (!fluids.texture) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture fluids_texture: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(fluids.texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(fluids.texture, SDL_BLENDMODE_BLEND);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture fluids_texture created successfully.");

    // Creating SDL_Textures for the ripples. They are float so heights can be
    // negative, and start cleared since GPU textures start undefined.
    for (Uint8 i = 0; i < 2; i++) {
        fluids.ripples[i] = SDL_CreateTexture(renderer,
                                              SDL_PIXELFORMAT_RGBA64_FLOAT,
                                              SDL_TEXTUREACCESS_TARGET,
                                              WINDOW_SIZE_X,
                                              WINDOW_SIZE_Y);
        if (!fluids.ripples[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU,
                         "Couldn't create SDL_Texture ripples: %s",
                         SDL_GetError());
            return 1;
        }
        SDL_SetTextureScaleMode(fluids.ripples[i], SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(fluids.ripples[i], SDL_BLENDMODE_NONE);
        SDL_SetRenderTarget(renderer, fluids.ripples[i]);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
    }
    SDL_SetRenderTarget(renderer, NULL);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture ripples created successfully.");

    // Sampler for the fluids and ripples shaders to sample the extra textures
    // they're given
    nearest_sampler = SDL_CreateGPUSampler(
        device,
        &(SDL_GPUSamplerCreateInfo){
            .min_filter = SDL_GPU_FILTER_NEAREST,
            .mag_filter = SDL_GPU_FILTER_NEAREST,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        });
    if (!nearest_sampler) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create nearest_sampler: %s",
                     SDL_GetError());
        return 1;
    }
#endif

    return 0;
}

#ifndef TEST
static SDL_GPUShader* load_fragment_shader(const Uint8* spirv,
                                           size_t spirv_size,
                                           const Uint8* dxil,
                                           size_t dxil_size,
                                           Uint32 num_samplers,
                                           Uint32 num_uniform_buffers) {
    SDL_GPUShaderCreateInfo info = (SDL_GPUShaderCreateInfo){
        .entrypoint = "main",
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = num_samplers,
        .num_uniform_buffers = num_uniform_buffers,
    };
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = dxil;
        info.code_size = dxil_size;
    } else {
        SDL_SetError("No supported shader format");
        return NULL;
    }
    return SDL_CreateGPUShader(device, &info);
}
#endif

static int load_graphics_files(void) {
#ifndef TEST
    Uint8 mem[] = {
#embed SPRITESHEET_PATH
    };
    SDL_IOStream* stream = SDL_IOFromConstMem(mem, sizeof(mem));
    SDL_Surface* surface = SDL_LoadPNG_IO(stream, true);
    spritesheet = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Spritesheet loaded successfully.");

    // Fluids shader and render states, one sampling each ripples texture since
    // they are swapped every tick. Both sample pre_fluids_texture as well.
    static const Uint8 fluids_spirv[] = {
#embed FLUIDS_FRAG_SPIRV_PATH
    };
    static const Uint8 fluids_dxil[] = {
#embed FLUIDS_FRAG_DXIL_PATH
    };
    fluids_shader = load_fragment_shader(fluids_spirv,
                                         sizeof(fluids_spirv),
                                         fluids_dxil,
                                         sizeof(fluids_dxil),
                                         3,
                                         1);
    if (!fluids_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create fluids_shader: %s",
                     SDL_GetError());
        return 1;
    }
    for (Uint8 i = 0; i < 2; i++) {
        SDL_GPUTextureSamplerBinding bindings[2] = {
            (SDL_GPUTextureSamplerBinding){
                .texture = SDL_GetPointerProperty(
                    SDL_GetTextureProperties(fluids.ripples[i]),
                    SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER,
                    NULL),
                .sampler = nearest_sampler,
            },
            (SDL_GPUTextureSamplerBinding){
                .texture = SDL_GetPointerProperty(
                    SDL_GetTextureProperties(pre_fluids_texture),
                    SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER,
                    NULL),
                .sampler = nearest_sampler,
            },
        };
        SDL_GPURenderStateCreateInfo info = (SDL_GPURenderStateCreateInfo){
            .fragment_shader = fluids_shader,
            .num_sampler_bindings = 2,
            .sampler_bindings = bindings,
        };
        fluids.render_states[i] = SDL_CreateGPURenderState(renderer, &info);
        if (!fluids.render_states[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU,
                         "Couldn't create fluids_render_state: %s",
                         SDL_GetError());
            return 1;
        }
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Fluids shader loaded successfully.");

    // Ripples shader and render state
    static const Uint8 ripples_spirv[] = {
#embed FLUIDS_RIPPLES_FRAG_SPIRV_PATH
    };
    static const Uint8 ripples_dxil[] = {
#embed FLUIDS_RIPPLES_FRAG_DXIL_PATH
    };
    ripples_shader = load_fragment_shader(ripples_spirv,
                                          sizeof(ripples_spirv),
                                          ripples_dxil,
                                          sizeof(ripples_dxil),
                                          2,
                                          0);
    if (!ripples_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create ripples_shader: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_GPUTextureSamplerBinding fluids_binding =
        (SDL_GPUTextureSamplerBinding){
            .texture =
                SDL_GetPointerProperty(SDL_GetTextureProperties(fluids.texture),
                                       SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER,
                                       NULL),
            .sampler = nearest_sampler,
    };
    fluids.ripples_state =
        SDL_CreateGPURenderState(renderer,
                                 &(SDL_GPURenderStateCreateInfo){
                                     .fragment_shader = ripples_shader,
                                     .num_sampler_bindings = 1,
                                     .sampler_bindings = &fluids_binding,
                                 });
    if (!fluids.ripples_state) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create ripples_render_state: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Ripples shader loaded successfully.");
#endif

    return 0;
}



// Sprites are drawn on pre_fluids_texture first, for the fluids to refract.
// The fluids shader draws all of it on base_screen, refracted where there are
// fluids and as is everywhere else, then the tilemap is drawn on top.
static void render_base_screen(void) {
#ifndef TEST
    SDL_SetRenderTarget(renderer, pre_fluids_texture);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);
#endif

    SPRITES_draw(renderer, spritesheet);

#ifndef TEST
    SDL_SetRenderTarget(renderer, base_screen);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);
#endif

    FLUIDS_draw(renderer, &fluids);
    TILEMAP_draw(renderer, spritesheet);

#ifndef TEST
    SDL_RenderPresent(renderer);
#endif
}



static void render_final_screen(void) {
#ifndef TEST
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);

    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);
    float window_ar = (float)window_w / (float)window_h;
    float base_ar = (float)WINDOW_SIZE_X / (float)WINDOW_SIZE_Y;

    // Center and stretch base_screen
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

    // Render base_screen
    SDL_FRect dstrect = (SDL_FRect){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    SDL_RenderTexture(renderer, base_screen, NULL, &dstrect);

    SDL_RenderPresent(renderer);
#endif
}



int RENDERER_start(void) {
    if (start_graphics_pipeline()) return 1;
    if (load_graphics_files()) return 1;
    return 0;
}


void RENDERER_stop(void) {
#ifndef TEST
    SDL_DestroyGPURenderState(fluids.ripples_state);
    SDL_ReleaseGPUShader(device, ripples_shader);
    for (Uint8 i = 0; i < 2; i++) {
        SDL_DestroyGPURenderState(fluids.render_states[i]);
        SDL_DestroyTexture(fluids.ripples[i]);
    }
    SDL_ReleaseGPUShader(device, fluids_shader);
    SDL_ReleaseGPUSampler(device, nearest_sampler);
    SDL_DestroyTexture(fluids.texture);
    SDL_DestroyTexture(pre_fluids_texture);
    SDL_DestroyTexture(base_screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
}


void RENDERER_update(void) {
    render_base_screen();
    render_final_screen();
}
