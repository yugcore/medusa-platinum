#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D shadowMask : register(t0);
Texture2D depthBuffer : register(t1);
SamplerState pointSampler : register(s0);

float4 PSMain(PSInput input) : SV_Target {
    float centerVal = shadowMask.Sample(pointSampler, input.uv).r;
    float centerDepth = depthBuffer.Sample(pointSampler, input.uv).r;
    
    if (centerDepth >= 1.0f) {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    float totalShadow = centerVal;
    float totalWeight = 1.0f;
    
    [unroll]
    for (int x = -1; x <= 1; ++x) {
        [unroll]
        for (int y = -1; y <= 1; ++y) {
            if (x == 0 && y == 0) continue;
            
            float2 offset = float2(x, y) * renderSizeInvSize.zw;
            float sampleShadow = shadowMask.Sample(pointSampler, input.uv + offset).r;
            float sampleDepth = depthBuffer.Sample(pointSampler, input.uv + offset).r;
            
            float depthDiff = abs(sampleDepth - centerDepth);
            float weight = exp(-float(x*x + y*y)) * exp(-depthDiff * 80.0f);
            
            totalShadow += sampleShadow * weight;
            totalWeight += weight;
        }
    }
    
    float filtered = totalShadow / totalWeight;
    return float4(filtered, filtered, filtered, 1.0f);
}
