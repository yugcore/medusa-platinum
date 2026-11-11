#include "../common/ze_binding_spaces.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D currentHistory : register(t0);
SamplerState pointSampler : register(s0);

float4 PSMain(PSInput input) : SV_Target {
    float3 value = currentHistory.Sample(pointSampler, input.uv).rgb;
    float luma = dot(value, float3(0.2126f, 0.7152f, 0.0722f));
    
    // Output: first moment, second moment (E[x], E[x2])
    return float4(luma, luma * luma, 0.0f, 1.0f);
}
