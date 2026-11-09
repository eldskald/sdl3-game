#pragma once
#include "data-structs/hashmap.h"
#include "defs.h"
#include "engine/graphics.h"
#include <SDL3/SDL.h>

// Particle types, whose data is on `PARTICLES_CSV_PATH`. `particle_none` is
// for no particle, like a type without death particles.
//
// ```
// enum particle_type { particle_none, PARTICLES_TYPES };
// ```
typedef enum { particle_none, PARTICLES_TYPES } particle_type;

// Fragment shaders particles are drawn with, set by the `shader` column of
// `PARTICLES_CSV_PATH`, where an empty cell is `particle_shader_base`. They're
// the groups particles are drawn in, each on a draw of its own, under the
// groups after it. `particle_shaders_total` counts them.
typedef enum {
    particle_shader_base,
    particle_shader_glow,
    particle_shaders_total,
} particle_shader;

// Size of `Particle` on `src/gpu/includes/particles-common.hlsli`. The CPU
// never reads or writes particles, so it has no struct for them, only buffers
// of this many bytes per particle.
#define PARTICLE_SIZE 48



// Loads the particle types and creates the buffers and shaders the particles
// are simulated and drawn with. Call after `GRAPHICS_start()`. Returns `0` when
// successful, `1` otherwise.
int PARTICLES_start(void);

// Releases everything `PARTICLES_start()` created. Call before
// `GRAPHICS_stop()`.
void PARTICLES_stop(void);



// Shapes particles are spawned from, see `particles_emit_settings`.
//
// ```
// enum particles_emit_shape {
//     particles_emit_point,
//     particles_emit_rect,
// };
// ```
typedef enum {
    particles_emit_point,
    particles_emit_rect,
} particles_emit_shape;

// How particles are spawned by `PARTICLES_emit()`. They spawn at `pos` when
// `shape` is `particles_emit_point`, or at random positions inside `rect` when
// it's `particles_emit_rect`, both in world pixels. Each spawns with `vel`
// initial velocity, angled randomly by up to `spread` degrees and slowed by
// a random fraction up to `speed_jitter` of its speed. `count` is how many
// spawn.
//
// ```
// struct particles_emit_settings {
//     particles_emit_shape shape;
//     union {
//         SDL_FPoint pos;
//         SDL_FRect rect;
//     };
//     SDL_FPoint vel;
//     float speed_jitter;
//     float spread;
//     Uint32 count;
// };
// ```
typedef struct {
    particles_emit_shape shape;
    union {
        SDL_FPoint pos;
        SDL_FRect rect;
    };
    SDL_FPoint vel;
    float speed_jitter;
    float spread;
    Uint32 count;
} particles_emit_settings;

// Queues spawning particles of `type` as set by `settings`. They spawn on the
// next `PARTICLES_draw()`. Spawns past `PARTICLES_MAX_REQUESTS` a frame are
// dropped. Call in any thread.
void PARTICLES_emit(particle_type type, particles_emit_settings settings);

// Simulates the particles for a frame and draws them on the current render
// target, relative to the camera, with their sprites from `spritesheet`. Does
// nothing before `PARTICLES_start()`. Call on renderer update.
void PARTICLES_draw(graphics_texture* spritesheet);



// Get a `string-->particle_type` map, used to translate the particle names
// written on CSV files into their corresponding `particle_type`.
hashmap PARTICLES_get_string_particle_types_map(void);

// Get a `string-->particle_shader` map, used to translate the particle shader
// names written on CSV files into their corresponding `particle_shader`.
hashmap PARTICLES_get_string_particle_shaders_map(void);
