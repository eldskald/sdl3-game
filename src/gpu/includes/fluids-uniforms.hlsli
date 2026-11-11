#ifndef FLUIDS_UNIFORMS
#define FLUIDS_UNIFORMS

StructuredBuffer<float4> _Colors : register(t3, space2);

cbuffer Uniforms : register(b0, space3) {
    float2 _CamPos;
    float _Time;
    float _Refraction;
};

#endif
