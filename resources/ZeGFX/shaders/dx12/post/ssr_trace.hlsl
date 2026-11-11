#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D depthBuffer : register(t0);
Texture2D normalMap : register(t1);
SamplerState pointSampler : register(s0);

cbuffer SSRSettings : register(b0) {
    uint maxSteps;
    float stepSize;
    float roughnessCutoff;
    float padding;
};

float3 GetViewPos(float2 uv, float depth) {
    float4 clipPos = float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
    float4 viewPos = mul(clipPos, inverseProjection);
    return viewPos.xyz / viewPos.w;
}

float4 PSMain(PSInput input) : SV_Target {
    float depth = depthBuffer.Sample(pointSampler, input.uv).r;
    if (depth >= 1.0f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f); // Confidence = 0
    }
    
    float3 P = GetViewPos(input.uv, depth);
    float3 N = normalize(normalMap.Sample(pointSampler, input.uv).xyz * 2.0f - 1.0f);
    N = mul(float4(N, 0.0f), view).xyz; // View space normal
    
    float3 V = normalize(P);
    float3 R = reflect(V, N);
    
    if (R.z > 0.0f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f); // Ray points away from camera
    }
    
    // Step ray in view space
    float3 rayPos = P;
    float3 rayStep = R * stepSize;
    
    float2 hitUV = float2(0.0f, 0.0f);
    float hitMask = 0.0f;
    
    for (uint i = 0; i < maxSteps; ++i) {
        rayPos += rayStep;
        
        // Transform to screen space
        float4 clipPos = mul(float4(rayPos, 1.0f), projection);
        float3 screenPos = clipPos.xyz / clipPos.w;
        float2 uv = screenPos.xy * 0.5f + 0.5f;
        uv.y = 1.0f - uv.y;
        
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
            break;
        }
        
        float sampleDepth = depthBuffer.Sample(pointSampler, uv).r;
        float3 sampleViewPos = GetViewPos(uv, sampleDepth);
        
        if (rayPos.z < sampleViewPos.z && abs(rayPos.z - sampleViewPos.z) < 1.0f) {
            // Binary search refinement
            float3 startPos = rayPos - rayStep;
            float3 endPos = rayPos;
            for (int b = 0; b < 4; ++b) {
                float3 midPos = (startPos + endPos) * 0.5f;
                float4 midClip = mul(float4(midPos, 1.0f), projection);
                float3 midScreen = midClip.xyz / midClip.w;
                float2 midUV = midScreen.xy * 0.5f + 0.5f;
                midUV.y = 1.0f - midUV.y;
                
                float mDepth = depthBuffer.Sample(pointSampler, midUV).r;
                float3 mViewPos = GetViewPos(midUV, mDepth);
                
                if (midPos.z < mViewPos.z) {
                    endPos = midPos;
                } else {
                    startPos = midPos;
                }
            }
            
            float4 finalClip = mul(float4(endPos, 1.0f), projection);
            float3 finalScreen = finalClip.xyz / finalClip.w;
            hitUV = finalScreen.xy * 0.5f + 0.5f;
            hitUV.y = 1.0f - hitUV.y;
            hitMask = 1.0f;
            break;
        }
    }
    
    // Compute confidence based on edge distance
    float edgeDist = min(min(hitUV.x, 1.0f - hitUV.x), min(hitUV.y, 1.0f - hitUV.y));
    float confidence = saturate(edgeDist * 10.0f) * hitMask;
    
    return float4(hitUV, confidence, 1.0f);
}
