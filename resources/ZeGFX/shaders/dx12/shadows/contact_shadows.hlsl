#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D depthBuffer : register(t0);
SamplerState pointSampler : register(s0);

cbuffer ContactShadowSettings : register(b0) {
    float3 lightDirection;
    float maxWorldDistance;
    float thickness;
    float bias;
    float intensity;
};

float4 PSMain(PSInput input) : SV_Target {
    float depth = depthBuffer.Sample(pointSampler, input.uv).r;
    if (depth >= 1.0f) {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    // Screen-space contact shadow tracing
    float shadowFactor = 1.0f;
    float3 rayPos = float3(input.uv, depth);
    float3 rayDir = mul(float4(lightDirection, 0.0f), projection).xyz;
    rayDir = normalize(rayDir);
    
    for (int i = 0; i < 16; ++i) {
        rayPos += rayDir * 0.01f;
        if (rayPos.x < 0.0f || rayPos.x > 1.0f || rayPos.y < 0.0f || rayPos.y > 1.0f) {
            break;
        }
        float sampleDepth = depthBuffer.Sample(pointSampler, rayPos.xy).r;
        if (sampleDepth < rayPos.z - bias && sampleDepth > rayPos.z - thickness) {
            shadowFactor = 1.0f - intensity;
            break;
        }
    }
    return float4(shadowFactor, shadowFactor, shadowFactor, 1.0f);
}
