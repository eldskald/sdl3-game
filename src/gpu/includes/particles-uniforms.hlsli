// Uniforms of the particles' compute shaders, matching `compute_uniforms` on
// `src/engine/particles.c`.
cbuffer Uniforms : register(b0, space2) {
    int4 _FluidSimRect;
    float2 _FluidCellSize;
    int2 _Pad0;
    float _Dt;
    uint _ParticleBufferCap;
    uint _MaxRequests;
    int _Pad1;
};
