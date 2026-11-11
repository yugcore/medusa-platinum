#include "../common/ze_gi_common.hlsli"

Texture2D<float4> RayRadianceDistance : register(t0);
RWByteAddressBuffer OutProbeState : register(u0);

cbuffer ClassifyCB : register(b0) {
    uint raysPerProbe;
    float geometryThreshold;
};

[numthreads(32, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint probeIndex = dispatchThreadID.x;
    
    uint insideCount = 0;
    for (uint r = 0; r < raysPerProbe; ++r) {
        float hitDistance = RayRadianceDistance[uint2(r, probeIndex)].a;
        if (hitDistance < geometryThreshold) {
            insideCount++;
        }
    }
    
    // If >50% rays are extremely short, classify as InsideGeometry (state = 1)
    uint state = (insideCount > raysPerProbe / 2) ? 1 : 0;
    OutProbeState.Store(probeIndex * 4, state);
}
