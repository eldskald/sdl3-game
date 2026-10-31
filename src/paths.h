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
