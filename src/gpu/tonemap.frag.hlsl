Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(input i) : SV_Target0 {
    float3 col = max(_Texture.Sample(_Sampler, i.uv).rgb, 0.0);
    float peak = max(max(col.r, col.g), max(col.b, 1.0));
    col = lerp(col / peak, 1.0, 1.0 - 1.0 / peak);
    return float4(col, 1.0);
}
