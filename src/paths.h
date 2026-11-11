#pragma once

// clang-format off

// Graphics
#define GRAPHICS_BASE_VERT_SPIRV_PATH "res/shaders/base.vert.spv"
#define GRAPHICS_BASE_VERT_DXIL_PATH  "res/shaders/base.vert.dxil"
#define GRAPHICS_BASE_FRAG_SPIRV_PATH "res/shaders/base.frag.spv"
#define GRAPHICS_BASE_FRAG_DXIL_PATH  "res/shaders/base.frag.dxil"



// Renderer
#define RENDERER_TONEMAP_FRAG_SPIRV_PATH         "res/shaders/tonemap.frag.spv"
#define RENDERER_TONEMAP_FRAG_DXIL_PATH          "res/shaders/tonemap.frag.dxil"
#define RENDERER_BLOOM_PREFILTER_FRAG_SPIRV_PATH "res/shaders/bloom-prefilter.frag.spv"
#define RENDERER_BLOOM_PREFILTER_FRAG_DXIL_PATH  "res/shaders/bloom-prefilter.frag.dxil"
#define RENDERER_BOX_SAMPLE_FRAG_SPIRV_PATH      "res/shaders/box-sample.frag.spv"
#define RENDERER_BOX_SAMPLE_FRAG_DXIL_PATH       "res/shaders/box-sample.frag.dxil"
#define RENDERER_BLOOM_CSV_PATH                  "data/graphics/bloom.csv"



// Spritesheet
#define SPRITESHEET_PATH "res/textures/spritesheet.png"



// Text
#define TEXT_UNIFONT_PATH        "res/fonts/unifont.hex"
#define TEXT_ABLE_5_PATH         "res/fonts/able-5.hex"
#define TEXT_GALMURI_9_PATH      "res/fonts/galmuri9.hex"
#define TEXT_FUSION_JA_PATH      "res/fonts/fusion-pixel-10px-ja.hex"
#define TEXT_FUSION_KO_PATH      "res/fonts/fusion-pixel-10px-ko.hex"
#define TEXT_FUSION_ZH_HANS_PATH "res/fonts/fusion-pixel-10px-zh-hans.hex"
#define TEXT_FUSION_ZH_HANT_PATH "res/fonts/fusion-pixel-10px-zh-hant.hex"
#define TEXT_FUSION_ZH_HK_PATH   "res/fonts/fusion-pixel-10px-zh-hk.hex"
#define TEXT_FUSION_ZH_TW_PATH   "res/fonts/fusion-pixel-10px-zh-tw.hex"



// Chunks
#define CHUNKS_PLATFORMS_CSV_PATH "data/tiles/platforms.csv"
#define CHUNKS_TYPES_MAP_CSV_PATH "data/tiles/types-map.csv"

#define CHUNKS_001_PATH "data/chunks/001.json"
#define CHUNKS_002_PATH "data/chunks/002.json"
#define CHUNKS_003_PATH "data/chunks/003.json"
#define CHUNKS_004_PATH "data/chunks/004.json"
#define CHUNKS_005_PATH "data/chunks/005.json"



// Tilemap
#define TILEMAP_DIRT_AUTOTILE_PATH         "editor/autotiles/dirt.json"
#define TILEMAP_STONE_AUTOTILE_PATH        "editor/autotiles/stone.json"
#define TILEMAP_STEP_ON_PARTICLES_CSV_PATH "data/graphics/step_on_particles.csv"
#define TILEMAP_LAND_ON_PARTICLES_CSV_PATH "data/graphics/land_on_particles.csv"
#define TILEMAP_AUDIO_CSV_PATH             "data/audio/tiles.csv"

#define TILEMAP_DIRT_LAND_1_SFX_PATH "res/sfx/footsteps/dirt/Footsteps_DirtyGround_Land/Footsteps_DirtyGround_Jump_Land_01.wav"
#define TILEMAP_DIRT_LAND_2_SFX_PATH "res/sfx/footsteps/dirt/Footsteps_DirtyGround_Land/Footsteps_DirtyGround_Jump_Land_02.wav"

#define TILEMAP_STONE_LAND_1_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Jump/Footsteps_Rock_Jump_Land_01.wav"
#define TILEMAP_STONE_LAND_2_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Jump/Footsteps_Rock_Jump_Land_02.wav"
#define TILEMAP_STONE_LAND_3_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Jump/Footsteps_Rock_Jump_Land_03.wav"

#define TILEMAP_WOOD_LAND_1_SFX_PATH "res/sfx/footsteps/wood/Footsteps_Wood_Jump/Footsteps_Wood_Jump_Land_01.wav"
#define TILEMAP_WOOD_LAND_2_SFX_PATH "res/sfx/footsteps/wood/Footsteps_Wood_Jump/Footsteps_Wood_Jump_Land_02.wav"

#define TILEMAP_METAL_LAND_1_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Jump/Footsteps_MetalV1_Jump_Land_01.wav"
#define TILEMAP_METAL_LAND_2_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Jump/Footsteps_MetalV1_Jump_Land_02.wav"
#define TILEMAP_METAL_LAND_3_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Jump/Footsteps_MetalV1_Jump_Land_03.wav"

