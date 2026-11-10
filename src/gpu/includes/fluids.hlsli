#ifndef FLUIDS
#define FLUIDS

#include "includes/fluids-common.hlsli"
#include "includes/water.hlsli"
#include "includes/lava.hlsli"

// Fluid texture sample `s` at `uv`. Varies from `-1.0` to `1.0`.
float fluid_bubbles(float s, float2 uv) {
    [branch] switch (fluid_idx(s)) {
    case IDX_WATER:
        return water_bubbles(uv);
    case IDX_LAVA:
        return lava_bubbles(uv);
    default:
        return 0.0;
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
    [branch] switch (fluid_idx(o)) {
    case IDX_WATER:
        return water_col(uv, bg, n, o, u, l, d, r);
    case IDX_LAVA:
        return lava_col(uv, bg, n, o, u, l, d, r);
    default:
        return 0.0;
    }
}

#endif
