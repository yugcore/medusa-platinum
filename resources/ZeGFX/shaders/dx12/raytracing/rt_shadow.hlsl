#include "../common/ze_raytracing_common.hlsli"

RaytracingAccelerationStructure Scene : register(t0);
Texture2D DepthBuffer : register(t1);
SamplerState PointSampler : register(s0);
RWTexture2D<float> OutShadowMask : register(u0);

cbuffer ShadowCB : register(b0) {
    float3 lightDirection;
    float maxDistance;
    float normalBias;
    float angularSize;
};

[shader("raygeneration")]
void RayGenMain() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    float depth = DepthBuffer[launchIndex].r;
    
    if (depth >= 1.0f) {
        OutShadowMask[launchIndex] = 1.0f;
        return;
    }
    
    // Trace visibility ray
    RayDesc ray;
    ray.Origin = float3(launchIndex.x / 1280.f, launchIndex.y / 720.f, depth);
    ray.Direction = lightDirection;
    ray.TMin = normalBias;
    ray.TMax = maxDistance;
    
    RayPayload payload;
    payload.color = float3(0.f, 0.f, 0.f);
    payload.distance = -1.f;
    payload.instanceId = 0xFFFFFFFF;
    payload.geometryId = 0xFFFFFFFF;
    payload.bounces = 0;
    
    TraceRay(Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT, 0xFF, 0, 0, 0, ray, payload);
    
    float shadowFactor = (payload.distance >= 0.f) ? 0.0f : 1.0f;
    OutShadowMask[launchIndex] = shadowFactor;
}
