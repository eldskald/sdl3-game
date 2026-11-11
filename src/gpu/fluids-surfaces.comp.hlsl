// Finds the surfaces of the fluids on the camera and appends the particles
// each spawns, from its fluid's `fluid_particles()`, to the particles'
// requests, before the particles' spawn pass spawns them. Dispatched by
// `FLUIDS_prepare()` on `src/engine/fluids.c`, one thread per fluid cell.

#include "includes/fluids.hlsli"
#include "includes/particles-common.hlsli"

// Fluid cells over the camera, a row of `_CellsRect.z` cells after the other,
// encoded by `encode_surface_cell()` on `src/engine/fluids.c`.
StructuredBuffer<uint> _Cells : register(t0, space0);

// The particles' requests and counters, see `PARTICLES_get_requests_buffer()`
// and `PARTICLES_get_counters_buffer()` on `src/engine/particles.h`.
RWStructuredBuffer<Request> _Requests : register(u0, space1);
RWStructuredBuffer<uint> _Counters : register(u1, space1);

// Matches `surface_uniforms` on `src/engine/fluids.c`.
cbuffer SurfaceUniforms : register(b0, space2) {
    // Cell of `_Cells`' first cell, and how many cells wide and tall it is.
    // Its first row is the one above the camera's and its last row the one
    // below it, so the camera's top and bottom cells know what's above and
    // below them. Neither has threads of their own.
    int4 _CellsRect;

    // Size of a fluid cell, in pixels.
    float2 _CellSize;

    // Seconds since the last frame.
    float _Dt;

    // Random every frame, so surfaces spawn differently each frame.
    uint _Seed;

    // Length of `_Requests`, past which requests are dropped.
    uint _MaxRequests;

    int _Pad0;
    int _Pad1;
    int _Pad2;
};

// Cells with no fluid and no wall, see `encode_surface_cell()`.
#define CELL_EMPTY 0

// Type, the `IDX_*` of the lightest fluid in the cell, `0` for none and `255`
// for walls.
int cell_type(uint cell) {
    return cell & 0xFF;
}

// How full the cell is, from `0.0` to `1.0`.
float cell_fill(uint cell) {
    return ((cell >> 8) & 0xFF) / 255.0;
}

[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint w = _CellsRect.z;
    if (id.x >= w * (_CellsRect.w - 2)) return;
    uint x = id.x % w;
    uint y = id.x / w + 1;

    // Surfaces are cells with fluid under completely empty ones. Empty cells
    // and walls have no `IDX_*`, so `fluid_particles()` spawns nothing there.
    if (_Cells[(y - 1) * w + x] != CELL_EMPTY) return;
    uint cell = _Cells[y * w + x];
    int idx = cell_type(cell);

    // The fluid sits at the bottom of the cell, as high as it's full.
    int2 world_cell = _CellsRect.xy + int2(x, y);
    float2 pos = float2(world_cell.x * _CellSize.x,
                        (world_cell.y + 1.0 - cell_fill(cell)) * _CellSize.y);
    uint seed = hash(_Seed ^ hash(asuint(world_cell.x) ^
                                  hash(asuint(world_cell.y))));
    // How much fluid there is at the surface, of any type, counting the cell
    // under it too, so puddles can be told apart from deeper fluid.
    float volume =
        saturate(cell_fill(cell) + cell_fill(_Cells[(y + 1) * w + x]));
    Request r = fluid_particles(idx, pos, _CellSize, volume, _Dt, seed);

    // Most surfaces spawn nothing most frames, so they don't take up requests.
    if (r.count == 0) return;
    uint i;
    InterlockedAdd(_Counters[COUNTER_REQUESTS], 1, i);
    if (i < _MaxRequests) _Requests[i] = r;
}
