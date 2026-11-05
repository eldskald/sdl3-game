#include "includes/particles-common.hlsli"
#include "includes/particles-uniforms.hlsli"

StructuredBuffer<ParticleType> _Types : register(t0, space0);
StructuredBuffer<Particle> _Particles : register(t1, space0);
RWStructuredBuffer<uint> _AliveIndices : register(u0, space1);
RWStructuredBuffer<DrawArgs> _DrawArgs : register(u1, space1);

[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    if (id.x >= _ParticleBufferCap) return;
    Particle p = _Particles[id.x];
    if (p.type == 0) return;

    uint group = _Types[p.type].group;
    uint n;
    InterlockedAdd(_DrawArgs[group].num_instances, 1, n);
    _AliveIndices[group * _ParticleBufferCap + n] = id.x;
}
