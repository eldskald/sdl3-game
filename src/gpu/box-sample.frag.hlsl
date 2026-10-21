Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

cbuffer Uniforms : register(b0, space3) {
    // How many of `_Texture`'s texels away the four samples are. `1.0` when
    // downsampling and `0.5` when upsampling.
    float _Delta;

    // Multiplies the returned color. `1.0` when downsampling and set per level
    // when upsampling, to control how much each level contributes.
    float _Intensity;
};

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// Averages four bilinear samples `_Delta` texels away diagonally, so with
// linear filtering it blurs a box of texels around `uv`, then multiplies it by
// `_Intensity`.
float4 main(input i) : SV_Target0 {
    uint width, height;
    _Texture.GetDimensions(width, height);
    float2 texel_size = float2(1.0 / width, 1.0 / height);
    float4 offset = texel_size.xyxy * float2(-_Delta, _Delta).xxyy;

    return (_Texture.Sample(_Sampler, i.uv + offset.xy) +
            _Texture.Sample(_Sampler, i.uv + offset.zy) +
            _Texture.Sample(_Sampler, i.uv + offset.xw) +
            _Texture.Sample(_Sampler, i.uv + offset.zw)) *
           0.25 * _Intensity;
}
