Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

float4 main(float2 UV : TEXCOORD0) : SV_Target0
{
    return _Texture.Sample(_Sampler, UV);
}
