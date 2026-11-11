#include "../common/ze_gi_common.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D<float4> rayRadianceDistance : register(t0);
Texture2D<float4> distanceHistory : register(t1);
SamplerState linearSampler : register(s0);

cbuffer UpdateCB : register(b0) {
    uint raysPerProbe;
    float hysteresis;
};

float4 PSMain(PSInput input) : SV_Target {
    uint probeIndex = uint(input.uv.y * 32.0f);
    float3 dir = OctDecode(input.uv);
    
    float totalDistance = 0.0f;
    float totalDistanceSq = 0.0f;
    float totalWeight = 0.0f;
    
    for (uint r = 0; r < raysPerProbe; ++r) {
        float4 rayVal = rayRadianceDistance[uint2(r, probeIndex)];
        float3 rayDir = float3(0.f, 0.f, 1.f);
        float weight = max(dot(rayDir, dir), 0.0f);
        
        float d = rayVal.a;
        totalDistance += d * weight;
        totalDistanceSq += d * d * weight;
        totalWeight += weight;
    }
    
    float2 newMoments = (totalWeight > 0.0f) ? float2(totalDistance / totalWeight, totalDistanceSq / totalWeight) : float2(0.f, 0.f);
    float2 oldMoments = distanceHistory.Sample(linearSampler, input.uv).rg;
    
    return float4(lerp(newMoments, oldMoments, hysteresis), 0.0f, 1.0f);
}
