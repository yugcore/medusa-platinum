#include "../common/ze_raytracing_common.hlsli"

RaytracingAccelerationStructure Scene : register(t0);
Texture2D DepthBuffer : register(t1);
Texture2D NormalMap : register(t2);
TextureCube EnvMap : register(t3);
SamplerState LinearSampler : register(s0);
RWTexture2D<float4> OutReflectionRadiance : register(u0);

cbuffer ReflectionCB : register(b0) {
    float maxDistance;
    float roughnessCutoff;
};

[shader("raygeneration")]
void RayGenMain() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    float depth = DepthBuffer[launchIndex].r;
    
    if (depth >= 1.0f) {
        OutReflectionRadiance[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);
        return;
    }
    
    float3 P = float3(launchIndex.x / 1280.f, launchIndex.y / 720.f, depth);
    float3 N = normalize(NormalMap[launchIndex].xyz * 2.f - 1.f);
    
    float3 V = normalize(P - float3(0.5f, 0.5f, 0.f)); // Approximate camera view vector
    float3 R = reflect(V, N);
    
    RayDesc ray;
    ray.Origin = P + N * 0.01f;
    ray.Direction = R;
    ray.TMin = 0.001f;
    ray.TMax = maxDistance;
    
    RayPayload payload;
    payload.color = float3(0.f, 0.f, 0.f);
    payload.distance = -1.f;
    payload.instanceId = 0xFFFFFFFF;
    payload.geometryId = 0xFFFFFFFF;
    payload.bounces = 0;
    
    TraceRay(Scene, RAY_FLAGS_NONE, 0xFF, 0, 0, 0, ray, payload);
    
    float3 radiance = float3(0.f, 0.f, 0.f);
    if (payload.distance >= 0.f) {
        radiance = payload.color;
    } else {
        // Miss fallback: sample environment map
        radiance = EnvMap.SampleLevel(LinearSampler, R, 0).rgb;
    }
    
    OutReflectionRadiance[launchIndex] = float4(radiance, 1.0f);
}
