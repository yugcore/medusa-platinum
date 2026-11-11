#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D aoMask : register(t0);
Texture2D depthBuffer : register(t1);
SamplerState pointSampler : register(s0);

float4 PSMain(PSInput input) : SV_Target {
    float centerVal = aoMask.Sample(pointSampler, input.uv).r;
    float centerDepth = depthBuffer.Sample(pointSampler, input.uv).r;
    
    if (centerDepth >= 1.0f) {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    float totalAO = centerVal;
    float totalWeight = 1.0f;
    
    [unroll]
    for (int x = -2; x <= 2; ++x) {
        [unroll]
        for (int y = -2; y <= 2; ++y) {
            if (x == 0 && y == 0) continue;
            
            float2 offset = float2(x, y) * renderSizeInvSize.zw;
            float sampleAO = aoMask.Sample(pointSampler, input.uv + offset).r;
            float sampleDepth = depthBuffer.Sample(pointSampler, input.uv + offset).r;
            
            float depthDiff = abs(sampleDepth - centerDepth);
            float weight = exp(-float(x*x + y*y)/4.0f) * exp(-depthDiff * 100.0f);
            
            totalAO += sampleAO * weight;
            totalWeight += weight;
        }
    }
    
    float filtered = totalAO / totalWeight;
    return float4(filtered, filtered, filtered, 1.0f);
}
