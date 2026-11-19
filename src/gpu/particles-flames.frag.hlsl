#include "includes/common.hlsli"

Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    uint id : TEXCOORD1;
    float age : TEXCOORD2;
};

// This is meant to animate and randomize the particle's frames. On the spritesheet,
// there are four different flame particle sprites, each animated with six frames.
// The animation frames go from left to right, and the variations are stacked
// vertically, so this shader animates the UV to go right as the particle ages and
// it moves the UV down with instance ID so we get randomized particles.
float4 main(input i) : SV_Target0 {
    uint seed = hash(i.id);
    float2 uv = i.uv;
    uv.x += SPRITESHEET_CELL.x * 0.5 * floor(i.age * 6.0);
    uv.y += SPRITESHEET_CELL.y * 0.5 * floor(random(seed) * 4.0);
    float4 col = _Texture.Sample(_Sampler, uv) * i.color;
    col.a = saturate(col.a);
    return col;
}
