#include "includes/fluids-common.hlsli"
#include "includes/water.hlsli"
#include "includes/lava.hlsli"
#include "includes/normals.hlsli"

Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

Texture2D<float4> _Ripples : register(t1, space2);
SamplerState _RipplesSampler : register(s1, space2);

// Everything drawn before the fluids, shown through them.
Texture2D<float4> _PreFluids : register(t2, space2);
SamplerState _PreFluidsSampler : register(s2, space2);

// How much the ripples' slopes tilt their normals.
#define RIPPLES_NORMAL_STRENGTH 16.0

// How far, in pixels, a fully tilted normal offsets what's seen through the
// fluid.
#define RIPPLES_REFRACTION 4.0

// How far, in pixels, surfaces can be waved up or down by the ripples, and how
// many pixels each unit of the ripples' height waves them.
#define WAVES_HEIGHT 2
#define WAVES_STRENGTH 24.0

// How deep, in pixels, fluids must be under a surface for it to wave by `1px`,
// and for it to wave up to `WAVES_HEIGHT`. Shallower surfaces don't wave.
#define WAVES_SHALLOW_DEPTH 3
#define WAVES_DEEP_DEPTH 6

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
// lightest), `c` and `e` are the same material as `o`, and neither `f`, the
// diagonal past `b` away from `a`, nor `g`, the diagonal past `a` away from
// `b`, are. Otherwise `a` or `b` ends a step one pixel tall, which
// `is_corner()` cuts, and filling `o` too would shift the step sideways,
// leaving a gap. Samples are `(type, density rank)`.
float is_inner_corner(
    float2 o, float2 a, float2 b, float2 c, float2 e, float2 f, float2 g) {
    return step(abs(a.r - b.r), EPSILON) * step(o.g + EPSILON, a.g) *
           is_fluid(a.r) * (1.0 - is_wall(o.r)) *
           step(abs(c.r - o.r), EPSILON) * step(abs(e.r - o.r), EPSILON) *
           (1.0 - is_same(f, o)) * (1.0 - is_same(g, o));
}

// How far, in pixels, a surface can be waved with `depth` pixels of fluid under
// it. See `WAVES_SHALLOW_DEPTH` and `WAVES_DEEP_DEPTH`.
float depth_limit(float depth) {
    return step(WAVES_SHALLOW_DEPTH, depth) +
           step(WAVES_DEEP_DEPTH, depth) * (WAVES_HEIGHT - 1);
}

// Sample at `uv`, `(type, density rank)`, with surfaces waved by the ripples.
// Surfaces are the tops of fluids under lighter materials other than walls,
// including lighter fluids. Each is moved up by the ripples' height at its top
// pixel, times `WAVES_STRENGTH` and rounded, or down when it's negative, up to
// `WAVES_HEIGHT` pixels. So it samples the `WAVES_HEIGHT` pixels above and
// below `uv` to find surfaces that can reach it. The height is blurred with its
// left and right pixels, weighted `1, 2, 1`, which cancels out ripples that
// alternate every pixel, otherwise they'd wave surfaces into spikes. How far
// each surface waves is limited by how deep the fluids under it are, of any
// type, see `WAVES_SHALLOW_DEPTH` and `WAVES_DEEP_DEPTH`, so it also samples
// enough pixels under the lowest surface to measure it. Lava surfaces also
// bubble up and down by `lava_noise()`, limited the same way by how deep the
// lava alone is under them, which fades out as the waves get higher so they
// take over smoothly.
float2 waved(float2 uv, float2 dx, float2 dy) {
    float2 s[2 * WAVES_HEIGHT + WAVES_DEEP_DEPTH];
    [unroll] for (int k = 0; k < 2 * WAVES_HEIGHT + WAVES_DEEP_DEPTH; k++)
        s[k] = _Texture.Sample(_Sampler, uv + (k - WAVES_HEIGHT) * dy).rg;

    // For a surface between `k` and `t`, `a` is the lighter material and `b`
    // the denser one. When it's under `uv` and waved up to it, `uv` becomes
    // `b`, and when it's over `uv` and waved down past it, `uv` becomes `a`.
    // Pixels between `uv` and the surface must be on the same side as `uv`.
    float2 x = s[WAVES_HEIGHT];
    [unroll] for (int k = -WAVES_HEIGHT; k < WAVES_HEIGHT; k++) {
        int t = k + 1;
        float2 a = s[k + WAVES_HEIGHT];
        float2 b = s[t + WAVES_HEIGHT];
        float surface = step(a.g + EPSILON, b.g) * (1.0 - is_wall(a.r));

        float2 p = uv + t * dy;
        float h = (_Ripples.Sample(_RipplesSampler, p - dx).r +
                   2.0 * _Ripples.Sample(_RipplesSampler, p).r +
                   _Ripples.Sample(_RipplesSampler, p + dx).r) /
                  4.0;

        // Pixels of any fluid, and of lava alone, from the surface down, up to
        // `WAVES_DEEP_DEPTH`.
        float depth = 0.0;
        float lava_depth = 0.0;
        float run = 1.0;
        float lava_run = 1.0;
        [unroll] for (int j = 0; j < WAVES_DEEP_DEPTH; j++) {
            float type = s[t + j + WAVES_HEIGHT].r;
            run *= is_fluid(type);
            lava_run *= is_type(type, IDX_LAVA);
            depth += run;
            lava_depth += lava_run;
        }
        float limit = depth_limit(depth);
        float wave = clamp(h * WAVES_STRENGTH, -limit, limit);

        float lava_limit = depth_limit(lava_depth);
        float fade = 1.0 - saturate(abs(h * WAVES_STRENGTH) / WAVES_HEIGHT);
        float bubble =
            clamp(lava_noise(p) * WAVES_HEIGHT, -lava_limit, lava_limit) * fade;

        float w = clamp(round(wave + bubble), -WAVES_HEIGHT, WAVES_HEIGHT);

        float side = 1.0;
        if (t > 0) {
            [unroll] for (int j = 0; j < k; j++)
                side *= is_same(s[j + WAVES_HEIGHT], a);
            x = lerp(x, b, surface * side * step(t, w));
        } else {
            [unroll] for (int j = t + 1; j <= 0; j++)
                side *= is_same(s[j + WAVES_HEIGHT], b);
            x = lerp(x, a, surface * side * step(w, t - 1));
        }
    }
    return x;
}

