#include "../common/ze_binding_spaces.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D sceneColor : register(t0);
Texture2D bloomTex : register(t1);
SamplerState linearSampler : register(s0);

cbuffer TonemapCB : register(b0) {
    float exposureMultiplier;
    float contrast;
    float saturation;
    float temperature;
    float tint;
    uint toneMapOperator; // 0 = neutral/clamp, 1 = Reinhard, 2 = ACES Filmic
    bool bloomEnabled;
};

// ACES Filmic Tone Mapping Curve Approximation
float3 ACESFilmic(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Simple Reinhard Tone Mapping
float3 Reinhard(float3 x) {
    return x / (x + float3(1.0f, 1.0f, 1.0f));
}

// White balance / color temperature adjustments
float3 ApplyColorGrading(float3 color) {
    // Contrast
    color = saturate((color - 0.5f) * contrast + 0.5f);
    
    // Saturation
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(float3(luma, luma, luma), color, saturation);
    
    // Simple temperature adjustment
    color.r += temperature * 0.05f;
    color.b -= temperature * 0.05f;
    color.g += tint * 0.02f;
    
    return max(color, 0.0f);
}

float4 PSMain(PSInput input) : SV_Target {
    float3 hdrColor = sceneColor.Sample(linearSampler, input.uv).rgb;
    
    if (bloomEnabled) {
        float3 bloomColor = bloomTex.Sample(linearSampler, input.uv).rgb;
        hdrColor += bloomColor;
    }
    
    // Apply exposure
    hdrColor *= exposureMultiplier;
    
    // Apply White Balance & Color Grading
    hdrColor = ApplyColorGrading(hdrColor);
    
    // Apply Tone Mapping operator
    float3 ldrColor = hdrColor;
    if (toneMapOperator == 1) {
        ldrColor = Reinhard(hdrColor);
    } else if (toneMapOperator == 2) {
        ldrColor = ACESFilmic(hdrColor);
    } else {
        ldrColor = saturate(hdrColor);
    }
    
    // Encode to sRGB gamma space
    float3 finalColor = pow(ldrColor, 1.0f / 2.2f);
    
    return float4(finalColor, 1.0f);
}
