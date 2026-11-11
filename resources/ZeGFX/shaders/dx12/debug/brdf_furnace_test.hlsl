#include "../common/ze_brdf.hlsli"

cbuffer FurnaceParams : register(b0, space0) {
    float gRoughness;
    float gMetallic;
};

struct PSInput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

float4 PSMain(PSInput input) : SV_Target {
    float3 V = float3(0.0f, 0.0f, 1.0f);
    float3 N = float3(0.0f, 0.0f, 1.0f);
    float3 baseColor = float3(1.0f, 1.0f, 1.0f);
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    
    float3 totalReflected = float3(0.0f, 0.0f, 0.0f);
    const uint SAMPLES = 128;
    for (uint i = 0; i < SAMPLES; ++i) {
        float u = float(i) / float(SAMPLES);
        // Simple LCG random sequence for phi
        uint temp = i * 1664525u + 1013904223u;
        float v = float(temp & 0xFFFFu) / 65536.0f;
        
        float phi = 2.0f * PI * u;
        float cosTheta = sqrt(v);
        float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
        
        float3 L = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
        
        float NoL = max(dot(N, L), 0.0f);
        if (NoL > 0.0f) {
            float3 brdf = EvaluateDefaultLitBRDF(L, V, N, baseColor, gRoughness, gMetallic, F0);
            totalReflected += brdf * PI / float(SAMPLES);
        }
    }
    
    return float4(totalReflected, 1.0f);
}
