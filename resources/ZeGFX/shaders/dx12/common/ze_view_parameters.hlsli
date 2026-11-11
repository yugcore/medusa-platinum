#ifndef ZE_VIEW_PARAMETERS_HLSLI
#define ZE_VIEW_PARAMETERS_HLSLI

#include "ze_binding_spaces.hlsli"

cbuffer ViewParameters : register(b1, SPACE_GLOBAL)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 inverseView;
    float4x4 inverseProjection;
    float4x4 inverseViewProjection;
    float4x4 previousViewProjection;
    float4 cameraPositionNear;
    float4 renderSizeInvSize;
    float4 jitterPreviousJitter;
};

#endif // ZE_VIEW_PARAMETERS_HLSLI
