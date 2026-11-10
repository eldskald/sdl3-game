#include "includes/fluids-common.hlsli"

#define RIPPLES_SPONGE_WIDTH 32.0
#define RIPPLES_SPONGE_STRENGTH 0.3

// Red is the current height and green is the previous one.
Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

Texture2D<float4> _Fluids : register(t1, space2);
SamplerState _FluidsSampler : register(s1, space2);

// Each draw call we flip this value, 1, 0, 1, 0, etc so we only iterate slow
// fluids once every two calls.
cbuffer Uniforms : register(b0, space3) {
    uint _StepSlow;
    int _Pad0;
    int _Pad1;
    int _Pad2;
};

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// Slow fluids set the the fluids texture's alpha channel to 128. We tick them
// when _StepSlow is 1.
float is_ticking(float2 uv) {
    float is_normal = step(0.75, _Fluids.Sample(_FluidsSampler, uv).a);
    return max(is_normal, (float)_StepSlow);
}

// To make ripples reflect off of non-fluids and even fluids of different type,
// as well as allowing the waves to pass through the other fluid type.
float neighbor(float2 uv, float type, float o) {
    float s = _Fluids.Sample(_FluidsSampler, uv).r;
    float h = _Texture.Sample(_Sampler, uv).r;
    return lerp(o, h, is_same(type, s));
}

// Sponge to absorb ripples at the edges of the texture.
float sponge(float2 uv, float2 size) {
    float2 px = uv * size;
    float2 edges = min(px, size - px);
    float t = 1.0 - saturate(min(edges.x, edges.y) / RIPPLES_SPONGE_WIDTH);
    return 1.0 - RIPPLES_SPONGE_STRENGTH * t * t;
}

float4 main(input i) : SV_Target0 {
    uint width, height;
    _Texture.GetDimensions(width, height);
    float2 dx = float2(1.0 / width, 0.0);
    float2 dy = float2(0.0, 1.0 / height);

    float type = _Fluids.Sample(_FluidsSampler, i.uv).r;
    float tick = is_ticking(i.uv);
    float damping = _Fluids.Sample(_FluidsSampler, i.uv).b;
    float fluid = is_fluid(type);
    float2 o = _Texture.Sample(_Sampler, i.uv).rg;
    float u = neighbor(i.uv - dy, type, o.r);
    float l = neighbor(i.uv - dx, type, o.r);
    float d = neighbor(i.uv + dy, type, o.r);
    float r = neighbor(i.uv + dx, type, o.r);

    float h = ((u + l + d + r) / 2.0 - o.g) * (1.0 - damping) *
              sponge(i.uv, float2(width, height));
    float2 res = lerp(o, float2(h, o.r), tick);

    return float4(res, 0.0, 1.0) * float4(fluid, fluid, 1.0, 1.0);
}
