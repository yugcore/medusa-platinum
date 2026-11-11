#ifndef ZE_INDIRECT_LIGHTING_HLSLI
#define ZE_INDIRECT_LIGHTING_HLSLI
#include "ze_brdf.hlsli"

// Partition the existing single-scattering split-sum response. Irradiance maps
// in ZeGFX store irradiance/pi, so no additional pi factor belongs here.
float3 ZeIndirectDiffuseWeight(float3 baseColor, float metallic, float3 specularResponse) {
    return baseColor * (1.0f - saturate(metallic)) * (1.0f - saturate(specularResponse));
}

// The fallback sky and baked environments share the same integrated GGX
// response. A raw grazing-angle Fresnel sample ignores roughness and can turn
// dense leaf cards silver. The caller supplies F0 * DFG.x + DFG.y from the LUT.
float3 ZeSkyAmbientResponse(float3 skyIrradiance, float3 baseColor, float metallic,
                            float3 specularResponse) {
    return skyIrradiance / PI * (ZeIndirectDiffuseWeight(baseColor, metallic, specularResponse) + specularResponse);
}

// Legacy helper retained for external shaders. Production material paths use
// ZeSkyAmbientResponse with the renderer's integrated GGX lookup texture.
float3 ZeSkyAmbient(float3 skyIrradiance, float3 baseColor, float metallic, float NoV) {
    float3 f0 = lerp(0.04f.xxx, baseColor, saturate(metallic));
    return ZeSkyAmbientResponse(skyIrradiance,baseColor,metallic,FresnelSchlick(saturate(NoV),f0));
}
#endif
