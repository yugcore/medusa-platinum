#ifndef SURFACE_DATA_HLSLI
#define SURFACE_DATA_HLSLI

// Core surface representation used across deferred passes
struct SurfaceData
{
    float3 BaseColor;
    float3 NormalWS;
    float  Metallic;
    float  PerceptualRoughness;
    float  AmbientOcclusion;
    float3 Emissive;
    uint   ShadingModelID;
    float2 Velocity;
};

// Layout encoding for MRT output
struct GBufferOutput
{
    float4 RT0 : SV_Target0; // BaseColor.rgb, Roughness (a)
    float4 RT1 : SV_Target1; // NormalWS.xyz, Metallic (a)
    float4 RT2 : SV_Target2; // Emissive.rgb, AmbientOcclusion (a)
    float4 RT3 : SV_Target3; // Velocity.xy, ShadingModelID (z, w unused)
};

#endif
