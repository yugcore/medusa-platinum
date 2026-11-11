#include "../common/ze_binding_spaces.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D ssrTrace : register(t0);
Texture2D sceneColor : register(t1);
SamplerState linearSampler : register(s0);

float4 PSMain(PSInput input) : SV_Target {
    float4 traceResult = ssrTrace.Sample(linearSampler, input.uv);
    float2 hitUV = traceResult.xy;
    float confidence = traceResult.z;
    
    if (confidence <= 0.0f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    
    float3 reflectedColor = sceneColor.SampleLevel(linearSampler, hitUV, 0).rgb;
    return float4(reflectedColor * confidence, confidence);
}
