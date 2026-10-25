#include "includes/fluids-common.hlsli"

// Ripples fade out over this many pixels from the texture's edges, so they're
// absorbed there instead of bouncing back off them. It must be thinner than the
// margin off screen, see `FLUIDS_RIPPLES_TEXTURE_RES_FACTOR`, or ripples would
// fade out on screen too.
#define RIPPLES_SPONGE_WIDTH 32.0

// How much of their height ripples lose per tick right at the texture's edges,
// easing into none at `RIPPLES_SPONGE_WIDTH` from them. It eases in so the
// damping doesn't change abruptly, which would reflect ripples as well.
#define RIPPLES_SPONGE_STRENGTH 0.3

// Red is the current height and green is the previous one.
Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

Texture2D<float4> _Fluids : register(t1, space2);
SamplerState _FluidsSampler : register(s1, space2);

cbuffer Uniforms : register(b0, space3) {
    // How much of their height ripples keep per tick.
    float _Damping;
};

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

// How much of its height the pixel at `uv` keeps, from being close to the edges
// of a texture of `size` pixels. See `RIPPLES_SPONGE_WIDTH`.
float sponge(float2 uv, float2 size) {
    float2 px = uv * size;
    float2 edges = min(px, size - px);
    float t = 1.0 - saturate(min(edges.x, edges.y) / RIPPLES_SPONGE_WIDTH);
    return 1.0 - RIPPLES_SPONGE_STRENGTH * t * t;
}

// Ticks the ripples once. The new height is half the sum of the neighbors'
// current heights minus the previous height, damped. It outputs the new height
// on red and the current one on green, to be the previous on the next tick.
// Pixels that aren't fluid are always flat, and ripples near the texture's edges
// are absorbed, see `sponge()`.
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

    float h = ((u + l + d + r) / 2.0 - o.g) * _Damping *
              sponge(i.uv, float2(width, height));
    return float4(h, o.r, 0.0, 1.0) * float4(fluid, fluid, 1.0, 1.0);
}
