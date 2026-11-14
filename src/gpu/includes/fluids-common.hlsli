#ifndef FLUIDS_COMMON
#define FLUIDS_COMMON

#include "includes/common.hlsli"
#include "includes/particles-common.hlsli"
#include "../../../out/out_fluids.hlsli"

#define FAC 32.0

// `1.0` when the sample is air, `0.0` otherwise.
float is_air(float s) {
    return step(s, EPSILON);
}

// `1.0` when the sample is any fluid, `0.0` when it's air or wall.
float is_fluid(float s) {
    return step(EPSILON, s) * step(s, 1.0 - EPSILON);
}

// `1.0` when the sample is a wall, `0.0` otherwise.
float is_wall(float s) {
    return step(1.0 - EPSILON, s);
}

// Index of the material of sample `s`, `0` for air, `IDX_*` for fluids.
int fluid_idx(float s) {
    return (int)round(s * FAC);
}

// `1.0` when the sample is the fluid of index `idx`, `0.0` otherwise.
float is_type(float s, float idx) {
    return step(abs(s - idx / FAC), EPSILON);
}

// `1.0` when samples `a` and `b` are the same material.
float is_same(float a, float b) {
    return step(abs(a - b), EPSILON);
}

// `1.0` when samples `a` and `b`, `(type, density rank)`, are the same
// material.
float is_same(float2 a, float2 b) {
    return is_same(a.r, b.r);
}

// If you call this every frame, expect it to return `1` a `freq` amount per
// second. `rand` is a number from `0.0` to `1.0` from the `random()` function,
// and `dt` is the frame time in seconds. `freq` can't be higher than the frame
// rate.
uint rand_per_sec(float rand, float dt, float freq) {
    return rand < freq * dt ? 1 : 0;
}

#endif
