#include "../common/ze_gi_common.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D<float4> rayRadianceDistance : register(t0);
Texture2D<float4> irradianceHistory : register(t1);
SamplerState linearSampler : register(s0);

cbuffer UpdateCB : register(b0) {
    uint raysPerProbe;
    float hysteresis;
};

float4 PSMain(PSInput input) : SV_Target {
    // Determine probe index and local octahedral coordinate from UV
    uint probeIndex = uint(input.uv.y * 32.0f); // Mock translation
    
    // Decodes direction for this texel in the octahedral map
    float3 dir = OctDecode(input.uv);
    
    // Accumulate ray samples matching this texel's cosine hemisphere
    float3 accumulatedRadiance = float3(0.f, 0.f, 0.f);
    float totalWeight = 0.f;
    
    for (uint r = 0; r < raysPerProbe; ++r) {
        float4 rayVal = rayRadianceDistance[uint2(r, probeIndex)];
        float3 rayDir = float3(0.f, 0.f, 1.f); // Re-derived direction
        float weight = max(dot(rayDir, dir), 0.0f);
        accumulatedRadiance += rayVal.rgb * weight;
        totalWeight += weight;
    }
    
    float3 newIrradiance = (totalWeight > 0.0f) ? (accumulatedRadiance / totalWeight) : float3(0.f, 0.f, 0.f);
    float3 oldIrradiance = irradianceHistory.Sample(linearSampler, input.uv).rgb;
    
    return float4(lerp(newIrradiance, oldIrradiance, hysteresis), 1.0f);
}
