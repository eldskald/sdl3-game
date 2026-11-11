#include "includes/fluids-uniforms.hlsli"
#include "includes/fluids-common.hlsli"
#include "includes/particles-common.hlsli"
#include "includes/normals.hlsli"

#define WATER_BACKGROUND_BLEND 0.7

#define WATER_LIGHT_DIR float2(1.0, 1.0)
#define WATER_HIGHLIGHT 0.15
#define WATER_SHADOW 0.1

float water_bubbles(float2 uv) {
    return 0.0;
}

// Particles a water surface spawns this frame, see `fluid_particles()`.
Request water_particles(
    float2 pos, float2 cell_size, float volume, float dt, uint seed) {
    return (Request)0;
}

float4 water_col(float2 uv,
                 float4 bg,
                 float3 n,
                 float o,
                 float u,
                 float l,
                 float d,
                 float r) {
    float4 col = _Colors[IDX_WATER - 1];
    col.rgb = lerp(col.rgb, bg.rgb, WATER_BACKGROUND_BLEND);

    // Light the ripples.
    col.rgb = light_normal(
        col.rgb, n, WATER_LIGHT_DIR, WATER_HIGHLIGHT, WATER_SHADOW);

    // Brighten pixels next to air for a contour.
    float edge = saturate(is_air(u) + is_air(l) + is_air(d) + is_air(r));
    col.rgb = lerp(col.rgb, _Colors[0].rgb, edge);

    return col;
}
