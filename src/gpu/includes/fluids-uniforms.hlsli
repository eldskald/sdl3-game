#ifndef FLUIDS_UNIFORMS
#define FLUIDS_UNIFORMS

cbuffer Uniforms : register(b0, space3) {
    // Colors of each fluid indexed by `fluid_type`. Size must be
    // `FLUIDS_TOTAL`.
    float4 _Colors[2];

    // Camera position in uv units, so world pixels over the screen's size.
    // Effects anchored to the world, like lava's noise, add it to `uv`. It's
    // before `_Time` so both are laid out the same on SPIR-V and DXIL.
    float2 _CamPos;

    // Seconds since the game started.
    float _Time;

    // How far, in pixels, a fully tilted normal offsets what's seen through
    // the fluid.
    float _Refraction;
};

#endif
