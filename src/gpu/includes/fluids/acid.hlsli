#include "includes/fluids-uniforms.hlsli"
#include "includes/fluids-common.hlsli"
#include "includes/particles-common.hlsli"
#include "includes/normals.hlsli"
#include "includes/simplex-noise.hlsli"

#define ACID_NOISE_FREQ 20.0
#define ACID_NOISE_SPEED 4.0

#define ACID_BACKGROUND_BLEND 0.5

#define ACID_LIGHT_DIR float2(1.0, 1.0)
#define ACID_HIGHLIGHT 0.15
#define ACID_SHADOW 0.1

#define ACID_PARTICLES_TYPE PARTICLES_ACID_BUBBLE
#define ACID_PARTICLES_FREQ 6.0
#define ACID_PARTICLES_VEL float2(0.0, -50.0)
#define ACID_PARTICLES_SPEED_JITTER 0.0
#define ACID_PARTICLES_SPREAD 0.0

float acid_bubbles(float2 uv) {
    uv += _CamPos;
    uv.x *= ASPECT;
    return snoise(float3(uv * ACID_NOISE_FREQ, _Time * ACID_NOISE_SPEED));
}

Request acid_particles(
    float2 pos, float2 cell_size, float volume, float dt, uint seed) {
    uint spawn = rand_per_sec(random(seed), dt, ACID_PARTICLES_FREQ);
    Request req;
    req.shape = SHAPE_RECT;
    req.size = cell_size;
    req.pos = pos;
    req.vel = ACID_PARTICLES_VEL;
    req.speed_jitter = ACID_PARTICLES_SPEED_JITTER;
    req.spread = ACID_PARTICLES_SPREAD;
    req.type = ACID_PARTICLES_TYPE;
    req.count = spawn;
    req.seed = seed;
    return req;
}

float4 acid_col(float2 uv,
                 float4 bg,
                 float3 n,
                 float o,
                 float u,
                 float l,
                 float d,
                 float r) {
    float4 type_col = _Colors[FLUIDS_ACID - 1];
    float4 col = type_col;
    col.rgb = lerp(col.rgb, bg.rgb, ACID_BACKGROUND_BLEND);

    // Light the ripples.
    col.rgb = light_normal(
        col.rgb, n, ACID_LIGHT_DIR, ACID_HIGHLIGHT, ACID_SHADOW);

    // Brighten pixels next to air for a contour.
    float edge = saturate(is_air(u) + is_air(l) + is_air(d) + is_air(r));
    col.rgb = lerp(col.rgb, type_col.rgb, edge);

    return col;
}
