Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(input i) : SV_Target0 {
    float4 col = _Texture.Sample(_Sampler, i.uv) * i.color;
    col.a = saturate(col.a);
    return col;
}
