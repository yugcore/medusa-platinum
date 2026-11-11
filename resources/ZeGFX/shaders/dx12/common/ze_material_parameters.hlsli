#ifndef ZE_MATERIAL_PARAMETERS_HLSLI
#define ZE_MATERIAL_PARAMETERS_HLSLI

#include "ze_binding_spaces.hlsli"

cbuffer MaterialParameters : register(b0, SPACE_MATERIAL)
{
    float4 objectColor;
    float metallic;
    float roughness;
    float emissiveStrength;
    float hasNormalMap;
    float hasBaseColorTexture;
    float hasMetallicRoughnessTexture;
    float hasEmissiveMap;
    float hasOcclusionTexture;
    float4 emissiveColor;
    float alphaCutoff;
    float alphaMode;
    float doubleSided;
    float hasLightmap;
    float lightmapIntensity;
    float lightmapOcclusionBlend;
    float3 probeAmbient;
    float probeAmbientIntensity;
};

#endif // ZE_MATERIAL_PARAMETERS_HLSLI
