#define EPSILON 0.0001

Texture2D<float4> _Texture : register(t0, space2);
SamplerState _Sampler : register(s0, space2);

cbuffer Uniforms : register(b0, space3) {
    // Brightness, on the brightest channel, above which colors bloom.
    float _Threshold;

    // How soft the transition into blooming is, from `0.0`, a hard cut at
    // `_Threshold`, to `1.0`, starting to bloom from zero brightness.
    float _SoftThreshold;
};

struct input {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// Keeps only how much each pixel is brighter than `_Threshold`, keeping its
// hue, for the bloom to blur. Around the threshold, within `knee` of it, the
// contribution eases in on a quadratic curve instead of cutting in hard.
float4 main(input i) : SV_Target0 {
    float4 col = _Texture.Sample(_Sampler, i.uv);
    float brightness = max(max(col.r, col.g), col.b);

    float knee = _Threshold * _SoftThreshold;
    float soft = brightness - _Threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);

    float contribution =
        max(soft, brightness - _Threshold) / max(EPSILON, brightness);
    return float4(contribution * col.rgb, 1.0);
}
