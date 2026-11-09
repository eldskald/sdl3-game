// Shared by the particles' shaders. Structs here match those on
// `src/engine/particles.c`, so every member is aligned to its own size and
// every struct is a multiple of 16 bytes, which lays them out the same on
// SPIR-V and DXIL.

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
    float speed_jitter;
    float spread;
    uint type;
    uint count;
    uint seed;
    uint shape; // One of the `SHAPE_` defines
    float2 size; // Of the rect, when `shape` is `SHAPE_RECT`
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

#define EPSILON 0.0001
#define PI 3.14159265
#define TAU (PI * 2)
#define DEG2RAD (PI / 180)

// Matches `particles_emit_shape` on `src/engine/particles.h`.
#define SHAPE_POINT 0
#define SHAPE_RECT 1

#define COUNTER_REQUESTS 0
#define COUNTER_NEXT_SLOT 1

// PCG's, from "Hash Functions for GPU Rendering", Jarzynski and Olano, 2020.
uint hash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Advances `seed` and returns a random float in `[0.0, 1.0)` from it.
float random(inout uint seed) {
    seed = hash(seed);
    return (seed >> 8) / 16777216.0;
}
