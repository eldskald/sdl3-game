// Draws a quad for each rect on `_Rects`, as `x,y,w,h` in pixels, colored by
// `_Color`, with the same outputs as `src/gpu/base.vert.hlsl` so it draws with
// the base fragment shader.

StructuredBuffer<float4> _Rects : register(t0, space0);

cbuffer TargetUniforms : register(b0, space1) {
    float2 _TargetSize;
    int2 _TargetPad0;
};

cbuffer Uniforms : register(b1, space1) {
    float4 _Color;
};

struct output {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 pos : SV_Position;
};

// Corners of the quad, in the same order as `GRAPHICS_quad()`.
static const float2 CORNERS[6] = {
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0),
};

output main(uint vertex: SV_VertexID, uint instance: SV_InstanceID) {
    output o;
    float4 rect = _Rects[instance];
    float2 corner = CORNERS[vertex];
    float2 pos = rect.xy + corner * rect.zw;
    o.color = _Color;
    o.uv = corner;
    float2 ndc = pos / _TargetSize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    o.pos = float4(ndc, 0.0, 1.0);
    return o;
}