#define TILEMAP_WATER_LAND_1_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Jump/Footsteps_Water_Jump_Light_01.wav"
#define TILEMAP_WATER_LAND_2_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Jump/Footsteps_Water_Jump_Light_02.wav"
#define TILEMAP_WATER_LAND_3_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Jump/Footsteps_Water_Jump_Light_03.wav"

#define TILEMAP_SLUDGE_LAND_1_SFX_PATH "res/sfx/footsteps/sludge/slime-hit-1.wav"
#define TILEMAP_SLUDGE_LAND_2_SFX_PATH "res/sfx/footsteps/sludge/slime-hit-2.wav"

#define TILEMAP_DIRT_STEP_1_SFX_PATH "res/sfx/footsteps/dirt/Footsteps_DirtyGround_Run/Footsteps_DirtyGround_Run_01.wav"
#define TILEMAP_DIRT_STEP_2_SFX_PATH "res/sfx/footsteps/dirt/Footsteps_DirtyGround_Run/Footsteps_DirtyGround_Run_02.wav"
#define TILEMAP_DIRT_STEP_3_SFX_PATH "res/sfx/footsteps/dirt/Footsteps_DirtyGround_Run/Footsteps_DirtyGround_Run_03.wav"
#define TILEMAP_DIRT_STEP_4_SFX_PATH "res/sfx/footsteps/dirt/Footsteps_DirtyGround_Run/Footsteps_DirtyGround_Run_04.wav"
#define TILEMAP_DIRT_STEP_5_SFX_PATH "res/sfx/footsteps/dirt/Footsteps_DirtyGround_Run/Footsteps_DirtyGround_Run_05.wav"

#define TILEMAP_STONE_STEP_1_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Run/Footsteps_Rock_Run_01.wav"
#define TILEMAP_STONE_STEP_2_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Run/Footsteps_Rock_Run_02.wav"
#define TILEMAP_STONE_STEP_3_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Run/Footsteps_Rock_Run_03.wav"
#define TILEMAP_STONE_STEP_4_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Run/Footsteps_Rock_Run_04.wav"
#define TILEMAP_STONE_STEP_5_SFX_PATH "res/sfx/footsteps/stone/Footsteps_Rock_Run/Footsteps_Rock_Run_05.wav"

#define TILEMAP_WOOD_STEP_1_SFX_PATH "res/sfx/footsteps/wood/Footsteps_Wood_Run/Footsteps_Wood_Run_01.wav"
#define TILEMAP_WOOD_STEP_2_SFX_PATH "res/sfx/footsteps/wood/Footsteps_Wood_Run/Footsteps_Wood_Run_02.wav"
#define TILEMAP_WOOD_STEP_3_SFX_PATH "res/sfx/footsteps/wood/Footsteps_Wood_Run/Footsteps_Wood_Run_03.wav"
#define TILEMAP_WOOD_STEP_4_SFX_PATH "res/sfx/footsteps/wood/Footsteps_Wood_Run/Footsteps_Wood_Run_04.wav"
#define TILEMAP_WOOD_STEP_5_SFX_PATH "res/sfx/footsteps/wood/Footsteps_Wood_Run/Footsteps_Wood_Run_05.wav"

#define TILEMAP_METAL_STEP_1_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Run/Footsteps_MetalV1_Run_01.wav"
#define TILEMAP_METAL_STEP_2_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Run/Footsteps_MetalV1_Run_02.wav"
#define TILEMAP_METAL_STEP_3_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Run/Footsteps_MetalV1_Run_03.wav"
#define TILEMAP_METAL_STEP_4_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Run/Footsteps_MetalV1_Run_04.wav"
#define TILEMAP_METAL_STEP_5_SFX_PATH "res/sfx/footsteps/metal/Footsteps_Metal_Run/Footsteps_MetalV1_Run_05.wav"

#define TILEMAP_WATER_STEP_1_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Walk/Footsteps_WaterV1_Walk_01.wav"
#define TILEMAP_WATER_STEP_2_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Walk/Footsteps_WaterV1_Walk_02.wav"
#define TILEMAP_WATER_STEP_3_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Walk/Footsteps_WaterV1_Walk_03.wav"
#define TILEMAP_WATER_STEP_4_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Walk/Footsteps_WaterV1_Walk_04.wav"
#define TILEMAP_WATER_STEP_5_SFX_PATH "res/sfx/footsteps/water/Footsteps_Water_Walk/Footsteps_WaterV1_Walk_05.wav"

#define TILEMAP_SLUDGE_STEP_1_SFX_PATH "res/sfx/footsteps/sludge/sludge-step-1.wav"
#define TILEMAP_SLUDGE_STEP_2_SFX_PATH "res/sfx/footsteps/sludge/sludge-step-2.wav"



