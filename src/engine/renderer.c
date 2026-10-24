#include "engine/renderer.h"
#include "defs.h"
#include "engine/fluids.h"
#include "engine/sprites.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include <SDL3/SDL.h>

#ifndef TEST
static SDL_Window* window = NULL;
static SDL_Texture* base_screen = NULL;
static SDL_Texture* final_screen = NULL;
static SDL_Texture* bloom_prefilter = NULL;
static SDL_Texture* bloom_chain[BLOOM_MAX_ITERATIONS] = {0};
static SDL_Texture* pre_fluids_texture = NULL;
static SDL_GPUDevice* device = NULL;
static SDL_GPUShader* fluids_shader = NULL;
static SDL_GPUShader* ripples_shader = NULL;
static SDL_GPUShader* tonemap_shader = NULL;
static SDL_GPURenderState* tonemap_state = NULL;
static SDL_GPUShader* tint_shader = NULL;
static SDL_GPURenderState* tint_state = NULL;
static SDL_GPUShader* bloom_prefilter_shader = NULL;
static SDL_GPURenderState* bloom_prefilter_state = NULL;
static SDL_GPUShader* box_sample_shader = NULL;
static SDL_GPURenderState* downsample_state = NULL;
static SDL_GPURenderState* upsample_states[BLOOM_MAX_ITERATIONS] = {0};
static SDL_GPUSampler* nearest_sampler = NULL;
#endif
static SDL_Renderer* renderer = NULL;
static SDL_Texture* spritesheet = NULL;
static fluids_canvas fluids = (fluids_canvas){0};
static float bloom_threshold = 1.0f;
static float bloom_soft_threshold = 0.5f;
static int bloom_iterations = 4;
static float bloom_intensities[BLOOM_MAX_ITERATIONS] = {0};
static bool window_resized = false;



#ifndef TEST
// Float render target, so colors can go beyond 1.0. It's half float since
// SDL's GPU renderer doesn't support RGBA128_FLOAT, it would silently make a
// native RGBA64_FLOAT texture behind it, linear and without a GPU texture
// pointer to bind to shaders. Its colorspace is sRGB, otherwise float textures
// default to linear and SDL would convert everything drawn on it.
static SDL_Texture* create_hdr_target(int w, int h) {
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props,
                          SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                          SDL_PIXELFORMAT_RGBA64_FLOAT);
    SDL_SetNumberProperty(
        props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_TARGET);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, w);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, h);
    SDL_SetNumberProperty(
        props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB);
    SDL_Texture* texture = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    }
    return texture;
}
#endif



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

    // Creating SDL_Texture for the base game. It's HDR for post processing,
    // and tone mapped onto final_screen.
    base_screen = create_hdr_target(WINDOW_SIZE_X, WINDOW_SIZE_Y);
    if (!base_screen) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture base_screen: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture base_screen created successfully.");

    // Creating SDL_Texture for the parts of base_screen bright enough to bloom.
    // It's filtered linearly for the downsampling to blur it.
    bloom_prefilter = create_hdr_target(WINDOW_SIZE_X, WINDOW_SIZE_Y);
    if (!bloom_prefilter) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture bloom_prefilter: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(bloom_prefilter, SDL_SCALEMODE_LINEAR);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture bloom_prefilter created successfully.");

    // Creating SDL_Textures for the bloom's downsampling chain, each half the
    // size of the previous one, starting at half of bloom_prefilter. Also
    // filtered linearly. The chain is cut short when it would get too small.
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
        bloom_chain[i] = create_hdr_target(w, h);
        if (!bloom_chain[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU,
                         "Couldn't create SDL_Texture bloom_chain: %s",
                         SDL_GetError());
            return 1;
        }
        SDL_SetTextureScaleMode(bloom_chain[i], SDL_SCALEMODE_LINEAR);
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture bloom_chain created successfully.");

    // Creating SDL_Texture for base_screen tone mapped, which is what's drawn
    // on the window
    final_screen = SDL_CreateTexture(renderer,
                                     SDL_PIXELFORMAT_ABGR8888,
                                     SDL_TEXTUREACCESS_TARGET,
                                     WINDOW_SIZE_X,
                                     WINDOW_SIZE_Y);
    if (!final_screen) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture final_screen: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(final_screen, SDL_SCALEMODE_NEAREST);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture final_screen created successfully.");

    // Creating SDL_Texture for everything drawn before the fluids, for them to
    // refract. It's HDR, since it's drawn as is on base_screen where there are
    // no fluids.
    pre_fluids_texture = create_hdr_target(WINDOW_SIZE_X, WINDOW_SIZE_Y);
    if (!pre_fluids_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create SDL_Texture pre_fluids_texture: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "SDL_Texture pre_fluids_texture created successfully.");

    // Creating SDL_Texture for the fluids. It's bigger than the screen so the
    // ripples can be simulated off screen too.
    fluids.texture =
        SDL_CreateTexture(renderer,
                          SDL_PIXELFORMAT_ABGR8888,
                          SDL_TEXTUREACCESS_TARGET,
                          FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_X,
                          FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_Y);
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

    // Creating SDL_Textures for the ripples, the same size as the fluids'. They
    // are float so heights can be negative, and start cleared since GPU
    // textures start undefined.
    for (Uint8 i = 0; i < 2; i++) {
        fluids.ripples[i] = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA64_FLOAT,
            SDL_TEXTUREACCESS_TARGET,
            FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_X,
            FLUIDS_RIPPLES_TEXTURE_RES_FACTOR * WINDOW_SIZE_Y);
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

