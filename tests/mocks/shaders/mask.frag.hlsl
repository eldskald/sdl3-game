// Draws the texture multiplied by the vertex color, by `_Mask`, sampled on the
// same UVs, and by `_Tint`.

Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);
Texture2D<float4> _Mask : register(t1, space2);
SamplerState _MaskSampler : register(s1, space2);

cbuffer Uniforms : register(b0, space3) {
    float4 _Tint;
};

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(input i) : SV_Target0 {
    return _Texture.Sample(_Sampler, i.uv) * _Mask.Sample(_MaskSampler, i.uv) *
           i.color * _Tint;
}
