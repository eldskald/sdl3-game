#include "includes/fluids-uniforms.hlsli"
#include "includes/fluids-common.hlsli"
#include "includes/particles-common.hlsli"
#include "includes/normals.hlsli"
#include "includes/simplex-noise.hlsli"

#define LAVA_NOISE_FREQ 16.0
#define LAVA_NOISE_SPEED 0.5

#define LAVA_CONTOUR 0.4
#define LAVA_BACKGROUND_BLEND 0.1
#define LAVA_GLOW 1.8

#define LAVA_LIGHT_DIR float2(1.0, 1.0)
#define LAVA_HIGHLIGHT 0.0
#define LAVA_SHADOW 0.4

float lava_bubbles(float2 uv) {
    uv += _CamPos;
    uv.x *= ASPECT;
    return snoise(float3(uv * LAVA_NOISE_FREQ, _Time * LAVA_NOISE_SPEED));
}

// `1.0` when neighbor `s` of lava sample `o` is air or another fluid.
float lava_edge(float o, float s) {
    return (1.0 - is_same(o, s)) * (1.0 - is_wall(s));
}

// Particles a lava surface spawns this frame, see `fluid_particles()`.
Request lava_particles(
    float2 pos, float2 cell_size, float volume, float dt, uint seed) {
    return (Request)0;
}

float4 lava_col(float2 uv,
                float4 bg,
                float3 n,
                float o,
                float u,
                float l,
                float d,
                float r) {
    float4 col = _Colors[FLUIDS_LAVA - 1];

    // Ripples.
    col.rgb = lerp(col.rgb, bg.rgb, LAVA_BACKGROUND_BLEND);
    col.rgb =
        light_normal(col.rgb, n, LAVA_LIGHT_DIR, LAVA_HIGHLIGHT, LAVA_SHADOW);

    // Contour.
    float edge = saturate(lava_edge(o, u) + lava_edge(o, l) + lava_edge(o, d) +
                          lava_edge(o, r));
    col.rgb = lerp(col.rgb, float3(1.0, 0.0, 0.0), edge * LAVA_CONTOUR);

    return col * LAVA_GLOW;
}
