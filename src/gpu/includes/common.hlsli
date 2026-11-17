// Shared by every shader: constants and helpers that don't belong to any one
// system.

#ifndef COMMON
#define COMMON

#define EPSILON 0.0001
#define PI 3.14159265
#define TAU (PI * 2)
#define DEG2RAD (PI / 180)

// Aspect ratio of the screen, `WINDOW_SIZE_X / WINDOW_SIZE_Y` on `src/defs.h`.
#define ASPECT (16.0 / 9.0)

// UV size of a spritesheet cell, `SPRITESHEET_CELL_X/Y / SPRITESHEET_X/Y` on `src/defs.h`.
#define SPRITESHEET_CELL float2(1.0 / 32.0, 1.0 / 32.0)

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

// Safely normalizes a vector, if it's zero then it returns zero instead of NaN
float2 norm(float2 v) {
    float sq_len = dot(v, v);
    return sq_len > EPSILON ? v * rsqrt(sq_len) : float2(0.0, 0.0);
}

#endif
