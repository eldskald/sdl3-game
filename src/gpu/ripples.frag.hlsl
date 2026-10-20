#include "includes/fluids-common.hlsli"

#define RIPPLES_DAMPING 0.95

// Red is the current height and green is the previous one.
Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

Texture2D<float4> _Fluids : register(t1, space2);
SamplerState _FluidsSampler : register(s1, space2);

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// Current height of the pixel at `uv`, or `o`, the current height of the pixel
// being ticked, when it isn't the fluid of sample `type`. So ripples bounce off
// anything else without leaving the body of fluid they're in, and surfaces are
// free to rise and fall.
float neighbor(float2 uv, float type, float o) {
    float s = _Fluids.Sample(_FluidsSampler, uv).r;
    float h = _Texture.Sample(_Sampler, uv).r;
    return lerp(o, h, step(abs(s - type), EPSILON));
}

// Ticks the ripples once. The new height is half the sum of the neighbors'
// current heights minus the previous height, damped. It outputs the new height
// on red and the current one on green, to be the previous on the next tick.
// Pixels that aren't fluid are always flat.
float4 main(input i) : SV_Target0 {
    uint width, height;
    _Texture.GetDimensions(width, height);
    float2 dx = float2(1.0 / width, 0.0);
    float2 dy = float2(0.0, 1.0 / height);

    float type = _Fluids.Sample(_FluidsSampler, i.uv).r;
    float fluid = is_fluid(type);
    float2 o = _Texture.Sample(_Sampler, i.uv).rg;
    float u = neighbor(i.uv - dy, type, o.r);
    float l = neighbor(i.uv - dx, type, o.r);
    float d = neighbor(i.uv + dy, type, o.r);
    float r = neighbor(i.uv + dx, type, o.r);

    float h = ((u + l + d + r) / 2.0 - o.g) * RIPPLES_DAMPING;
    return float4(h, o.r, 0.0, 1.0) * float4(fluid, fluid, 1.0, 1.0);
}