static void load_bloom_data_from_csv(void) {
    char csv[] = {
#embed BLOOM_CSV_PATH suffix(, '\0')
    };

    for (int i = 0; i < BLOOM_MAX_ITERATIONS; i++)
        bloom_intensities[i] = 1.0f;

    dynarr parsed = CSV_parse(csv);

    for (int i = 0; i < parsed.len; i++) {
        if (((dynarr*)parsed.at[i])->len < 2) continue;
        char* key = (char*)((dynarr*)parsed.at[i])->at[0];
        char* val = (char*)((dynarr*)parsed.at[i])->at[1];
        if (SDL_strcmp(key, "threshold") == 0)
            bloom_threshold = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "soft_threshold") == 0)
            bloom_soft_threshold = SDL_strtod(val, NULL);
        else if (SDL_strcmp(key, "intensities") == 0) {
            dynarr* row = (dynarr*)parsed.at[i];
            bloom_iterations = row->len - 1;
            for (int j = 1; j < row->len && j <= BLOOM_MAX_ITERATIONS; j++)
                bloom_intensities[j - 1] = SDL_strtod((char*)row->at[j], NULL);
        }
    }

    CSV_free(&parsed);

    bloom_iterations = SDL_clamp(bloom_iterations, 1, BLOOM_MAX_ITERATIONS);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Bloom data loaded successfully.");
}

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

    // Tone mapping shader and render state
    static const Uint8 tonemap_spirv[] = {
#embed TONEMAP_FRAG_SPIRV_PATH
    };
    static const Uint8 tonemap_dxil[] = {
#embed TONEMAP_FRAG_DXIL_PATH
    };
    tonemap_shader = load_fragment_shader(tonemap_spirv,
                                          sizeof(tonemap_spirv),
                                          tonemap_dxil,
                                          sizeof(tonemap_dxil),
                                          1,
                                          0);
    if (!tonemap_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create tonemap_shader: %s",
                     SDL_GetError());
        return 1;
    }
    tonemap_state =
        SDL_CreateGPURenderState(renderer,
                                 &(SDL_GPURenderStateCreateInfo){
                                     .fragment_shader = tonemap_shader,
                                 });
    if (!tonemap_state) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create tonemap_render_state: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Tonemap shader loaded successfully.");

    // Tint shader and render state, for sprites. Its uniforms are set per
    // sprite.
    static const Uint8 tint_spirv[] = {
#embed TINT_FRAG_SPIRV_PATH
    };
    static const Uint8 tint_dxil[] = {
#embed TINT_FRAG_DXIL_PATH
    };
    tint_shader = load_fragment_shader(
        tint_spirv, sizeof(tint_spirv), tint_dxil, sizeof(tint_dxil), 1, 1);
    if (!tint_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create tint_shader: %s",
                     SDL_GetError());
        return 1;
    }
    tint_state = SDL_CreateGPURenderState(renderer,
                                          &(SDL_GPURenderStateCreateInfo){
                                              .fragment_shader = tint_shader,
                                          });
    if (!tint_state) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create tint_render_state: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Tint shader loaded successfully.");

    // Bloom prefilter shader and render state. Its thresholds never change, so
    // its uniforms are set once here.
    static const Uint8 bloom_prefilter_spirv[] = {
#embed BLOOM_PREFILTER_FRAG_SPIRV_PATH
    };
    static const Uint8 bloom_prefilter_dxil[] = {
#embed BLOOM_PREFILTER_FRAG_DXIL_PATH
    };
    bloom_prefilter_shader = load_fragment_shader(bloom_prefilter_spirv,
                                                  sizeof(bloom_prefilter_spirv),
                                                  bloom_prefilter_dxil,
                                                  sizeof(bloom_prefilter_dxil),
                                                  1,
                                                  1);
    if (!bloom_prefilter_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create bloom_prefilter_shader: %s",
                     SDL_GetError());
        return 1;
    }
    bloom_prefilter_state =
        SDL_CreateGPURenderState(renderer,
                                 &(SDL_GPURenderStateCreateInfo){
                                     .fragment_shader = bloom_prefilter_shader,
                                 });
    if (!bloom_prefilter_state) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create bloom_prefilter_render_state: %s",
                     SDL_GetError());
        return 1;
    }
    float bloom_prefilter_uniforms[2] = {bloom_threshold, bloom_soft_threshold};
    SDL_SetGPURenderStateFragmentUniforms(bloom_prefilter_state,
                                          0,
                                          bloom_prefilter_uniforms,
                                          sizeof(bloom_prefilter_uniforms));
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU,
                "Bloom prefilter shader loaded successfully.");

    // Box sample shader and its render states, one for downsampling and one
    // for each upsampling level. Uniforms belong to the render state, so each
    // _Delta and _Intensity gets its own state instead of changing it between
    // draws in a frame.
    static const Uint8 box_sample_spirv[] = {
#embed BOX_SAMPLE_FRAG_SPIRV_PATH
    };
    static const Uint8 box_sample_dxil[] = {
#embed BOX_SAMPLE_FRAG_DXIL_PATH
    };
    box_sample_shader = load_fragment_shader(box_sample_spirv,
                                             sizeof(box_sample_spirv),
                                             box_sample_dxil,
                                             sizeof(box_sample_dxil),
                                             1,
                                             1);
    if (!box_sample_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create box_sample_shader: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_GPURenderStateCreateInfo box_sample_info =
        (SDL_GPURenderStateCreateInfo){
            .fragment_shader = box_sample_shader,
    };
    downsample_state = SDL_CreateGPURenderState(renderer, &box_sample_info);
    if (!downsample_state) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "Couldn't create box_sample_render_state: %s",
                     SDL_GetError());
        return 1;
    }
    float downsample_uniforms[2] = {1.0f, 1.0f};
    SDL_SetGPURenderStateFragmentUniforms(
        downsample_state, 0, downsample_uniforms, sizeof(downsample_uniforms));
    for (int i = 0; i < bloom_iterations; i++) {
        upsample_states[i] =
            SDL_CreateGPURenderState(renderer, &box_sample_info);
        if (!upsample_states[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU,
                         "Couldn't create box_sample_render_state: %s",
                         SDL_GetError());
            return 1;
        }
        float upsample_uniforms[2] = {0.5f, bloom_intensities[i]};
        SDL_SetGPURenderStateFragmentUniforms(upsample_states[i],
                                              0,
                                              upsample_uniforms,
                                              sizeof(upsample_uniforms));
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Box sample shader loaded successfully.");
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

#ifndef TEST
    SPRITES_draw(renderer, spritesheet, tint_state);
#else
    SPRITES_draw(renderer, spritesheet, NULL);
#endif

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



// Bloom: the parts of base_screen bright enough to bloom are filtered onto
// bloom_prefilter, which is downsampled down bloom_chain, blurring it more each
// time. Then it's upsampled back up, each level multiplied by its intensity and
// added onto the one above it, and the first one is added onto base_screen.
// Then base_screen is tone mapped onto final_screen.
static void render_post_processing(void) {
#ifndef TEST
    SDL_SetRenderTarget(renderer, bloom_prefilter);
    SDL_SetGPURenderState(renderer, bloom_prefilter_state);
    SDL_RenderTexture(renderer, base_screen, NULL, NULL);

    SDL_SetGPURenderState(renderer, downsample_state);
    SDL_Texture* source = bloom_prefilter;
    for (int i = 0; i < bloom_iterations; i++) {
        SDL_SetTextureBlendMode(source, SDL_BLENDMODE_NONE);
        SDL_SetRenderTarget(renderer, bloom_chain[i]);
        SDL_RenderTexture(renderer, source, NULL, NULL);
        source = bloom_chain[i];
    }

    for (int i = bloom_iterations - 1; i > 0; i--) {
        SDL_SetGPURenderState(renderer, upsample_states[i]);
        SDL_SetTextureBlendMode(bloom_chain[i], SDL_BLENDMODE_ADD);
        SDL_SetRenderTarget(renderer, bloom_chain[i - 1]);
        SDL_RenderTexture(renderer, bloom_chain[i], NULL, NULL);
    }
    SDL_SetGPURenderState(renderer, upsample_states[0]);
    SDL_SetTextureBlendMode(bloom_chain[0], SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer, base_screen);
    SDL_RenderTexture(renderer, bloom_chain[0], NULL, NULL);

    SDL_SetRenderTarget(renderer, final_screen);
    SDL_SetGPURenderState(renderer, tonemap_state);
    SDL_RenderTexture(renderer, base_screen, NULL, NULL);
    SDL_SetGPURenderState(renderer, NULL);
#endif
}



static void render_final_screen(void) {
#ifndef TEST
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, COL_BLACK);
    SDL_RenderClear(renderer);

    // SDL's GPU renderer only resizes its backbuffer on present, so drawing on
    // the frame the window grew would use a viewport bigger than the
    // backbuffer. Clearing doesn't use the viewport, so just clear and present
    // this frame to resize it.
    if (window_resized) {
        window_resized = false;
        SDL_RenderPresent(renderer);
        return;
    }

    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);
    float window_ar = (float)window_w / (float)window_h;
    float base_ar = (float)WINDOW_SIZE_X / (float)WINDOW_SIZE_Y;

    // Center and stretch final_screen
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

    // Render final_screen
    SDL_FRect dstrect = (SDL_FRect){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    SDL_RenderTexture(renderer, final_screen, NULL, &dstrect);

    SDL_RenderPresent(renderer);
#endif
}



