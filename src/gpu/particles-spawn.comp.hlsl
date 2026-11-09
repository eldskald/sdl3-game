#include "includes/particles-common.hlsli"
#include "includes/particles-uniforms.hlsli"

StructuredBuffer<ParticleType> _Types : register(t0, space0);
RWStructuredBuffer<Particle> _Particles : register(u0, space1);
RWStructuredBuffer<Request> _Requests : register(u1, space1);
RWStructuredBuffer<uint> _Counters : register(u2, space1);

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint n = min(_Counters[COUNTER_REQUESTS], _MaxRequests);
    if (id.x >= n) return;
    Request r = _Requests[id.x];
    ParticleType t = _Types[r.type];

    for (uint k = 0; k < r.count; k++) {
        uint slot;
        InterlockedAdd(_Counters[COUNTER_NEXT_SLOT], 1, slot);
        slot &= _ParticleBufferCap - 1;

        uint seed = hash(r.seed + k);

        float speed_diff = random(seed) * r.speed_jitter;
        float angle_diff = (random(seed) - 0.5) * r.spread * DEG2RAD;
        float s = sin(angle_diff);
        float c = cos(angle_diff);
        float2x2 rot = float2x2(c, -s, s, c);
        float2 vel = mul(r.vel, rot) * (1.0 - speed_diff);

        float2 pos = r.pos;
        if (r.shape == SHAPE_RECT)
            pos += float2(random(seed), random(seed)) * r.size;

        Particle p;
        p.pos = pos;
        p.vel = vel;
        p.age = random(seed) * t.lifetime * t.lifetime_jitter;
        p.remaining_bounces = t.bounce_count;
        p.time_since_last_trail = 0.0;
        p.type = r.type;
        p.seed = seed;
        _Particles[slot] = p;
    }
}
