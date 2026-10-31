cbuffer Uniforms : register(b0, space1) {
    // Size, in pixels, of the texture being drawn on.
    float2 _TargetSize;
};

struct input {
    float2 pos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
};

// Every fragment shader takes `color` and `uv` in this order, so they line up
// on both SPIR-V and DXIL.
struct output {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 pos : SV_Position;
};

// Positions are in pixels on the target, from its top left, and colors are
// passed through as is, so they can go beyond `1.0` to make things bloom.
output main(input i) {
    output o;
    o.color = i.color;
    o.uv = i.uv;
    float2 ndc = i.pos / _TargetSize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    o.pos = float4(ndc, 0.0, 1.0);
    return o;
}
