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

#define SIMULATION_SPACE_X 120
#define SIMULATION_SPACE_Y 66

// Localization
// The languages the game can be in, by their ISO 639-1 code, with Chinese also
// by script or region, since each has its own font with its own glyph shapes.
// `STARTING_LANGUAGE` is the one it starts in, set it with `make GAME_LANG=pt`,
// see the Makefile. Changing it while running is `TEXT_set_language()`.
typedef enum {
    language_en,      // English
    language_pt,      // Portuguese
    language_es,      // Spanish
    language_fr,      // French
    language_de,      // German
    language_it,      // Italian
    language_nl,      // Dutch
    language_sv,      // Swedish
    language_da,      // Danish
    language_nb,      // Norwegian Bokmål
    language_fi,      // Finnish
    language_pl,      // Polish
    language_cs,      // Czech
    language_hu,      // Hungarian
    language_ro,      // Romanian
    language_tr,      // Turkish
    language_id,      // Indonesian
    language_ms,      // Malay
    language_tl,      // Tagalog, Filipino
    language_el,      // Greek
    language_ru,      // Russian
    language_uk,      // Ukrainian
    language_vi,      // Vietnamese
    language_ja,      // Japanese
    language_ko,      // Korean
    language_zh_hans, // Chinese, Simplified
    language_zh_hant, // Chinese, Traditional
    language_zh_hk,   // Chinese, Traditional, Hong Kong
    language_zh_tw,   // Chinese, Traditional, Taiwan
} language;
#define LANGUAGES_TOTAL 29
#ifndef STARTING_LANGUAGE
#define STARTING_LANGUAGE language_en
#endif

// Helpers
#define EPSILON 0.0001f
#define COL_BLACK 0, 0, 0, 255
#define COL_WHITE 255, 255, 255, 255



// Graphics
#define GRAPHICS_MAX_EXTRA_SAMPLERS 3
#define GRAPHICS_MAX_UNIFORMS_SIZE 64
#define GRAPHICS_MAX_PIPELINES 32
#define GRAPHICS_VERTICES_STARTING_CAP 4096
#define GRAPHICS_COMMANDS_STARTING_CAP 64
#define GRAPHICS_UPLOADS_STARTING_CAP 16
#define GRAPHICS_STAGING_STARTING_CAP 4096
#define GRAPHICS_MAX_STORAGE_BUFFERS 4

// Renderer
#define RENDERER_BLOOM_MAX_ITERATIONS 8
#define RENDERER_Z_INDEX_TOTAL 256 // z_index is Sint8, varies from -128 to 127

// Spritesheet
#define SPRITESHEET_X 256
#define SPRITESHEET_Y 256
#define SPRITESHEET_CELL_X 8
#define SPRITESHEET_CELL_Y 8

// Text
// `font_main` is drawn with a font depending on the language, see `text.c`.
// Unifont is only for debugging, it's too big for the game's resolution.
#if DEV
#define TEXT_FONT_TYPES font_main, font_unifont
#define TEXT_FONTS_TOTAL 2
#else
#define TEXT_FONT_TYPES font_main
#define TEXT_FONTS_TOTAL 1
#endif
#define TEXT_ATLAS_X 2048 // Its height is however much the glyphs need
#define TEXT_FALLBACK_CODEPOINT 0xFFFD // Replacement character, '�'

// Sprites
#define SPRITES_STARTING_DYNARR_CAP 1024
#define SPRITES_ANIM_MAX_KEYFRAMES 8
#define SPRITES_FRAME_STARTING_CAP 1024
#define SPRITES_FRAME_STARTING_SHAPES_CAP 1024
#define SPRITES_FRAME_CULL_MARGIN 16



// Audio
#define AUDIO_STARTING_VOLUME 1.0f // Of every bus

// SFX
#define SFX_STARTING_POOL_CAP 64

// BGS
#define BGS_TRACKS                                                             \
    bgs_track_cave, bgs_track_water_streaming, bgs_track_lava_bubbling,
#define BGS_TRACKS_TOTAL 4
#define BGS_BUFFER_MS 500   // How far ahead of what's heard tracks are decoded
#define BGS_MAX_FILL_MS 100 // Most a track decodes per tick to get there
#define BGS_DECODE_CHUNK_SAMPLES 4096 // Floats decoded at a time