float4 main(input i) : SV_Target0 {
    float4 col = 0;

    uint width, height;
    _Texture.GetDimensions(width, height);
    float2 dx = float2(1.0 / width, 0.0);
    float2 dy = float2(0.0, 1.0 / height);

    // The pixel and its neighbors, waved, so smoothing and contours follow the
    // waves.
    float2 o = waved(i.uv, dx, dy);
    float2 u = waved(i.uv - dy, dx, dy);
    float2 l = waved(i.uv - dx, dx, dy);
    float2 d = waved(i.uv + dy, dx, dy);
    float2 r = waved(i.uv + dx, dx, dy);
    float2 ul = waved(i.uv - dx - dy, dx, dy);
    float2 ur = waved(i.uv + dx - dy, dx, dy);
    float2 dl = waved(i.uv - dx + dy, dx, dy);
    float2 dr = waved(i.uv + dx + dy, dx, dy);

    // Corners poking into a lighter material are painted as it, to smooth out
    // square edges. Corners into air aren't rendered.
    float2 x = o;
    x = lerp(x, u, is_corner(o, u, r, l, d));
    x = lerp(x, u, is_corner(o, u, l, r, d));
    x = lerp(x, d, is_corner(o, d, r, l, u));
    x = lerp(x, d, is_corner(o, d, l, r, u));

    // Inner corners of a denser fluid are painted as it, smoothing the other
    // side. When the corner is air its other two neighbors are air too, so the
    // fluid's function contours it. Steps one pixel tall are only cut, see
    // `is_inner_corner()`.
    x = lerp(x, u, is_inner_corner(o, u, r, l, d, dr, ul));
    x = lerp(x, u, is_inner_corner(o, u, l, r, d, dl, ur));
    x = lerp(x, d, is_inner_corner(o, d, r, l, u, ur, dl));
    x = lerp(x, d, is_inner_corner(o, d, l, r, u, ul, dr));

    // Pixels without fluid show what was drawn before the fluids as is.
    float fluid = is_fluid(x.r);
    col += (1.0 - fluid) * _PreFluids.Sample(_PreFluidsSampler, i.uv);

    // Fluids refract it, offset by the ripples' normal. A flat heightmap has no
    // offset.
    float3 n = height_to_normal(
        _Ripples, _RipplesSampler, i.uv, RIPPLES_NORMAL_STRENGTH);
    float2 offset = n.xy * RIPPLES_REFRACTION * float2(dx.x, dy.y);
    float4 bg = _PreFluids.Sample(_PreFluidsSampler, i.uv + offset);

    // Check if it's water
    col += is_type(x.r, IDX_WATER) * water(i.uv, bg, n, x.r, u.r, l.r, d.r, r.r);

    // Check if it's lava
    col += is_type(x.r, IDX_LAVA) * lava(i.uv, bg, n, x.r, u.r, l.r, d.r, r.r);

    // It covers the whole screen, so it's always opaque.
    col.a = 1.0;
    return col;
}
