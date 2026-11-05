#pragma once

// Paths of files embedded with `#embed`, relative to the project root. Tests
// use `tests/paths.h` instead, which replaces some of them with mocks.

// Graphics
#define GRAPHICS_BASE_VERT_SPIRV_PATH "res/shaders/base.vert.spv"
#define GRAPHICS_BASE_VERT_DXIL_PATH "res/shaders/base.vert.dxil"
#define GRAPHICS_BASE_FRAG_SPIRV_PATH "res/shaders/base.frag.spv"
#define GRAPHICS_BASE_FRAG_DXIL_PATH "res/shaders/base.frag.dxil"

// Renderer
#define RENDERER_TONEMAP_FRAG_SPIRV_PATH "res/shaders/tonemap.frag.spv"
#define RENDERER_TONEMAP_FRAG_DXIL_PATH "res/shaders/tonemap.frag.dxil"
#define RENDERER_BLOOM_PREFILTER_FRAG_SPIRV_PATH                               \
    "res/shaders/bloom-prefilter.frag.spv"
#define RENDERER_BLOOM_PREFILTER_FRAG_DXIL_PATH                                \
    "res/shaders/bloom-prefilter.frag.dxil"
#define RENDERER_BOX_SAMPLE_FRAG_SPIRV_PATH "res/shaders/box-sample.frag.spv"
#define RENDERER_BOX_SAMPLE_FRAG_DXIL_PATH "res/shaders/box-sample.frag.dxil"
#define RENDERER_BLOOM_CSV_PATH "data/graphics/bloom.csv"

// Spritesheet
#define SPRITESHEET_PATH "res/textures/spritesheet.png"

// Text
#define TEXT_UNIFONT_PATH "res/fonts/unifont.hex"
#define TEXT_ABLE_5_PATH "res/fonts/able-5.hex"
#define TEXT_GALMURI_9_PATH "res/fonts/galmuri9.hex"
#define TEXT_FUSION_JA_PATH "res/fonts/fusion-pixel-10px-ja.hex"
#define TEXT_FUSION_KO_PATH "res/fonts/fusion-pixel-10px-ko.hex"
#define TEXT_FUSION_ZH_HANS_PATH "res/fonts/fusion-pixel-10px-zh-hans.hex"
#define TEXT_FUSION_ZH_HANT_PATH "res/fonts/fusion-pixel-10px-zh-hant.hex"
#define TEXT_FUSION_ZH_HK_PATH "res/fonts/fusion-pixel-10px-zh-hk.hex"
#define TEXT_FUSION_ZH_TW_PATH "res/fonts/fusion-pixel-10px-zh-tw.hex"

// Chunks
#define CHUNKS_PLATFORMS_CSV_PATH "data/tiles/platforms.csv"
#define CHUNKS_TYPES_MAP_CSV_PATH "data/tiles/types-map.csv"
#define CHUNKS_001_PATH "data/chunks/001.json"
#define CHUNKS_002_PATH "data/chunks/002.json"
#define CHUNKS_003_PATH "data/chunks/003.json"
#define CHUNKS_004_PATH "data/chunks/004.json"

// Tilemap
#define TILEMAP_DIRT_AUTOTILE_PATH "editor/autotiles/dirt.json"
#define TILEMAP_STONE_AUTOTILE_PATH "editor/autotiles/stone.json"

// Fluids
#define FLUIDS_CSV_PATH "data/simulation/fluids.csv"
#define FLUIDS_PARTICLES_CSV_PATH "data/simulation/fluid-particles.csv"
#define FLUIDS_FRAG_SPIRV_PATH "res/shaders/fluids.frag.spv"
#define FLUIDS_FRAG_DXIL_PATH "res/shaders/fluids.frag.dxil"
#define FLUIDS_RIPPLES_FRAG_SPIRV_PATH "res/shaders/ripples.frag.spv"
#define FLUIDS_RIPPLES_FRAG_DXIL_PATH "res/shaders/ripples.frag.dxil"
#define FLUIDS_RIPPLES_CSV_PATH "data/graphics/ripples.csv"

// Particles
#define PARTICLES_CSV_PATH "data/graphics/particles.csv"
#define PARTICLES_SIMULATE_COMP_SPIRV_PATH                                     \
    "res/shaders/particles-simulate.comp.spv"
#define PARTICLES_SIMULATE_COMP_DXIL_PATH                                      \
    "res/shaders/particles-simulate.comp.dxil"
#define PARTICLES_SPAWN_COMP_SPIRV_PATH "res/shaders/particles-spawn.comp.spv"
#define PARTICLES_SPAWN_COMP_DXIL_PATH "res/shaders/particles-spawn.comp.dxil"
#define PARTICLES_COMPACT_COMP_SPIRV_PATH                                      \
    "res/shaders/particles-compact.comp.spv"
#define PARTICLES_COMPACT_COMP_DXIL_PATH                                       \
    "res/shaders/particles-compact.comp.dxil"
#define PARTICLES_VERT_SPIRV_PATH "res/shaders/particles.vert.spv"
#define PARTICLES_VERT_DXIL_PATH "res/shaders/particles.vert.dxil"
#define PARTICLES_FRAG_SPIRV_PATH "res/shaders/particles.frag.spv"
#define PARTICLES_FRAG_DXIL_PATH "res/shaders/particles.frag.dxil"
#define PARTICLES_SHADER_GLOW_SPIRV_PATH "res/shaders/particles-glow.frag.spv"
#define PARTICLES_SHADER_GLOW_DXIL_PATH "res/shaders/particles-glow.frag.dxil"

// Reactions
#define REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH                               \
    "data/simulation/reactions/fluid-fluid-into-tile.csv"

// Physics
#define PHYSICS_PROPS_CSV_PATH "data/graphics/props.csv"

// Player
#define PLAYER_BASE_CSV_PATH "data/player/core.csv"
#define PLAYER_PRESENTATION_CSV_PATH "data/player/presentation.csv"

// Camera
#define CAMERA_CSV_PATH "data/player/camera.csv"
