#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D reflectionRadiance : register(t0);
Texture2D depthBuffer : register(t1);
Texture2D normalMap : register(t2);
SamplerState pointSampler : register(s0);

cbuffer SpatialSettings : register(b0) {
    float roughness;
};

float4 PSMain(PSInput input) : SV_Target {
    float4 centerVal = reflectionRadiance.Sample(pointSampler, input.uv);
    float centerDepth = depthBuffer.Sample(pointSampler, input.uv).r;
    float3 centerNorm = normalize(normalMap.Sample(pointSampler, input.uv).xyz * 2.0f - 1.0f);
    
    if (centerDepth >= 1.0f) {
        return float4(0.f, 0.f, 0.f, 0.f);
    }
    
    float3 totalColor = centerVal.rgb;
    float totalWeight = 1.0f;
    
    // Scale filter size based on roughness
    int filterSize = roughness > 0.4f ? 3 : 1;
    
    for (int x = -filterSize; x <= filterSize; ++x) {
        for (int y = -filterSize; y <= filterSize; ++y) {
            if (x == 0 && y == 0) continue;
            
            float2 offset = float2(x, y) * renderSizeInvSize.zw;
            float4 sampleVal = reflectionRadiance.Sample(pointSampler, input.uv + offset);
            float sampleDepth = depthBuffer.Sample(pointSampler, input.uv + offset).r;
            float3 sampleNorm = normalize(normalMap.Sample(pointSampler, input.uv + offset).xyz * 2.0f - 1.0f);
            
            float depthDiff = abs(sampleDepth - centerDepth);
            float normDiff = dot(centerNorm, sampleNorm);
            
            if (depthDiff < 0.05f && normDiff > 0.9f) {
                // Firefly clamp: discard extreme values
                float luma = dot(sampleVal.rgb, float3(0.2126f, 0.7152f, 0.0722f));
                float centerLuma = dot(centerVal.rgb, float3(0.2126f, 0.7152f, 0.0722f));
                if (luma < centerLuma * 10.0f) {
                    float weight = exp(-float(x*x + y*y));
                    totalColor += sampleVal.rgb * weight;
                    totalWeight += weight;
                }
            }
        }
    }
    
    return float4(totalColor / totalWeight, centerVal.a);
}
