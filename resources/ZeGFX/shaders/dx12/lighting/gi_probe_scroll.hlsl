#include "../common/ze_gi_common.hlsli"

RWStructuredBuffer<float3> ProbeOffsets : register(u0);
RWByteAddressBuffer ProbeStates : register(u1);

cbuffer ScrollCB : register(b0) {
    int3 scrollOffset;
    uint3 probeCounts;
};

[numthreads(32, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint probeIndex = dispatchThreadID.x;
    uint totalProbes = probeCounts.x * probeCounts.y * probeCounts.z;
    if (probeIndex >= totalProbes) return;
    
    // Snaps and checks if this cell was shifted into the newly exposed slab
    uint3 coord;
    uint slices = probeCounts.x * probeCounts.y;
    coord.z = probeIndex / slices;
    uint rem = probeIndex % slices;
    coord.y = rem / probeCounts.x;
    coord.x = rem % probeCounts.x;
    
    int3 oldCoord = int3(coord) - scrollOffset;
    
    if (oldCoord.x < 0 || oldCoord.x >= (int)probeCounts.x ||
        oldCoord.y < 0 || oldCoord.y >= (int)probeCounts.y ||
        oldCoord.z < 0 || oldCoord.z >= (int)probeCounts.z) 
    {
        // Newly exposed probe: reset offsets and set state to NewlyExposed (state = 3)
        ProbeOffsets[probeIndex] = float3(0.f, 0.f, 0.f);
        ProbeStates.Store(probeIndex * 4, 3);
    }
}
