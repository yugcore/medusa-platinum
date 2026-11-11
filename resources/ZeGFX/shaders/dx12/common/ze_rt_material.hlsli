#ifndef ZE_RT_MATERIAL_HLSLI
#define ZE_RT_MATERIAL_HLSLI

struct GpuRtMaterial {
    float4 baseColorFactor;
    float4 emissiveAlphaCutoff;
    float4 metallicRoughnessFlags;
    uint baseColorTexture;
    uint normalTexture;
    uint ormTexture;
    uint emissiveTexture;
    uint samplerIndex;
};

#endif // ZE_RT_MATERIAL_HLSLI
