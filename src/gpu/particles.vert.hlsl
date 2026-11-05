#include "includes/particles-common.hlsli"

StructuredBuffer<Particle> _Particles : register(t0, space0);
StructuredBuffer<ParticleType> _Types : register(t1, space0);
StructuredBuffer<uint> _AliveIndices : register(t2, space0);

cbuffer TargetUniforms : register(b0, space1) {
    float2 _TargetSize;
};

cbuffer Uniforms : register(b1, space1) {
    float2 _CamPos;
    float2 _TextureSize;
    uint _Group;
    uint _ParticleBufferCap;
    int2 _Pad0;
};

struct output {
    float4 pos : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    uint id : TEXCOORD1;
    float age : TEXCOORD2;
};

// Corners of the quad, in the same order as `GRAPHICS_quad()`.
static const float2 CORNERS[6] = {
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0),
};

output main(uint vertex: SV_VertexID, uint instance: SV_InstanceID) {
    output o;
    // Instances are the live particles of `_Group`, listed by the compaction
    // pass, so there are no dead ones to skip.
    uint slot = _AliveIndices[_Group * _ParticleBufferCap + instance];
    Particle p = _Particles[slot];
    ParticleType t = _Types[p.type];

    float2 corner = CORNERS[vertex];
    float2 size = t.src.zw;
    float2 corner_vec = corner * size - size * 0.5;

    float speed = length(p.vel);
    float s = -p.vel.y / speed;
    float c = p.vel.x / speed;
    float2x2 rot = float2x2(c, -s, s, c);
    corner_vec = t.face_vel > 0 ? mul(corner_vec, rot) : corner_vec;

    float2 tan = normalize(float2(-p.vel.y, p.vel.x));
    float2 slither = tan * t.slither_amp * sin(TAU * t.slither_freq * p.age);
    float derivative = TAU * t.slither_amp * t.slither_freq *
                       cos(TAU * t.slither_freq * p.age);
    float slither_ang = -atan2(derivative, speed);

    s = sin(slither_ang);
    c = cos(slither_ang);
    rot = float2x2(c, -s, s, c);
    corner_vec = t.face_vel > 0 ? mul(corner_vec, rot) : corner_vec;

    float2 pos = p.pos + slither + corner_vec;

    // A copy, since `random()` advances it.
    uint seed = p.seed;
    float4 col = lerp(t.col, t.col_jitter, random(seed));
    o.color.rgb = col.rgb * t.col_str;
    o.color.a = col.a * saturate((t.lifetime - p.age) / t.fade_time);
    o.uv = (t.src.xy + corner * size) / _TextureSize;
    float2 ndc = pos / _TargetSize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    o.pos = float4(ndc, 0.0, 1.0);
    o.id = slot;
    o.age = p.age / t.lifetime;
    return o;
}
