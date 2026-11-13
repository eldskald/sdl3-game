// Shared by the particles' shaders. Structs here match those on
// `src/engine/particles.c`, so every member is aligned to its own size and
// every struct is a multiple of 16 bytes, which lays them out the same on
// SPIR-V and DXIL.

#ifndef PARTICLES_COMMON
#define PARTICLES_COMMON

#include "includes/common.hlsli"
#include "../../../out/out_particles.hlsli"

struct Particle {
    float2 pos;
    float2 vel;
    float age;
    float time_since_last_trail;
    uint remaining_bounces;
    uint type;
    uint seed;
    int pad0;
    int pad1;
    int pad2;
};

struct ParticleType {
    float4 col;
    float4 col_jitter;
    float4 src;
    float2 acc;
    int2 pad0;

    float lifetime;
    float lifetime_jitter;
    float fade_time;
    uint ignore_fluids;

    uint face_vel;
    float damping;
    float slither_freq;
    float slither_amp;

    uint bounce_count;
    float bounce;
    float bounce_until;

    float col_str;

    uint death_particles;
    uint death_count;
    float death_speed;
    float death_spread;
    float death_speed_jitter;

    uint trail_particles;
    float trail_freq;
    float trail_speed;
    float trail_spread;
    float trail_speed_jitter;

    uint group;
    int pad1;
};

struct Request {
    float2 pos;
    float2 vel;
    float2 size; // Of the rect, when `shape` is `SHAPE_RECT`
    float speed_jitter;
    float spread;
    uint type;
    uint count;
    uint seed;
    uint shape; // One of the `SHAPE_` defines
};

// Matches `SDL_GPUIndirectDrawCommand`, one per group on the draw arguments'
// buffer. The compaction pass counts each group's live particles on its
// `num_instances`, the rest is set by the CPU.
struct DrawArgs {
    uint num_vertices;
    uint num_instances;
    uint first_vertex;
    uint first_instance;
};

// Matches `particles_emit_shape` on `src/engine/particles.h`.
#define SHAPE_POINT 0
#define SHAPE_RECT 1

#define COUNTER_REQUESTS 0
#define COUNTER_NEXT_SLOT 1

#endif
