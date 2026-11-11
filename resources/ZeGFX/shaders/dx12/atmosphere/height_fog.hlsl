#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D sceneColor : register(t0);
Texture2D depthBuffer : register(t1);
SamplerState pointSampler : register(s0);

cbuffer FogSettings : register(b0) {
    float fogDensity;
    float heightFalloff;
    float cameraHeight;
    float padding;
    float4 inScatteringColor;
};

float3 GetViewPos(float2 uv, float depth) {
    float4 clipPos = float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
    float4 viewPos = mul(clipPos, inverseProjection);
    return viewPos.xyz / viewPos.w;
}

float4 PSMain(PSInput input) : SV_Target {
    float3 color = sceneColor.Sample(pointSampler, input.uv).rgb;
    float depth = depthBuffer.Sample(pointSampler, input.uv).r;
    
    if (depth >= 1.0f) {
        // Fog the sky/far plane slightly
        return float4(lerp(color, inScatteringColor.rgb, 0.5f), 1.0f);
    }
    
    float3 viewPos = GetViewPos(input.uv, depth);
    float distance = length(viewPos);
    
    // Height fog formula
    // integral of density * exp(-heightFalloff * (cameraHeight + t * rayDir.y)) dt
    float rayDirY = viewPos.y / distance;
    float fogAmount = 0.0f;
    
    if (abs(rayDirY) > 0.0001f) {
        fogAmount = (fogDensity / heightFalloff) * exp(-heightFalloff * cameraHeight) * (1.0f - exp(-heightFalloff * viewPos.y)) / rayDirY;
    } else {
        fogAmount = fogDensity * distance * exp(-heightFalloff * cameraHeight);
    }
    
    float transmittance = saturate(exp(-fogAmount));
    float3 finalColor = lerp(inScatteringColor.rgb, color, transmittance);
    
    return float4(finalColor, 1.0f);
}
