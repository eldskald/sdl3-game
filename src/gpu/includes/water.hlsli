#include "includes/fluids-uniforms.hlsli"
#include "includes/fluids-common.hlsli"
#include "includes/normals.hlsli"

#define WATER_BACKGROUND_BLEND 0.7

#define WATER_LIGHT_DIR float2(1.0, 1.0)
#define WATER_HIGHLIGHT 0.3
#define WATER_SHADOW 0.2

float4 water(float2 uv,
             float4 bg,
             float3 n,
             float o,
             float u,
             float l,
             float d,
             float r) {
    float4 col = _Colors[0];
    col.rgb = lerp(col.rgb, bg.rgb, WATER_BACKGROUND_BLEND);

    // Light the ripples.
    col.rgb = light_normal(
        col.rgb, n, WATER_LIGHT_DIR, WATER_HIGHLIGHT, WATER_SHADOW);

    // Brighten pixels next to air for a contour.
    float edge = saturate(is_air(u) + is_air(l) + is_air(d) + is_air(r));
    col.rgb = lerp(col.rgb, _Colors[0].rgb, edge);

    return col;
}
