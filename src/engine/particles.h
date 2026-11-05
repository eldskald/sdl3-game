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



// Loads the particle types and creates the buffers and shaders the particles
// are simulated and drawn with. Call after `GRAPHICS_start()`. Returns `0` when
// successful, `1` otherwise.
int PARTICLES_start(void);

// Releases everything `PARTICLES_start()` created. Call before
// `GRAPHICS_stop()`.
void PARTICLES_stop(void);



// Queues spawning `count` particles of `type` at `(x,y)`, in world pixels, with
// `(vel_x,vel_y)` initial velocity that can be angled randomly by `spread`
// degrees. They spawn on the next `PARTICLES_draw()`. Spawns past
// `PARTICLES_MAX_REQUESTS` a frame are dropped. Call in any thread.
void PARTICLES_emit(particle_type type,
                    float x,
                    float y,
                    float vel_x,
                    float vel_y,
                    float speed_jitter,
                    float spread,
                    Uint32 count);

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
