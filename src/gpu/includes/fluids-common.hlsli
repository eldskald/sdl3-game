#ifndef FLUIDS_COMMON
#define FLUIDS_COMMON

#define EPSILON 0.001

#define FAC 32.0

#define IDX_WATER 1.0
#define IDX_LAVA 2.0



// Colors of each fluid indexed by `fluid_type`. Size must be `FLUIDS_TOTAL`.
cbuffer Uniforms : register(b0, space3) {
    float4 _Colors[2];
};

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

// `1.0` when the sample is the fluid of index `idx`, `0.0` otherwise.
float is_type(float s, float idx) {
    return step(abs(s - idx / FAC), EPSILON);
}

#endif
