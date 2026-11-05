#include "includes/particles-common.hlsli"
#include "includes/particles-uniforms.hlsli"

StructuredBuffer<ParticleType> _Types : register(t0, space0);
StructuredBuffer<uint> _Grid : register(t1, space0);
RWStructuredBuffer<Particle> _Particles : register(u0, space1);
RWStructuredBuffer<Request> _Requests : register(u1, space1);
RWStructuredBuffer<uint> _Counters : register(u2, space1);

#define CODE_OUTSIDE 256
#define CODE_WALL 255

uint grid_at(float2 pos) {
    int2 cell = int2(floor(pos / _FluidCellSize)) - _FluidSimRect.xy;
    if (cell.x < 0 || cell.y < 0 || cell.x >= _FluidSimRect.z ||
        cell.y >= _FluidSimRect.w)
        return CODE_OUTSIDE;
    uint i = cell.y * _FluidSimRect.z + cell.x;
    return (_Grid[i / 4] >> (i % 4 * 8)) & 0xFF;
}

float2 wall_normal(float2 pos, float2 move) {
    if (grid_at(pos + move) != CODE_WALL) return float2(0.0, 0.0);
    float2 n = float2(0.0, 0.0);
    if (grid_at(pos + float2(move.x, 0.0)) == CODE_WALL) n.x = -sign(move.x);
    if (grid_at(pos + float2(0.0, move.y)) == CODE_WALL) n.y = -sign(move.y);
    if (n.x == 0.0 && n.y == 0.0) n = -sign(move);
    return n;
}

float2 bounce(float2 v, float2 n) {
    return v * (1.0 - 2.0 * abs(n));
}

void trail(inout Particle p, ParticleType t) {
    if (t.trail_freq == 0.0 || t.trail_particles == 0) return;
    p.time_since_last_trail += _Dt;
    while (p.time_since_last_trail >= 1 / t.trail_freq) {
        p.time_since_last_trail -= 1 / t.trail_freq;
        uint i;
        InterlockedAdd(_Counters[COUNTER_REQUESTS], 1, i);
        if (i < _MaxRequests) {
            Request r;

            // Same logic as death particles
            float2 dir = float2(0.0, 0.0);
            if (length(p.vel) > EPSILON)
                dir = normalize(p.vel);
            else if (length(t.acc) > EPSILON)
                dir = -normalize(t.acc);
            r.vel = -dir * t.trail_speed;

            r.pos = p.pos;
            r.spread = t.trail_spread;
            r.speed_jitter = t.trail_speed_jitter;
            r.type = t.trail_particles;
            r.count = 1;
            // `p.seed` stays as it was, since the vertex shader picks the
            // particle's color from it. Its age and the time left until the
            // next trail differ on every emission, even many in one frame.
            r.seed = hash(hash(p.seed ^ asuint(p.age)) ^
                          asuint(p.time_since_last_trail));
            _Requests[i] = r;
        }
    }
}

// Kills `p` spawning its death particles.
void die(inout Particle p, ParticleType t, float2 normal) {
    if (t.death_particles != 0 && t.death_count > 0) {
        uint i;
        InterlockedAdd(_Counters[COUNTER_REQUESTS], 1, i);
        if (i < _MaxRequests) {
            Request r;

            // A particle dying still was moving against its acceleration,
            // like at the top of an arc, so that's where the death particles
            // go instead. Without acceleration either, they spawn still.
            float2 dir = float2(0.0, 0.0);
            if (length(p.vel) > EPSILON)
                dir = normalize(p.vel);
            else if (length(t.acc) > EPSILON)
                dir = -normalize(t.acc);
            r.vel = bounce(dir, normal) * t.death_speed;

            r.pos = p.pos;
            r.spread = t.death_spread;
            r.speed_jitter = t.death_speed_jitter;
            r.type = t.death_particles;
            r.count = t.death_count;
            r.seed = p.seed;
            _Requests[i] = r;
        }
    }
    p.type = 0;
}

[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    if (id.x >= _ParticleBufferCap) return;
    Particle p = _Particles[id.x];
    if (p.type == 0) return;
    ParticleType t = _Types[p.type];

    p.age += _Dt;
    if (p.age >= t.lifetime) {
        die(p, t, float2(0.0, 0.0));
    } else {

        p.vel += t.acc * _Dt;
        p.vel *= pow((1.0 - t.damping), _Dt);
        float2 move = p.vel * _Dt;
        float2 next = p.pos + move;

        if (t.bounce_until > 0.0 && p.age > t.bounce_until)
            p.remaining_bounces = 0;

        uint code = grid_at(next);
        if (code == CODE_WALL) {
            float2 normal = wall_normal(p.pos, move);
            if (p.remaining_bounces > 0) {
                p.vel = bounce(p.vel, normal) * t.bounce;
                p.remaining_bounces--;
            } else {
                die(p, t, normal);
            }
        } else if (code == CODE_OUTSIDE || (code > 0 && t.ignore_fluids != 0)) {
            p.type = 0;
        } else {
            p.pos = next;
            trail(p, t);
        }
    }
    _Particles[id.x] = p;
}
