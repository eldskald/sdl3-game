#include "renderer.h"
#include "defs.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>

static SDL_Texture* spritesheet = NULL;
static SDL_GPUDevice* gpu = NULL;
static SDL_GPUTexture* base_screen = NULL;

int RENDERER_start(char* base_path, SDL_Renderer* renderer) {
    char* sheet_path = NULL;
    SDL_asprintf(&sheet_path, "%s%stest.png", base_path, RES_TEXTURES_PATH);

    SDL_Surface* surface = SDL_LoadPNG(sheet_path);
    if (!surface) {
        SDL_Log("Couldn't load spritesheet: %s", SDL_GetError());
        return 1;
    }
    SDL_free(sheet_path);

    spritesheet = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    SDL_Log("Spritesheet loaded successfully.");

    gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (!gpu) {
        SDL_Log("Couldn't create SDL_GPUDevice: %s", SDL_GetError());
        return 1;
    }
    SDL_Log("SDL_GPUDevice loaded successfully.");

    base_screen =
        SDL_CreateGPUTexture(gpu,
                             &(SDL_GPUTextureCreateInfo){
                                 .type = SDL_GPU_TEXTURETYPE_2D,
                                 .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                 .width = WINDOW_SIZE_X,
                                 .height = WINDOW_SIZE_Y,
                                 .layer_count_or_depth = 1,
                                 .num_levels = 1,
                                 .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER});
    if (!base_screen) {
        SDL_Log("Couldn't create SDL_GPUTexture: %s", SDL_GetError());
        return 1;
    }
    SDL_Log("GPUTexture created successfully.");

    return 0;
}

void RENDERER_stop(void) {
    SDL_DestroyTexture(spritesheet);
    SDL_ReleaseGPUTexture(gpu, base_screen);
    SDL_DestroyGPUDevice(gpu);
}

// static void render_base_screen() {
// }

void RENDERER_update(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);

    SDL_FRect dstrect = {100, 100, 32, 32};
    SDL_RenderTexture(renderer, spritesheet, NULL, &dstrect);

    SDL_RenderPresent(renderer);
}
