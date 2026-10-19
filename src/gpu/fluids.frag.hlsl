#include "includes/fluids-common.hlsli"
#include "includes/water.hlsli"
#include "includes/lava.hlsli"

Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// `1.0` when `a` and `b` are the same material, lighter than `o` (air being the
// lightest), and `c` and `e` are fluids. Samples are `(type, density rank)`.
float is_corner(float2 o, float2 a, float2 b, float2 c, float2 e) {
    return step(abs(a.r - b.r), EPSILON) * step(a.g + EPSILON, o.g) *
           (1.0 - is_wall(a.r)) * is_fluid(c.r) * is_fluid(e.r);
}

// `1.0` when `a` and `b` are the same fluid, denser than `o` (air being the
// lightest), and `c` and `e` are the same material as `o`. Samples are
// `(type, density rank)`.
float is_inner_corner(float2 o, float2 a, float2 b, float2 c, float2 e) {
    return step(abs(a.r - b.r), EPSILON) * step(o.g + EPSILON, a.g) *
           is_fluid(a.r) * (1.0 - is_wall(o.r)) *
           step(abs(c.r - o.r), EPSILON) * step(abs(e.r - o.r), EPSILON);
}

float4 main(input i) : SV_Target0 {
    float4 col = 0;

    uint width, height;
    _Texture.GetDimensions(width, height);
    float2 dx = float2(1.0 / width, 0.0);
    float2 dy = float2(0.0, 1.0 / height);

    float2 o = _Texture.Sample(_Sampler, i.uv).rg;
    float2 u = _Texture.Sample(_Sampler, i.uv - dy).rg;
    float2 l = _Texture.Sample(_Sampler, i.uv - dx).rg;
    float2 d = _Texture.Sample(_Sampler, i.uv + dy).rg;
    float2 r = _Texture.Sample(_Sampler, i.uv + dx).rg;

    // Corners poking into a lighter material are painted as it, to smooth out
    // square edges. Corners into air aren't rendered.
    float2 x = o;
    x = lerp(x, u, is_corner(o, u, r, l, d));
    x = lerp(x, u, is_corner(o, u, l, r, d));
    x = lerp(x, d, is_corner(o, d, r, l, u));
    x = lerp(x, d, is_corner(o, d, l, r, u));

    // Inner corners of a denser fluid are painted as it, smoothing the other
    // side. When the corner is air its other two neighbors are air too, so the
    // fluid's function contours it.
    x = lerp(x, u, is_inner_corner(o, u, r, l, d));
    x = lerp(x, u, is_inner_corner(o, u, l, r, d));
    x = lerp(x, d, is_inner_corner(o, d, r, l, u));
    x = lerp(x, d, is_inner_corner(o, d, l, r, u));

    // Check if it's water
    col += is_type(x.r, IDX_WATER) * water(i.uv, x.r, u.r, l.r, d.r, r.r);

    // Check if it's lava
    col += is_type(x.r, IDX_LAVA) * lava(i.uv, x.r, u.r, l.r, d.r, r.r);

    return col;
}
