Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

cbuffer Uniforms : register(b0, space3) {
    // Multiplies the texture's color. Can go beyond `1.0` to make it bloom.
    float4 _Tint;
};

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// Draws the texture multiplied by `_Tint`, as well as the texture's color and
// alpha mods like SDL does. Alpha is kept in `[0, 1]` so blending still works.
float4 main(input i) : SV_Target0 {
    float4 col = _Texture.Sample(_Sampler, i.uv) * i.color * _Tint;
    col.a = saturate(col.a);
    return col;
}
