#ifndef ZE_OBJECT_PARAMETERS_HLSLI
#define ZE_OBJECT_PARAMETERS_HLSLI

#include "ze_binding_spaces.hlsli"

cbuffer ObjectParameters : register(b0, SPACE_OBJECT)
{
    float4x4 world;
    float4x4 previousWorld;
    float4x4 normalMatrix;
    uint32_t materialIndex;
    uint32_t objectId;
    uint32_t objectFlags;
    uint32_t padding;
};

#endif // ZE_OBJECT_PARAMETERS_HLSLI