// Chunks
#define CHUNKS_TOTAL 5
#define CHUNKS_HELPER_TILESET_NAME "helper-tiles.tsx"
#define CHUNKS_HELPER_TILES_TOTAL 64



// Tilemap
#define TILEMAP_SIZE_X 1024
#define TILEMAP_SIZE_Y 1024
#define TILEMAP_TILE_TYPES tile_dirt = 1, tile_stone, tile_wood, tile_metal,
#define TILEMAP_TILE_TYPES_TOTAL 5     // Counting the empty tile
#define TILEMAP_AUTOMAP_NON_EMPTY_ID 2 // On Tiled's automap-tiles.tsx
#define TILEMAP_Z_INDEX 20
#define TILEMAP_PLATFORMS_Z_INDEX (-20)

// Fluids
#define FLUIDS_SPLIT_X 2
#define FLUIDS_SPLIT_Y 2
#define FLUIDS_TYPES fluid_water, fluid_lava
#define FLUIDS_TOTAL 2
#define FLUIDS_CELL_W (SPRITESHEET_CELL_X / FLUIDS_SPLIT_X)
#define FLUIDS_CELL_H (SPRITESHEET_CELL_Y / FLUIDS_SPLIT_Y)
#define FLUIDS_RIPPLES_MAX_TICKS 4
#define FLUIDS_RIPPLES_TEXTURE_RES_FACTOR 2
#define FLUIDS_SHADER_IDX_STEP 8
#define FLUIDS_WALL_COLOR 255, 0, 0, 255
#define FLUIDS_STARTING_PARTICLES_DYNARR_CAP 1024
#define FLUIDS_STARTING_BODIES_POOL_CAP 32
#define FLUIDS_SPLASH_SURFACE_RANGE 2
#define FLUIDS_STAGGER_SECONDS 10
#define FLUIDS_INTERACTION_VOL_THRESHOLD 0.5f
#define FLUIDS_RIPPLE_SFX_VOL_THRESHOLD 0.5
#define FLUIDS_BODIES_BGS_FADE_RATE 1.0f
#define FLUIDS_Z_INDEX 10

// Particles
#define PARTICLES_TYPES                                                        \
    particle_tile_dirt, particle_tile_stone, particle_tile_wood,               \
        particle_tile_metal
#define PARTICLES_TOTAL 5
#define PARTICLES_SPLIT_X 2
#define PARTICLES_SPLIT_Y 2
#define PARTICLES_CELL_W (SPRITESHEET_CELL_X / PARTICLES_SPLIT_X)
#define PARTICLES_CELL_H (SPRITESHEET_CELL_Y / PARTICLES_SPLIT_Y)
#define PARTICLES_CAPACITY 16384
#define PARTICLES_MAX_REQUESTS 1024
#define PARTICLES_THREADS 64
#define PARTICLES_STARTING_REQUESTS_DYNARR_CAP 64
#define PARTICLES_Z_INDEX 15

// Reactions
#define REACTIONS_FLUID_FLUID_INTO_TILE_VOL_THRESHOLD 0.5

// Physics
#define PHYSICS_STARTING_BODIES_DYNARR_CAP 1024
#define PHYSICS_STARTING_PROP_POINTS_DYNARR_CAP 1024
#define PHYSICS_STARTING_PROP_CONNECTIONS_DYNARR_CAP 1024
#define PHYSICS_SPLASH_COOLDOWN 0.2
#define PHYSICS_DROP_TIMER 0.1
#define PHYSICS_PROP_ROPE_ITERATIONS 4
#define PHYSICS_PROP_MAX_DT 0.05



// Player
#define PLAYER_HAIR_MAX_SEGMENTS 8
#define PLAYER_CLOAK_MAX_HORIZ_SEGMENTS 8
#define PLAYER_CLOAK_MAX_VERT_SEGMENTS 8



// Props
#define PROPS_VINE_STARTING_DYNARR_CAP 1024
#define PROPS_VINE_FRAME_STARTING_POINTS_CAP 1024
#define PROPS_VINE_FRAME_STARTING_STRIPS_CAP 256
#define PROPS_VINE_FRAME_CULL_MARGIN 16
#define PROPS_VINE_Z_INDEX (-10)
