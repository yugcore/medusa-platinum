#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D aoRaw : register(t0);
Texture2D depthBuffer : register(t1);
SamplerState pointSampler : register(s0);

float4 PSMain(PSInput input) : SV_Target {
    float centerAO = aoRaw.Sample(pointSampler, input.uv).r;
    float centerDepth = depthBuffer.Sample(pointSampler, input.uv).r;
    
    if (centerDepth >= 1.0f) {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    float totalAO = centerAO;
    float totalWeight = 1.0f;
    
    // 5x5 Bilateral Filter
    [unroll]
    for (int x = -2; x <= 2; ++x) {
        [unroll]
        for (int y = -2; y <= 2; ++y) {
            if (x == 0 && y == 0) continue;
            
            float2 offset = float2(x, y) * renderSizeInvSize.zw;
            float sampleAO = aoRaw.Sample(pointSampler, input.uv + offset).r;
            float sampleDepth = depthBuffer.Sample(pointSampler, input.uv + offset).r;
            
            // Bilateral weights: spatial + depth similarity
            float depthDiff = abs(sampleDepth - centerDepth);
            float weight = exp(-float(x*x + y*y)/4.0f) * exp(-depthDiff * 100.0f);
            
            totalAO += sampleAO * weight;
            totalWeight += weight;
        }
    }
    
    float filteredAO = totalAO / totalWeight;
    return float4(filteredAO, filteredAO, filteredAO, 1.0f);
}
