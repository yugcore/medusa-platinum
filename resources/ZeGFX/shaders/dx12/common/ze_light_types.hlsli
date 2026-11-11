#ifndef ZE_LIGHT_TYPES_HLSLI
#define ZE_LIGHT_TYPES_HLSLI

struct GpuLocalLight {
    float4 positionRange;      // position.xyz, range
    float4 directionType;      // direction.xyz, (float)type
    float4 colorIntensity;     // color.rgb, intensity
    float4 coneRectFlags;      // innerConeCos, outerConeCos, rectSize.x, rectSize.y
};

struct TileLightRange {
    uint offset;
    uint count;
};

#endif
