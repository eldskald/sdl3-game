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



// Renderer
#define SPRITESHEET_PATH "res/textures/spritesheet.png"
#define TONEMAP_FRAG_SPIRV_PATH "res/shaders/tonemap.frag.spv"
#define TONEMAP_FRAG_DXIL_PATH "res/shaders/tonemap.frag.dxil"
#define BLOOM_PREFILTER_FRAG_SPIRV_PATH "res/shaders/bloom-prefilter.frag.spv"
#define BLOOM_PREFILTER_FRAG_DXIL_PATH "res/shaders/bloom-prefilter.frag.dxil"
#define TINT_FRAG_SPIRV_PATH "res/shaders/tint.frag.spv"
#define TINT_FRAG_DXIL_PATH "res/shaders/tint.frag.dxil"
#define BOX_SAMPLE_FRAG_SPIRV_PATH "res/shaders/box-sample.frag.spv"
#define BOX_SAMPLE_FRAG_DXIL_PATH "res/shaders/box-sample.frag.dxil"
#define BLOOM_CSV_PATH "data/graphics/bloom.csv"
#define BLOOM_MAX_ITERATIONS 8

#define SPRITESHEET_X 256
#define SPRITESHEET_Y 256
#define SPRITESHEET_CELL_X 8
#define SPRITESHEET_CELL_Y 8

#define SPRITESHEET_DIRT_MIN_X 0
#define SPRITESHEET_DIRT_MAX_X 3
#define SPRITESHEET_STONE_MIN_X 4
#define SPRITESHEET_STONE_MAX_X 7
#define SPRITESHEET_WOOD_MIN_X 8
#define SPRITESHEET_WOOD_MAX_X 11
#define SPRITESHEET_METAL_MIN_X 12
#define SPRITESHEET_METAL_MAX_X 15
#define SPRITESHEET_AUTOTILE_MAX_Y 11

#define SPRITESHEET_STONE_PLAT_X0 4
#define SPRITESHEET_STONE_PLAT_X 7
#define SPRITESHEET_STONE_PLAT_Y0 12
#define SPRITESHEET_STONE_PLAT_Y 13
#define SPRITESHEET_WOOD_PLAT_X0 8
#define SPRITESHEET_WOOD_PLAT_X 9
#define SPRITESHEET_WOOD_PLAT_Y0 5
#define SPRITESHEET_WOOD_PLAT_Y 6

#define COL_BLACK 0, 0, 0, 255
#define COL_WHITE 255, 255, 255, 255

// Sprites
#define STARTING_SPRITES_DYNARR_CAP 1024
#define Z_INDEX_TOTAL 256 // z_index is Sint8, varies from -127 to 127
#define SPRITES_ANIM_MAX_KEYFRAMES 8



// Chunks
#define CHUNK_MEM_BUFFER_SIZE 1048576 // 2^20
#define CHUNKS_TOTAL 4
#define CHUNK_001_PATH "data/chunks/001.json"
#define CHUNK_002_PATH "data/chunks/002.json"
#define CHUNK_003_PATH "data/chunks/003.json"
#define CHUNK_004_PATH "data/chunks/004.json"



// Tilemap
#define TILEMAP_SIZE_X 1024
#define TILEMAP_SIZE_Y 1024
#define TILES_TYPES tile_dirt = 1, tile_stone, tile_wood, tile_metal,

// Fluids
#define FLUIDS_SPLIT_X 2
#define FLUIDS_SPLIT_Y 2
#define FLUIDS_SIMULATE_X 120
#define FLUIDS_SIMULATE_Y 66
#define FLUIDS_TYPES fluid_water, fluid_lava
#define FLUIDS_TOTAL 2
#define FLUIDS_CELL_W (SPRITESHEET_CELL_X / FLUIDS_SPLIT_X)
#define FLUIDS_CELL_H (SPRITESHEET_CELL_Y / FLUIDS_SPLIT_Y)
#define FLUIDS_CSV_PATH "data/simulation/fluids.csv"
#define FLUID_PARTICLES_CSV_PATH "data/simulation/fluid-particles.csv"
#define FLUIDS_FRAG_SPIRV_PATH "res/shaders/fluids.frag.spv"
#define FLUIDS_FRAG_DXIL_PATH "res/shaders/fluids.frag.dxil"
#define FLUIDS_RIPPLES_FRAG_SPIRV_PATH "res/shaders/ripples.frag.spv"
#define FLUIDS_RIPPLES_FRAG_DXIL_PATH "res/shaders/ripples.frag.dxil"
#define FLUIDS_RIPPLE_STRENGTH 1.0f
#define FLUIDS_RIPPLE_MAX_SPEED 60
#define FLUIDS_RIPPLE_SIZE 2
#define FLUIDS_RIPPLES_MAX_TICKS 4
#define FLUIDS_SHADER_IDX_STEP 8
#define FLUIDS_WALL_COLOR 255, 0, 0, 255
#define STARTING_FLUIDS_PARTICLES_DYNARR_CAP 1024
#define FLUID_SPLASH_SURFACE_RANGE 2
#define FLUIDS_STAGGER_SECONDS 10

// Reactions
#define REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD 0.5
#define REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH                               \
    "data/simulation/reactions/fluid-fluid-into-tile.csv"

// Physics
#define STARTING_BODIES_DYNARR_CAP 1024
#define PHYSICS_SPLASH_COOLDOWN 0.2
#define PHYSICS_DROP_TIMER 0.1



// Player
#define PLAYER_BASE_CSV_PATH "data/player/core.csv"
#define PLAYER_SPRITE_CSV_PATH "data/player/presentation.csv"



// Helpers
#define EPSILON 0.0001f