int RENDERER_start(void) {
    load_bloom_data_from_csv();
    if (start_graphics_pipeline()) return 1;
    if (load_graphics_files()) return 1;
    return 0;
}


void RENDERER_stop(void) {
#ifndef TEST
    for (int i = 0; i < bloom_iterations; i++)
        SDL_DestroyGPURenderState(upsample_states[i]);
    SDL_DestroyGPURenderState(downsample_state);
    SDL_ReleaseGPUShader(device, box_sample_shader);
    SDL_DestroyGPURenderState(bloom_prefilter_state);
    SDL_ReleaseGPUShader(device, bloom_prefilter_shader);
    SDL_DestroyGPURenderState(tint_state);
    SDL_ReleaseGPUShader(device, tint_shader);
    SDL_DestroyGPURenderState(tonemap_state);
    SDL_ReleaseGPUShader(device, tonemap_shader);
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
    for (int i = 0; i < bloom_iterations; i++)
        SDL_DestroyTexture(bloom_chain[i]);
    SDL_DestroyTexture(bloom_prefilter);
    SDL_DestroyTexture(final_screen);
    SDL_DestroyTexture(base_screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
}


void RENDERER_process(SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        window_resized = true;
}


void RENDERER_update(void) {
    render_base_screen();
    render_post_processing();
    render_final_screen();
}
