#ifndef ZE_RAY_PAYLOADS_HLSLI
#define ZE_RAY_PAYLOADS_HLSLI

struct RayPayload {
    float3 color;
    float distance;
    uint instanceId;
    uint geometryId;
    uint bounces;
};

#endif // ZE_RAY_PAYLOADS_HLSLI
