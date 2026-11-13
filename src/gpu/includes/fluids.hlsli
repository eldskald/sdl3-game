#ifndef FLUIDS
#define FLUIDS

#include "includes/fluids-common.hlsli"
#include "includes/particles-common.hlsli"
#include "includes/water.hlsli"
#include "includes/lava.hlsli"
#include "includes/acid.hlsli"
#include "includes/sludge.hlsli"

// Fluid texture sample `s` at `uv`. Varies from `-1.0` to `1.0`.
float fluid_bubbles(float s, float2 uv) {
    [branch]
    switch (fluid_idx(s)) {
    case FLUIDS_WATER:
        return water_bubbles(uv);
    case FLUIDS_LAVA:
        return lava_bubbles(uv);
    case FLUIDS_ACID:
        return acid_bubbles(uv);
    case FLUIDS_SLUDGE:
        return sludge_bubbles(uv);
    default:
        return 0.0;
    }
}

// Particles spawned this frame by a surface of the fluid of index `idx`, one of
// the `IDX_*` defines, see `src/gpu/fluids-surfaces.comp.hlsl`. The surface is
// on a cell of `cell_size` pixels with its left end at `pos`, in world pixels.
// `volume` is how full that cell and the one under it are together, of any
// fluid, from `0.0` to `1.0`, so `1.0` unless it's a shallow puddle. `dt` is the
// seconds since the last frame and `seed` is random, different for every
// surface on every frame. Advance it with `random()` to vary each surface, like
// spawning only `random(seed) < rate * dt` to spawn `rate` times a second on
// average. Returns the request of the particles, which has a `count` of `0`
// when there are none, since `(Request)0` is a request of nothing.
Request fluid_particles(
    int idx, float2 pos, float2 cell_size, float volume, float dt, uint seed) {
    [branch]
    switch (idx) {
    case FLUIDS_WATER:
        return water_particles(pos, cell_size, volume, dt, seed);
    case FLUIDS_LAVA:
        return lava_particles(pos, cell_size, volume, dt, seed);
    case FLUIDS_ACID:
        return acid_particles(pos, cell_size, volume, dt, seed);
    case FLUIDS_SLUDGE:
        return sludge_particles(pos, cell_size, volume, dt, seed);
    default:
        return (Request)0;
    }
}

// Final color of fluid sample `o` at `uv`, the screen's uv, with `bg` the
// refracted background, `n` the ripples' normal and `u`, `l`, `d` and `r` its
// neighbors. `0.0` for air and walls.
float4 fluid_col(float2 uv,
                 float4 bg,
                 float3 n,
                 float o,
                 float u,
                 float l,
                 float d,
                 float r) {
    [branch]
    switch (fluid_idx(o)) {
    case FLUIDS_WATER:
        return water_col(uv, bg, n, o, u, l, d, r);
    case FLUIDS_LAVA:
        return lava_col(uv, bg, n, o, u, l, d, r);
    case FLUIDS_ACID:
        return acid_col(uv, bg, n, o, u, l, d, r);
    case FLUIDS_SLUDGE:
        return sludge_col(uv, bg, n, o, u, l, d, r);
    default:
        return 0.0;
    }
}

#endif
