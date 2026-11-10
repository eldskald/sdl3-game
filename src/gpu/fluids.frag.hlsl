#include "includes/fluids.hlsli"
#include "includes/normals.hlsli"

Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

Texture2D<float4> _Ripples : register(t1, space2);
SamplerState _RipplesSampler : register(s1, space2);

Texture2D<float4> _PreFluids : register(t2, space2);
SamplerState _PreFluidsSampler : register(s2, space2);

// `_Texture` and `_Ripples` are bigger than the screen and centered on it, so
// ripples are simulated off screen too, and only their part on screen is drawn.
// Converts `uv` on them to uv on the screen, which `_PreFluids` covers and
// effects anchored to the world, like lava's bubbles, are scaled by.
float2 to_screen_uv(float2 uv) {
    uint width, height, screen_width, screen_height;
    _Texture.GetDimensions(width, height);
    _PreFluids.GetDimensions(screen_width, screen_height);
    float2 size = float2(width, height);
    float2 screen = float2(screen_width, screen_height);
    return (uv * size - (size - screen) / 2.0) / screen;
}

// How much the ripples' slopes tilt their normals.
#define RIPPLES_NORMAL_STRENGTH 16.0

// How far, in pixels, surfaces can be waved up or down by the ripples, and how
// many pixels each unit of the ripples' height waves them.
#define WAVES_HEIGHT 2
#define WAVES_STRENGTH 24.0

// How far, in pixels, surfaces can be bubbled up or down by their fluid's
// `fluid_bubbles()`.
#define BUBBLES_HEIGHT 2

// How far, in pixels, surfaces can move up or down, waved and bubbled.
#define SURFACE_HEIGHT (WAVES_HEIGHT + BUBBLES_HEIGHT)

// How deep, in pixels, fluids must be under a surface for it to wave or bubble
// by `1px`, and for it to wave up to `WAVES_HEIGHT` or bubble up to
// `BUBBLES_HEIGHT`. Shallower surfaces don't move.
#define SURFACE_SHALLOW_DEPTH 3
#define SURFACE_DEEP_DEPTH 6

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

// How far, in pixels, a surface that can move up to `height` pixels can move
// with `depth` pixels of fluid under it. See `SURFACE_SHALLOW_DEPTH` and
// `SURFACE_DEEP_DEPTH`.
float depth_limit(float depth, float height) {
    return step(SURFACE_SHALLOW_DEPTH, depth) +
           step(SURFACE_DEEP_DEPTH, depth) * (height - 1);
}

// Sample at `uv`, `(type, density rank)`, with surfaces waved by the ripples
// and bubbled by their fluids. Surfaces are the tops of fluids under lighter
// materials other than walls, including lighter fluids. Each is moved up by the
// ripples' height at its top pixel, times `WAVES_STRENGTH`, up to
// `WAVES_HEIGHT` pixels, plus its fluid's `fluid_bubbles()` there, times
// `BUBBLES_HEIGHT`, then rounded, or down when it's negative. So it samples the
// `SURFACE_HEIGHT` pixels above and below `uv` to find surfaces that can reach
// it. The ripples' height is blurred with its left and right pixels, weighted
// `1, 2, 1`, which cancels out ripples that alternate every pixel, otherwise
// they'd wave surfaces into spikes. How far each surface waves is limited by
// how deep the fluids under it are, of any type, and how far it bubbles by how
// deep its fluid alone is, see `depth_limit()`, so it also samples enough
// pixels under the lowest surface to measure them.
float2 waved(float2 uv, float2 dx, float2 dy) {
    float2 s[2 * SURFACE_HEIGHT + SURFACE_DEEP_DEPTH];
    [unroll] for (int k = 0; k < 2 * SURFACE_HEIGHT + SURFACE_DEEP_DEPTH; k++)
        s[k] = _Texture.Sample(_Sampler, uv + (k - SURFACE_HEIGHT) * dy).rg;

    // For a surface between `k` and `t`, `a` is the lighter material and `b`
    // the denser one. When it's under `uv` and moved up to it, `uv` becomes
    // `b`, and when it's over `uv` and moved down past it, `uv` becomes `a`.
    // Pixels between `uv` and the surface must be on the same side as `uv`.
    float2 x = s[SURFACE_HEIGHT];
    [unroll] for (int k = -SURFACE_HEIGHT; k < SURFACE_HEIGHT; k++) {
        int t = k + 1;
        float2 a = s[k + SURFACE_HEIGHT];
        float2 b = s[t + SURFACE_HEIGHT];
        float surface = step(a.g + EPSILON, b.g) * (1.0 - is_wall(a.r));

        float2 p = uv + t * dy;
        float h = (_Ripples.Sample(_RipplesSampler, p - dx).r +
                   2.0 * _Ripples.Sample(_RipplesSampler, p).r +
                   _Ripples.Sample(_RipplesSampler, p + dx).r) /
                  4.0;

        // Pixels of any fluid, and of `b`'s fluid alone, from the surface
        // down, up to `SURFACE_DEEP_DEPTH`.
        float depth = 0.0;
        float fluid_depth = 0.0;
        float run = 1.0;
        float fluid_run = 1.0;
        [unroll] for (int j = 0; j < SURFACE_DEEP_DEPTH; j++) {
            float type = s[t + j + SURFACE_HEIGHT].r;
            run *= is_fluid(type);
            fluid_run *= is_same(type, b.r);
            depth += run;
            fluid_depth += fluid_run;
        }

        float wave_limit = depth_limit(depth, WAVES_HEIGHT);
        float wave = clamp(h * WAVES_STRENGTH, -wave_limit, wave_limit);

        float bubble_limit = depth_limit(fluid_depth, BUBBLES_HEIGHT);
        float bubble = clamp(fluid_bubbles(b.r, to_screen_uv(p)) *
                                 BUBBLES_HEIGHT,
                             -bubble_limit,
                             bubble_limit);

        float w = round(wave + bubble);

        float side = 1.0;
        if (t > 0) {
            [unroll] for (int j = 0; j < k; j++)
                side *= is_same(s[j + SURFACE_HEIGHT], a);
            x = lerp(x, b, surface * side * step(t, w));
        } else {
            [unroll] for (int j = t + 1; j <= 0; j++)
                side *= is_same(s[j + SURFACE_HEIGHT], b);
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
    float2 screen_uv = to_screen_uv(i.uv);
    float fluid = is_fluid(x.r);
    col += (1.0 - fluid) * _PreFluids.Sample(_PreFluidsSampler, screen_uv);

    // Fluids refract it, offset by the ripples' normal. A flat heightmap has no
    // offset.
    float3 n = height_to_normal(
        _Ripples, _RipplesSampler, i.uv, RIPPLES_NORMAL_STRENGTH);
    float2 offset = n.xy * _Refraction * float2(dx.x, dy.y);
    float4 bg =
        _PreFluids.Sample(_PreFluidsSampler, to_screen_uv(i.uv + offset));

    // Each fluid colors its own pixels.
    col += fluid_col(screen_uv, bg, n, x.r, u.r, l.r, d.r, r.r);

    // It covers the whole screen, so it's always opaque.
    col.a = 1.0;
    return col;
}
