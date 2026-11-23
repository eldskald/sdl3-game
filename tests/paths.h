#pragma once

// Replaces `src/paths.h` on tests, since `-Itests` comes before `-Isrc`. It's
// the same paths, but the files the tests check against point to their mocks.
#include "../out/paths.h"

#undef CHUNKS_PLATFORMS_CSV_PATH
#undef CHUNKS_001_PATH
#undef TILEMAP_DIRT_AUTOTILE_PATH
#undef TILEMAP_STONE_AUTOTILE_PATH
#undef TILEMAP_TILES_CSV_PATH
#undef FLUIDS_CSV_PATH
#undef FLUIDS_PARTICLES_CSV_PATH
#undef FLUIDS_RIPPLES_CSV_PATH
#undef REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH
#undef PHYSICS_PROPS_CSV_PATH
#undef TEXT_ABLE_5_PATH
#undef BGS_TRACKS_CSV_PATH
#undef BGS_CAVE_PATH
#undef BGS_WATER_STREAMING_PATH
#undef BGS_LAVA_BUBBLING_PATH

// Chunks
#define CHUNKS_PLATFORMS_CSV_PATH "tests/mocks/simulation/platforms.csv"
#define TILEMAP_TILES_CSV_PATH "tests/mocks/simulation/tiles.csv"
#define CHUNKS_001_PATH "tests/mocks/chunks/001.json"

// Tilemap
#define TILEMAP_DIRT_AUTOTILE_PATH "tests/mocks/editor/autotiles/dirt.json"
#define TILEMAP_STONE_AUTOTILE_PATH "tests/mocks/editor/autotiles/stone.json"

// Fluids
#define FLUIDS_CSV_PATH "tests/mocks/simulation/fluids.csv"
#define FLUIDS_PARTICLES_CSV_PATH "tests/mocks/simulation/fluid-particles.csv"
#define FLUIDS_RIPPLES_CSV_PATH "tests/mocks/graphics/ripples.csv"

// Reactions
#define REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH                               \
    "tests/mocks/simulation/reactions/fluid-fluid-into-tile.csv"

// Physics
#define PHYSICS_PROPS_CSV_PATH "tests/mocks/graphics/props.csv"

// Text
#define TEXT_ABLE_5_PATH "tests/mocks/fonts/mock.hex"

// BGS
#define BGS_TRACKS_CSV_PATH "tests/mocks/audio/tracks.csv"
#define BGS_CAVE_PATH "tests/mocks/audio/track.ogg"
#define BGS_WATER_RUNNING_PATH BGS_CAVE_PATH
#define BGS_LAVA_STATIC_PATH BGS_CAVE_PATH

// Graphics
#define TEST_GRAPHICS_RECTS_VERT_SPIRV_PATH "tests/mocks/shaders/rects.vert.spv"
#define TEST_GRAPHICS_RECTS_VERT_DXIL_PATH "tests/mocks/shaders/rects.vert.dxil"
#define TEST_GRAPHICS_MASK_FRAG_SPIRV_PATH "tests/mocks/shaders/mask.frag.spv"
#define TEST_GRAPHICS_MASK_FRAG_DXIL_PATH "tests/mocks/shaders/mask.frag.dxil"
#define TEST_GRAPHICS_RECTS_COMP_SPIRV_PATH "tests/mocks/shaders/rects.comp.spv"
#define TEST_GRAPHICS_RECTS_COMP_DXIL_PATH "tests/mocks/shaders/rects.comp.dxil"