// Fluids
#define FLUIDS_CSV_PATH           "data/simulation/fluids.csv"
#define FLUIDS_PARTICLES_CSV_PATH "data/simulation/fluid-particles.csv"
#define FLUIDS_RIPPLES_CSV_PATH   "data/graphics/ripples.csv"
#define FLUIDS_AUDIO_CSV_PATH     "data/audio/fluids.csv"

#define FLUIDS_FRAG_SPIRV_PATH         "res/shaders/fluids.frag.spv"
#define FLUIDS_FRAG_DXIL_PATH          "res/shaders/fluids.frag.dxil"
#define FLUIDS_RIPPLES_FRAG_SPIRV_PATH "res/shaders/ripples.frag.spv"
#define FLUIDS_RIPPLES_FRAG_DXIL_PATH  "res/shaders/ripples.frag.dxil"
#define FLUIDS_SURFACES_COMP_SPIRV_PATH "res/shaders/fluids-surfaces.comp.spv"
#define FLUIDS_SURFACES_COMP_DXIL_PATH  "res/shaders/fluids-surfaces.comp.dxil"

#define FLUIDS_WATER_SPLASH_1_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Jump_Big_01.wav"
#define FLUIDS_WATER_SPLASH_2_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Jump_Big_02.wav"
#define FLUIDS_WATER_SPLASH_3_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Jump_Big_03.wav"

#define FLUIDS_LAVA_SPLASH_1_SFX_PATH "res/sfx/fluids/lava/sizzler.wav"

#define FLUIDS_ACID_SPLASH_1_SFX_PATH "res/sfx/fluids/acid/shovel-sizzle.wav"

#define FLUIDS_SLUDGE_SPLASH_1_SFX_PATH "res/sfx/fluids/sludge/splat-1.wav"
#define FLUIDS_SLUDGE_SPLASH_2_SFX_PATH "res/sfx/fluids/sludge/splat-2.wav"

#define FLUIDS_WATER_SLOSH_1_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Run_01.wav"
#define FLUIDS_WATER_SLOSH_2_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Run_02.wav"
#define FLUIDS_WATER_SLOSH_3_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Run_03.wav"
#define FLUIDS_WATER_SLOSH_4_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Run_04.wav"
#define FLUIDS_WATER_SLOSH_5_SFX_PATH "res/sfx/fluids/water/Footsteps_Water_Run_05.wav"



// Particles
#define PARTICLES_CSV_PATH "data/graphics/particles.csv"

#define PARTICLES_SIMULATE_COMP_SPIRV_PATH "res/shaders/particles-simulate.comp.spv"
#define PARTICLES_SIMULATE_COMP_DXIL_PATH  "res/shaders/particles-simulate.comp.dxil"
#define PARTICLES_SPAWN_COMP_SPIRV_PATH    "res/shaders/particles-spawn.comp.spv"
#define PARTICLES_SPAWN_COMP_DXIL_PATH     "res/shaders/particles-spawn.comp.dxil"
#define PARTICLES_COMPACT_COMP_SPIRV_PATH  "res/shaders/particles-compact.comp.spv"
#define PARTICLES_COMPACT_COMP_DXIL_PATH   "res/shaders/particles-compact.comp.dxil"
#define PARTICLES_VERT_SPIRV_PATH          "res/shaders/particles.vert.spv"
#define PARTICLES_VERT_DXIL_PATH           "res/shaders/particles.vert.dxil"
#define PARTICLES_FRAG_SPIRV_PATH          "res/shaders/particles.frag.spv"
#define PARTICLES_FRAG_DXIL_PATH           "res/shaders/particles.frag.dxil"
#define PARTICLES_SHADER_GLOW_SPIRV_PATH   "res/shaders/particles-glow.frag.spv"
#define PARTICLES_SHADER_GLOW_DXIL_PATH    "res/shaders/particles-glow.frag.dxil"



// Reactions
#define REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH "data/simulation/reactions/fluid-fluid-into-tile.csv"
#define REACTIONS_AUDIO_CSV_PATH "data/audio/reactions.csv"

#define REACTIONS_WATER_LAVA_STONE_1_SFX_PATH "res/sfx/fluids/lava/steam-burst.wav"



// Physics
#define PHYSICS_PROPS_CSV_PATH "data/graphics/props.csv"



// Player
#define PLAYER_BASE_CSV_PATH         "data/player/core.csv"
#define PLAYER_PRESENTATION_CSV_PATH "data/player/presentation.csv"



// Camera
#define CAMERA_CSV_PATH "data/player/camera.csv"



// Background sounds
#define BGS_TRACKS_CSV_PATH "data/audio/tracks.csv"

#define BGS_CAVE_PATH "res/bgs/Cave/Cave.ogg"
#define BGS_WATER_STREAMING_PATH "res/bgs/fluids/water-streaming-loop.ogg"
#define BGS_LAVA_BUBBLING_PATH "res/bgs/fluids/lava-bubbling-loop.ogg"
#define BGS_ACID_FIZZ_PATH "res/bgs/fluids/acid-fizz.ogg"
#define BGS_SLUDGE_BUBBLING_PATH "res/bgs/fluids/sludge-looped.ogg"

// clang-format on
