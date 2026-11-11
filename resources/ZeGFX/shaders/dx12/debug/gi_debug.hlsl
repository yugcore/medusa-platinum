#include "../common/ze_binding_spaces.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D<float4> IrradianceAtlas : register(t0);
SamplerState PointSampler : register(s0);

cbuffer DebugCB : register(b0) {
    uint debugMode;
};

float4 PSMain(PSInput input) : SV_Target {
    float3 col = IrradianceAtlas.Sample(PointSampler, input.uv).rgb;
    return float4(col, 1.0f);
}
