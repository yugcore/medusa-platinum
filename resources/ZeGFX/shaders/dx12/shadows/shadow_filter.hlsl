#include "../common/ze_binding_spaces.hlsli"
#include "../common/ze_shadow_common.hlsli"

struct VSInput {
    float2 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D shadowAtlas : register(t0);
SamplerState pointSampler : register(s0);

PSInput VSMain(uint vertexID : SV_VertexID) {
    PSInput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 PSMain(PSInput input) : SV_Target {
    float depth = shadowAtlas.Sample(pointSampler, input.uv).r;
    return float4(depth, depth, depth, 1.0f);
}
