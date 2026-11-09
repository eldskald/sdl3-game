cbuffer TargetUniforms : register(b0, space1) {
    float2 _TargetSize;
    int2 _TargetPad0;
};

struct input {
    float2 pos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
};

struct output {
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 pos : SV_Position;
};

output main(input i) {
    output o;
    o.color = i.color;
    o.uv = i.uv;
    float2 ndc = i.pos / _TargetSize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    o.pos = float4(ndc, 0.0, 1.0);
    return o;
}
