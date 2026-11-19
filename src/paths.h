#pragma once
#include "../out/paths.h"

// clang-format off

// Graphics
#define GRAPHICS_BASE_VERT_SPIRV_PATH "shaders/base.vert.spv"
#define GRAPHICS_BASE_VERT_DXIL_PATH  "shaders/base.vert.dxil"
#define GRAPHICS_BASE_FRAG_SPIRV_PATH "shaders/base.frag.spv"
#define GRAPHICS_BASE_FRAG_DXIL_PATH  "shaders/base.frag.dxil"



// Renderer
#define RENDERER_TONEMAP_FRAG_SPIRV_PATH         "shaders/tonemap.frag.spv"
#define RENDERER_TONEMAP_FRAG_DXIL_PATH          "shaders/tonemap.frag.dxil"
#define RENDERER_BLOOM_PREFILTER_FRAG_SPIRV_PATH "shaders/bloom-prefilter.frag.spv"
#define RENDERER_BLOOM_PREFILTER_FRAG_DXIL_PATH  "shaders/bloom-prefilter.frag.dxil"
#define RENDERER_BOX_SAMPLE_FRAG_SPIRV_PATH      "shaders/box-sample.frag.spv"
#define RENDERER_BOX_SAMPLE_FRAG_DXIL_PATH       "shaders/box-sample.frag.dxil"
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
#define CHUNKS_PLATFORMS_CSV_PATH "data/simulation/platforms.csv"



// Tilemap
#define TILEMAP_TILES_CSV_PATH             "data/simulation/tiles.csv"
#define TILEMAP_DIRT_AUTOTILE_PATH         "editor/autotiles/dirt.json"
#define TILEMAP_STONE_AUTOTILE_PATH        "editor/autotiles/stone.json"
#define TILEMAP_STEP_ON_PARTICLES_CSV_PATH "data/graphics/step_on_particles.csv"
#define TILEMAP_LAND_ON_PARTICLES_CSV_PATH "data/graphics/land_on_particles.csv"
#define TILEMAP_AUDIO_CSV_PATH             "data/audio/tiles.csv"



// Fluids
#define FLUIDS_CSV_PATH           "data/simulation/fluids.csv"
#define FLUIDS_PARTICLES_CSV_PATH "data/simulation/fluid-particles.csv"
#define FLUIDS_RIPPLES_CSV_PATH   "data/graphics/ripples.csv"
#define FLUIDS_AUDIO_CSV_PATH     "data/audio/fluids.csv"

#define FLUIDS_FRAG_SPIRV_PATH          "shaders/fluids.frag.spv"
#define FLUIDS_FRAG_DXIL_PATH           "shaders/fluids.frag.dxil"
#define FLUIDS_RIPPLES_FRAG_SPIRV_PATH  "shaders/ripples.frag.spv"
#define FLUIDS_RIPPLES_FRAG_DXIL_PATH   "shaders/ripples.frag.dxil"
#define FLUIDS_SURFACES_COMP_SPIRV_PATH "shaders/fluids-surfaces.comp.spv"
#define FLUIDS_SURFACES_COMP_DXIL_PATH  "shaders/fluids-surfaces.comp.dxil"



// Particles
#define PARTICLES_CSV_PATH "data/graphics/particles.csv"

#define PARTICLES_SIMULATE_COMP_SPIRV_PATH "shaders/particles-simulate.comp.spv"
#define PARTICLES_SIMULATE_COMP_DXIL_PATH  "shaders/particles-simulate.comp.dxil"
#define PARTICLES_SPAWN_COMP_SPIRV_PATH    "shaders/particles-spawn.comp.spv"
#define PARTICLES_SPAWN_COMP_DXIL_PATH     "shaders/particles-spawn.comp.dxil"
#define PARTICLES_COMPACT_COMP_SPIRV_PATH  "shaders/particles-compact.comp.spv"
#define PARTICLES_COMPACT_COMP_DXIL_PATH   "shaders/particles-compact.comp.dxil"
#define PARTICLES_VERT_SPIRV_PATH          "shaders/particles.vert.spv"
#define PARTICLES_VERT_DXIL_PATH           "shaders/particles.vert.dxil"
#define PARTICLES_FRAG_SPIRV_PATH          "shaders/particles.frag.spv"
#define PARTICLES_FRAG_DXIL_PATH           "shaders/particles.frag.dxil"
#define PARTICLES_SHADER_FLAMES_SPIRV_PATH "shaders/particles-flames.frag.spv"
#define PARTICLES_SHADER_FLAMES_DXIL_PATH  "shaders/particles-flames.frag.dxil"



// Reactions
#define REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH "data/simulation/reactions/fluid-fluid-into-tile.csv"
#define REACTIONS_AUDIO_CSV_PATH                 "data/audio/reactions.csv"

#define REACTIONS_WATER_LAVA_STONE_1_SFX_PATH "res/sfx/fluids/lava/steam-burst.wav"



// Physics
#define PHYSICS_PROPS_CSV_PATH "data/graphics/props.csv"



// Props
#define PROPS_FLAMES_CSV_PATH "data/graphics/flames.csv"



// Player
#define PLAYER_BASE_CSV_PATH         "data/player/core.csv"
#define PLAYER_PRESENTATION_CSV_PATH "data/player/presentation.csv"



// Entities
#define ENTITY_CLASSES_CSV_PATH "data/entities/classes.csv"
#define FROG_CSV_PATH "data/entities/frog.csv"
#define SPIDER_CSV_PATH "data/entities/spider.csv"
#define WISP_CSV_PATH "data/entities/wisp.csv"



// Camera
#define CAMERA_CSV_PATH "data/player/camera.csv"



// Background sounds
#define BGS_TRACKS_CSV_PATH "data/audio/tracks.csv"

// clang-format on
