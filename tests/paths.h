#pragma once

// Replaces `src/paths.h` on tests, since `-Itests` comes before `-Isrc`. It's
// the same paths, but the files the tests check against point to their mocks.
#include "../src/paths.h"

#undef CHUNKS_PLATFORMS_CSV_PATH
#undef CHUNKS_TYPES_MAP_CSV_PATH
#undef CHUNKS_001_PATH
#undef CHUNKS_002_PATH
#undef CHUNKS_003_PATH
#undef CHUNKS_004_PATH
#undef TILEMAP_DIRT_AUTOTILE_PATH
#undef TILEMAP_STONE_AUTOTILE_PATH
#undef FLUIDS_CSV_PATH
#undef FLUIDS_PARTICLES_CSV_PATH
#undef FLUIDS_RIPPLES_CSV_PATH
#undef REACTIONS_FLUID_FLUID_INTO_TILE_CSV_PATH
#undef PHYSICS_PROPS_CSV_PATH

// Chunks
#define CHUNKS_PLATFORMS_CSV_PATH "tests/mocks/tiles/platforms.csv"
#define CHUNKS_TYPES_MAP_CSV_PATH "tests/mocks/tiles/types-map.csv"
// There's a single mocked chunk, so every chunk is it.
#define CHUNKS_001_PATH "tests/mocks/chunks/001.json"
#define CHUNKS_002_PATH CHUNKS_001_PATH
#define CHUNKS_003_PATH CHUNKS_001_PATH
#define CHUNKS_004_PATH CHUNKS_001_PATH

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
