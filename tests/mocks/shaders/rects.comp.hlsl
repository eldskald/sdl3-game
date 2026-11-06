// Writes the first `_Count` rects of `_Src` moved by `_Offset` on `_Rects`, and
// the indirect draw of a quad for each of them on `_Args`, as an
// `SDL_GPUIndirectDrawCommand`.

StructuredBuffer<float4> _Src : register(t0, space0);
RWStructuredBuffer<float4> _Rects : register(u0, space1);
RWStructuredBuffer<uint> _Args : register(u1, space1);

cbuffer Uniforms : register(b0, space2) {
    float2 _Offset;
    uint _Count;
    uint _Pad0;
};

[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    if (id.x >= _Count) return;
    _Rects[id.x] = _Src[id.x] + float4(_Offset, 0.0, 0.0);
    if (id.x != 0) return;
    _Args[0] = 6;
    _Args[1] = _Count;
    _Args[2] = 0;
    _Args[3] = 0;
}
