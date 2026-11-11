#ifndef ZE_FRAME_PARAMETERS_HLSLI
#define ZE_FRAME_PARAMETERS_HLSLI

#include "ze_binding_spaces.hlsli"

cbuffer FrameParameters : register(b0, SPACE_GLOBAL)
{
    uint64_t frameIndex;
    float deltaSeconds;
    float absoluteSeconds;
    uint32_t flags;
};

#endif // ZE_FRAME_PARAMETERS_HLSLI
