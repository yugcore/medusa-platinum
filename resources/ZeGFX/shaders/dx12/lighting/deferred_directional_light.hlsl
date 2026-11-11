#include "../common/ze_gbuffer.hlsli"
#include "../common/ze_brdf.hlsli"

Texture2D<float4> gbufferATex : register(t0);
Texture2D<float4> gbufferBTex : register(t1);
Texture2D<uint4>  gbufferCTex : register(t2);
Texture2D<float4> gbufferDTex : register(t3);
Texture2D<float>  depthTex : register(t4);
SamplerState      pointSampler : register(s0);

cbuffer CameraParams : register(b0, space0) {
    float4x4 gInvViewProj;
    float3 gCameraPos;
};

cbuffer LightParams : register(b0, space1) {
    float3 gLightDir;
    float gLightIlluminance;
    float3 gLightColor;
};

struct PSInput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 invViewProj) {
    float4 ndc = float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
    float4 world = mul(invViewProj, ndc);
    return world.xyz / world.w;
}

float4 PSMain(PSInput input) : SV_Target {
    float depth = depthTex.SampleLevel(pointSampler, input.uv, 0).r;
    
    // Check if background (clear depth 0.0 or 1.0 depending on convention)
    // We assume 0.0 clearing depth for reversed Z, or check if G-Buffer shading model is valid
    float4 gA = gbufferATex.SampleLevel(pointSampler, input.uv, 0);
    float4 gB = gbufferBTex.SampleLevel(pointSampler, input.uv, 0);
    uint4 gC = gbufferCTex.Load(int3(input.pos.xy, 0));
    float4 gD = gbufferDTex.SampleLevel(pointSampler, input.uv, 0);
    
    ZeGBufferData data = ZeDecodeGBufferUInt(gA, gB, gC, gD);
    
    if (data.shadingModel == 0) { // Unlit
        return float4(data.baseColor + data.emissive, 1.0f);
    }
    
    float3 worldPos = ReconstructWorldPosition(input.uv, depth, gInvViewProj);
    float3 V = normalize(gCameraPos - worldPos);
    float3 L = normalize(-gLightDir);
    float3 N = normalize(data.worldNormal);
    
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    float3 directLight = EvaluateDefaultLitBRDF(L, V, N, data.baseColor, data.perceptualRoughness, data.metallic, F0);
    
    float3 lightOutput = directLight * gLightColor * gLightIlluminance * max(dot(N, L), 0.0f) + data.emissive;
    return float4(lightOutput, 1.0f);
}
