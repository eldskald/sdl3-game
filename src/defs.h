#pragma once

// Main settings
#define WINDOW_TITLE "Game"
#define WINDOW_SIZE_X 320
#define WINDOW_SIZE_Y 180

#define MAIN_T_TARGET_FPS 60
#define FIXED_T_TARGET_FPS 60
#define FLUIDS_T_TARGET_FPS 60

#define STARTING_WINDOW_SIZE_X 1280
#define STARTING_WINDOW_SIZE_Y 720
#define MIN_WINDOW_SIZE_X 640
#define MIN_WINDOW_SIZE_Y 360

// Helpers
#define EPSILON 0.0001f
#define COL_BLACK 0, 0, 0, 255
#define COL_WHITE 255, 255, 255, 255



// Renderer
#define RENDERER_TONEMAP_FRAG_SPIRV_PATH "res/shaders/tonemap.frag.spv"
#define RENDERER_TONEMAP_FRAG_DXIL_PATH "res/shaders/tonemap.frag.dxil"
#define RENDERER_BLOOM_PREFILTER_FRAG_SPIRV_PATH                               \
    "res/shaders/bloom-prefilter.frag.spv"
#define RENDERER_BLOOM_PREFILTER_FRAG_DXIL_PATH                                \
    "res/shaders/bloom-prefilter.frag.dxil"
#define RENDERER_TINT_FRAG_SPIRV_PATH "res/shaders/tint.frag.spv"
#define RENDERER_TINT_FRAG_DXIL_PATH "res/shaders/tint.frag.dxil"
#define RENDERER_BOX_SAMPLE_FRAG_SPIRV_PATH "res/shaders/box-sample.frag.spv"
#define RENDERER_BOX_SAMPLE_FRAG_DXIL_PATH "res/shaders/box-sample.frag.dxil"
#define RENDERER_BLOOM_CSV_PATH "data/graphics/bloom.csv"
#define RENDERER_BLOOM_MAX_ITERATIONS 8

// Spritesheet
#define SPRITESHEET_PATH "res/textures/spritesheet.png"
#define SPRITESHEET_X 256
#define SPRITESHEET_Y 256
#define SPRITESHEET_CELL_X 8
#define SPRITESHEET_CELL_Y 8

// Sprites
#define SPRITES_STARTING_DYNARR_CAP 1024
#define SPRITES_Z_INDEX_TOTAL 256 // z_index is Sint8, varies from -127 to 127
#define SPRITES_ANIM_MAX_KEYFRAMES 8



// Chunks
#define CHUNKS_PLATFORMS_CSV_PATH "data/tiles/platforms.csv"
#define CHUNKS_TYPES_MAP_CSV_PATH "data/tiles/types-map.csv"
#define CHUNKS_001_PATH "data/chunks/001.json"
#define CHUNKS_002_PATH "data/chunks/002.json"
#define CHUNKS_003_PATH "data/chunks/003.json"
#define CHUNKS_004_PATH "data/chunks/004.json"
#define CHUNKS_TOTAL 4



// Tilemap
#define TILEMAP_DIRT_AUTOTILE_PATH "editor/autotiles/dirt.json"
#define TILEMAP_STONE_AUTOTILE_PATH "editor/autotiles/stone.json"
#define TILEMAP_SIZE_X 1024
#define TILEMAP_SIZE_Y 1024
#define TILEMAP_TILE_TYPES tile_dirt = 1, tile_stone, tile_wood, tile_metal,
#define TILEMAP_AUTOMAP_NON_EMPTY_ID 2 // On Tiled's automap-tiles.tsx

// Fluids
#define FLUIDS_CSV_PATH "data/simulation/fluids.csv"
#define FLUIDS_PARTICLES_CSV_PATH "data/simulation/fluid-particles.csv"
#define FLUIDS_FRAG_SPIRV_PATH "res/shaders/fluids.frag.spv"
#define FLUIDS_FRAG_DXIL_PATH "res/shaders/fluids.frag.dxil"
#define FLUIDS_RIPPLES_FRAG_SPIRV_PATH "res/shaders/ripples.frag.spv"
#define FLUIDS_RIPPLES_FRAG_DXIL_PATH "res/shaders/ripples.frag.dxil"
#define FLUIDS_RIPPLES_CSV_PATH "data/graphics/ripples.csv"
#define FLUIDS_SPLIT_X 2
#define FLUIDS_SPLIT_Y 2
#define FLUIDS_SIMULATE_X 120
#define FLUIDS_SIMULATE_Y 66
#define FLUIDS_TYPES fluid_water, fluid_lava
#define FLUIDS_TOTAL 2
#define FLUIDS_CELL_W (SPRITESHEET_CELL_X / FLUIDS_SPLIT_X)
#define FLUIDS_CELL_H (SPRITESHEET_CELL_Y / FLUIDS_SPLIT_Y)
#define FLUIDS_RIPPLES_MAX_TICKS 4
#define FLUIDS_RIPPLES_TEXTURE_RES_FACTOR 2
#define FLUIDS_SHADER_IDX_STEP 8
#define FLUIDS_WALL_COLOR 255, 0, 0, 255
#define FLUIDS_STARTING_PARTICLES_DYNARR_CAP 1024
#define FLUIDS_SPLASH_SURFACE_RANGE 2
#define FLUIDS_STAGGER_SECONDS 10

// Reactions
#define REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH                               \
    "data/simulation/reactions/fluid-fluid-into-tile.csv"
#define REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD 0.5

// Physics
#define PHYSICS_STARTING_BODIES_DYNARR_CAP 1024
#define PHYSICS_SPLASH_COOLDOWN 0.2
#define PHYSICS_DROP_TIMER 0.1



// Player
#define PLAYER_BASE_CSV_PATH "data/player/core.csv"
#define PLAYER_SPRITE_CSV_PATH "data/player/presentation.csv"

// Camera
#define CAMERA_CSV_PATH "data/player/camera.csv"



// Tests
#define TESTS_CHUNKS_001_PATH "tests/mocks/chunks/001.json"
#define TESTS_TILEMAP_DIRT_AUTOTILE_PATH                                       \
    "tests/mocks/editor/autotiles/dirt.json"
#define TESTS_TILEMAP_STONE_AUTOTILE_PATH                                      \
    "tests/mocks/editor/autotiles/stone.json"
#define TESTS_CHUNKS_PLATFORMS_CSV_PATH "tests/mocks/tiles/platforms.csv"
#define TESTS_CHUNKS_TYPES_MAP_CSV_PATH "tests/mocks/tiles/types-map.csv"
#define TESTS_FLUIDS_RIPPLES_CSV_PATH "tests/mocks/graphics/ripples.csv"
#define TESTS_FLUIDS_CSV_PATH "tests/mocks/simulation/fluids.csv"
#define TESTS_FLUIDS_PARTICLES_CSV_PATH                                        \
    "tests/mocks/simulation/fluid-particles.csv"
#define TESTS_REACTIONS_FFIT_CSV_PATH                                          \
    "tests/mocks/simulation/reactions/fluid-fluid-into-tile.csv"
