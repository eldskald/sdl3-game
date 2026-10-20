#include "includes/fluids-uniforms.hlsli"
#include "includes/fluids-common.hlsli"
#include "includes/normals.hlsli"
#include "includes/simplex-noise.hlsli"

#define LAVA_NOISE_FREQUENCY 16.0
#define LAVA_NOISE_SPEED 0.5
#define LAVA_NOISE_ASPECT (16.0 / 9.0)
#define LAVA_NOISE_TINT 0.5

#define LAVA_CONTOUR 0.4
#define LAVA_BACKGROUND_BLEND 0.1

#define LAVA_LIGHT_DIR float2(1.0, 1.0)
#define LAVA_HIGHLIGHT 0.2
#define LAVA_SHADOW 0.1

// Lava's noise at `uv`, from `-1.0` to `1.0`, animated by `_Time`. It's offset
// by `_CamPos` so it stays in place in the world as the camera moves.
float lava_noise(float2 uv) {
    uv += _CamPos;
    uv.x *= LAVA_NOISE_ASPECT;
    return snoise(float3(uv * LAVA_NOISE_FREQUENCY, _Time * LAVA_NOISE_SPEED));
}

// `1.0` when neighbor `s` of lava sample `o` is air or another fluid.
float lava_edge(float o, float s) {
    return (1.0 - is_same(o, s)) * (1.0 - is_wall(s));
}

float4 lava(float2 uv,
             float4 bg,
             float3 n,
             float o,
             float u,
             float l,
             float d,
             float r) {
    float4 col = _Colors[1];
    col.rgb = lerp(col.rgb, bg.rgb, LAVA_BACKGROUND_BLEND);

    // Light the ripples.
    col.rgb = light_normal(
        col.rgb, n, LAVA_LIGHT_DIR, LAVA_HIGHLIGHT, LAVA_SHADOW);

    // Tint it yellow where the noise is high.
    col.rgb = lerp(
        col.rgb, float3(1.0, 1.0, 0.0), saturate(lava_noise(uv)) * LAVA_NOISE_TINT);

    // Brighten pixels next to air or other fluids for a contour.
    float edge = saturate(lava_edge(o, u) + lava_edge(o, l) + lava_edge(o, d) +
                          lava_edge(o, r));
    col.rgb = lerp(col.rgb, float3(1.0, 0.0, 0.0), edge * LAVA_CONTOUR);

    return col;
}
