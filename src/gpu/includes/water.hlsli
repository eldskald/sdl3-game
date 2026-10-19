#include "includes/fluids-common.hlsli"

#define WATER_CONTOUR 0.8

float4 water(float2 uv, float o, float u, float l, float d, float r) {
    float4 col = _Colors[0];

    // Brighten pixels next to air for a contour.
    float edge = saturate(is_air(u) + is_air(l) + is_air(d) + is_air(r));
    col.rgb = lerp(col.rgb, 1.0, edge * WATER_CONTOUR);

    return col;
}
