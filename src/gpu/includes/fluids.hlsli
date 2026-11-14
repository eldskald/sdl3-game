#ifndef FLUIDS
#define FLUIDS

#include "includes/fluids-common.hlsli"
#include "includes/particles-common.hlsli"
#include "../../../out/out_includes.hlsli"

float fluid_bubbles(float s, float2 uv) {
    [branch]
    switch (fluid_idx(s)) { FLUID_BUBBLES(uv); }
}

Request fluid_particles(
    int idx, float2 pos, float2 cell_size, float volume, float dt, uint seed) {
    [branch]
    switch (idx) { FLUID_PARTICLES(pos, cell_size, volume, dt, seed); }
}

float4 fluid_col(float2 uv,
                 float4 bg,
                 float3 n,
                 float o,
                 float u,
                 float l,
                 float d,
                 float r) {
    [branch]
    switch (fluid_idx(o)) { FLUID_COL(uv, bg, n, o, u, l, d, r); }
}

#endif
