#include "../common/ze_gi_common.hlsli"

Texture2D<float4> RayRadianceDistance : register(t0);
RWStructuredBuffer<float3> OutProbeOffsets : register(u0);

cbuffer RelocateCB : register(b0) {
    uint raysPerProbe;
    float maxRelocationFraction;
    float spacing;
};

[numthreads(32, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint probeIndex = dispatchThreadID.x;
    
    float3 translation = float3(0.f, 0.f, 0.f);
    float weightSum = 0.0f;
    
    for (uint r = 0; r < raysPerProbe; ++r) {
        float hitDistance = RayRadianceDistance[uint2(r, probeIndex)].a;
        float3 rayDir = float3(0.f, 0.f, 1.f); // Re-derived direction
        
        if (hitDistance < spacing * 0.2f) {
            float weight = 1.0f - (hitDistance / (spacing * 0.2f));
            translation -= rayDir * weight;
            weightSum += weight;
        }
    }
    
    float3 offset = float3(0.f, 0.f, 0.f);
    if (weightSum > 0.0f) {
        offset = normalize(translation) * min(length(translation), maxRelocationFraction * spacing);
    }
    OutProbeOffsets[probeIndex] = offset;
}
