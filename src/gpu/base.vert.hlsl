struct input {
    float3 pos : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

struct output {
    float4 POS : SV_Position;
    float2 UV : TEXCOORD0;
};

output main(input i) {
    output o;
    o.POS = float4(i.pos, 1.0f);
    o.UV = i.uv;

    return o;
}
