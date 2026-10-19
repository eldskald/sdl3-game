#include "engine/renderer.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/sprites.h"
#include "engine/tilemap.h"
#include <SDL3/SDL.h>

#ifndef TEST
static SDL_Window* window = NULL;
static SDL_Texture* base_screen = NULL;
static SDL_GPUDevice* device = NULL;
static SDL_GPUShader* fluids_shader = NULL;
#endif
static SDL_Renderer* renderer = NULL;
static SDL_Texture* spritesheet = NULL;
static SDL_Texture* fluids_texture = NULL;
static SDL_GPURenderState* fluids_render_state = NULL;



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

    // Creating SDL_Texture for the fluids
    fluids_texture = SDL_CreateTexture(renderer,
                                       SDL_PIXELFORMAT_ABGR8888,
                                       SDL_TEXTUREACCESS_TARGET,
                                       WINDOW_SIZE_X,
                                       WINDOW_SIZE_Y);
    if (!fluids_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture fluids_texture: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(fluids_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(fluids_texture, SDL_BLENDMODE_BLEND);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture fluids_texture created successfully.");
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

    // Fluids shader and render state
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
                                         1,
                                         1);
    if (!fluids_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create fluids_shader: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_GPURenderStateCreateInfo info = (SDL_GPURenderStateCreateInfo){
        .fragment_shader = fluids_shader,
    };
    fluids_render_state = SDL_CreateGPURenderState(renderer, &info);
    if (!fluids_render_state) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create fluids_render_state: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Fluids shader loaded successfully.");
#endif

    return 0;
}



static void render_base_screen(void) {
#ifndef TEST
    SDL_SetRenderTarget(renderer, base_screen);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);
#endif

    FLUIDS_draw(renderer, fluids_texture, fluids_render_state);
    SPRITES_draw(renderer, spritesheet);
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
    SDL_DestroyGPURenderState(fluids_render_state);
    SDL_ReleaseGPUShader(device, fluids_shader);
    SDL_DestroyTexture(fluids_texture);
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
