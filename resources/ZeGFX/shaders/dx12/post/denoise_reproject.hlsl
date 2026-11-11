#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D currentSignal : register(t0);
Texture2D previousSignalHistory : register(t1);
Texture2D motionVectors : register(t2);
Texture2D currentDepth : register(t3);
Texture2D previousDepthHistory : register(t4);
Texture2D currentNormal : register(t5);
Texture2D previousNormalHistory : register(t6);
SamplerState linearSampler : register(s0);
SamplerState pointSampler : register(s1);

cbuffer ReprojectSettings : register(b0) {
    float maxHistoryWeight;
    float depthTolerance;
    float normalTolerance;
    float padding;
};

float4 PSMain(PSInput input) : SV_Target {
    float2 motion = motionVectors.Sample(pointSampler, input.uv).xy;
    float2 prevUV = input.uv - motion;
    
    if (prevUV.x < 0.0f || prevUV.x > 1.0f || prevUV.y < 0.0f || prevUV.y > 1.0f) {
        return float4(currentSignal.Sample(pointSampler, input.uv).rgb, 0.0f); // Rejection: weight = 0
    }
    
    // Depth check
    float currDepthVal = currentDepth.Sample(pointSampler, input.uv).r;
    float prevDepthVal = previousDepthHistory.Sample(pointSampler, prevUV).r;
    if (abs(currDepthVal - prevDepthVal) > depthTolerance) {
        return float4(currentSignal.Sample(pointSampler, input.uv).rgb, 0.0f); // Reject disocclusions
    }
    
    // Normal check
    float3 currNorm = normalize(currentNormal.Sample(pointSampler, input.uv).xyz * 2.0f - 1.0f);
    float3 prevNorm = normalize(previousNormalHistory.Sample(pointSampler, prevUV).xyz * 2.0f - 1.0f);
    if (dot(currNorm, prevNorm) < normalTolerance) {
        return float4(currentSignal.Sample(pointSampler, input.uv).rgb, 0.0f); // Reject orientation changes
    }
    
    float3 current = currentSignal.Sample(pointSampler, input.uv).rgb;
    float4 history = previousSignalHistory.Sample(linearSampler, prevUV);
    float weight = min(history.a + 1.0f, maxHistoryWeight);
    
    float3 blended = lerp(current, history.rgb, 1.0f / weight);
    return float4(blended, weight);
}
