#include "../common/ze_gbuffer.hlsli"
#include "../common/ze_brdf.hlsli"
#include "../common/ze_light_types.hlsli"

Texture2D<float4> gbufferATex : register(t0);
Texture2D<float4> gbufferBTex : register(t1);
Texture2D<uint4>  gbufferCTex : register(t2);
Texture2D<float4> gbufferDTex : register(t3);
Texture2D<float>  depthTex : register(t4);

StructuredBuffer<GpuLocalLight> localLights : register(t5);
StructuredBuffer<uint> tileLightIndexBuffer : register(t6);
StructuredBuffer<uint2> tileLightOffsetCountBuffer : register(t7); // x: offset, y: count

SamplerState pointSampler : register(s0);

cbuffer CameraParams : register(b0, space0) {
    float4x4 gInvViewProj;
    float3 gCameraPos;
    float gCameraPadding;
    float2 gViewportSize;
    uint gNumTilesX;
    uint gNumTilesY;
    uint gTileSize;
    uint gMaxLightsPerTile;
    uint gNumLocalLights;
    uint gLightGridEnabled;
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
    float4 gA = gbufferATex.SampleLevel(pointSampler, input.uv, 0);
    float4 gB = gbufferBTex.SampleLevel(pointSampler, input.uv, 0);
    uint4 gC = gbufferCTex.Load(int3(input.pos.xy, 0));
    float4 gD = gbufferDTex.SampleLevel(pointSampler, input.uv, 0);
    
    ZeGBufferData data = ZeDecodeGBufferUInt(gA, gB, gC, gD);
    if (data.shadingModel == 0) {
        discard;
    }
    
    float3 worldPos = ReconstructWorldPosition(input.uv, depth, gInvViewProj);
    float3 V = normalize(gCameraPos - worldPos);
    float3 N = normalize(data.worldNormal);
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    
    // Determine tile ID
    uint offset = 0;
    uint count = 0;
    if (gLightGridEnabled != 0 && gNumTilesX > 0 && gNumTilesY > 0) {
        uint2 pixelPos = uint2(input.pos.xy);
        uint2 tileId = min(
            pixelPos / max(gTileSize, 1u),
            uint2(gNumTilesX - 1, gNumTilesY - 1));
        uint tileIdx = tileId.y * gNumTilesX + tileId.x;
        uint2 offsetCount = tileLightOffsetCountBuffer[tileIdx];
        offset = offsetCount.x;
        count = min(offsetCount.y, gMaxLightsPerTile);
    }
    
    float3 accumulatedLight = float3(0.0f, 0.0f, 0.0f);
    
    for (uint i = 0; i < count; ++i) {
        uint lightIdx = tileLightIndexBuffer[offset + i];
        if (lightIdx >= gNumLocalLights) {
            continue;
        }
        GpuLocalLight light = localLights[lightIdx];
        
        float3 lightPos = light.positionRange.xyz;
        float range = light.positionRange.w;
        float3 lightColor = light.colorIntensity.xyz;
        float intensity = light.colorIntensity.w;
        
        float3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        if (dist > range) continue;
        
        float3 L = toLight / dist;
        
        // Attenuation (inverse-square with smooth cutoff)
        float attenuation = saturate(1.0f - pow(dist / range, 4.0f));
        attenuation = (attenuation * attenuation) / (dist * dist + 1.0f);
        
        // Spot cone attenuation
        float type = light.directionType.w;
        if (type == 2.0f) { // Spot light
            float3 spotDir = normalize(light.directionType.xyz);
            float cosAngle = dot(-L, spotDir);
            float innerCone = light.coneRectFlags.x;
            float outerCone = light.coneRectFlags.y;
            float coneAttenuation = saturate((cosAngle - outerCone) / max(innerCone - outerCone, 0.0001f));
            attenuation *= coneAttenuation * coneAttenuation;
        }
        
        float3 radiance = lightColor * intensity * attenuation;
        float3 brdf = EvaluateDefaultLitBRDF(L, V, N, data.baseColor, data.perceptualRoughness, data.metallic, F0);
        
        accumulatedLight += brdf * radiance * max(dot(N, L), 0.0f);
    }
    
    return float4(accumulatedLight, 1.0f);
}
