#include "includes/fluids-uniforms.hlsli"
#include "includes/fluids-common.hlsli"
#include "includes/particles-common.hlsli"
#include "includes/normals.hlsli"
#include "includes/simplex-noise.hlsli"

#define SLUDGE_NOISE_FREQ 8.0
#define SLUDGE_NOISE_SPEED 1.0

#define SLUDGE_BACKGROUND_BLEND 0.5

#define SLUDGE_LIGHT_DIR float2(1.0, 1.0)
#define SLUDGE_HIGHLIGHT 0.15
#define SLUDGE_SHADOW 0.1

#define SLUDGE_PARTICLES_TYPE 6
#define SLUDGE_PARTICLES_FREQ 0.05
#define SLUDGE_PARTICLES_VEL float2(0.0, -10.0)
#define SLUDGE_PARTICLES_SPEED_JITTER 0.0
#define SLUDGE_PARTICLES_SPREAD 0.0

float sludge_bubbles(float2 uv) {
    uv += _CamPos;
    uv.x *= ASPECT;
    return snoise(float3(uv * SLUDGE_NOISE_FREQ, _Time * SLUDGE_NOISE_SPEED));
}

Request sludge_particles(
    float2 pos, float2 cell_size, float volume, float dt, uint seed) {
    uint spawn = rand_per_sec(random(seed), dt, SLUDGE_PARTICLES_FREQ);
    spawn = volume >= 0.9 ? spawn : 0;
    Request req;
    req.shape = SHAPE_RECT;
    req.size = cell_size;
    req.pos = pos;
    req.vel = SLUDGE_PARTICLES_VEL;
    req.speed_jitter = SLUDGE_PARTICLES_SPEED_JITTER;
    req.spread = SLUDGE_PARTICLES_SPREAD;
    req.type = SLUDGE_PARTICLES_TYPE;
    req.count = spawn;
    req.seed = seed;
    return req;

}

float4 sludge_col(float2 uv,
                  float4 bg,
                  float3 n,
                  float o,
                  float u,
                  float l,
                  float d,
                  float r) {
    float4 type_col = _Colors[IDX_SLUDGE - 1];
    float4 col = type_col;
    col.rgb = lerp(col.rgb, bg.rgb, SLUDGE_BACKGROUND_BLEND);

    // Light the ripples.
    col.rgb =
        light_normal(col.rgb, n, SLUDGE_LIGHT_DIR, SLUDGE_HIGHLIGHT, SLUDGE_SHADOW);

    // Brighten pixels next to air for a contour.
    float edge = saturate(is_air(u) + is_air(l) + is_air(d) + is_air(r));
    col.rgb = lerp(col.rgb, type_col.rgb, edge);

    return col;
}
