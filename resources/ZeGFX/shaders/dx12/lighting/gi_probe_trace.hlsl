#include "../common/ze_raytracing_common.hlsli"
#include "../common/ze_gi_common.hlsli"

RaytracingAccelerationStructure Scene : register(t0);
TextureCube EnvMap : register(t1);
SamplerState LinearSampler : register(s0);
RWTexture2D<float4> OutRadianceDistance : register(u0);

cbuffer ProbeTraceCB : register(b0) {
    uint3 probeCounts;
    uint raysPerProbe;
    float maximumRayDistance;
    float3 probeSpacing;
    float3 volumeOrigin;
};

// Generate spherical Fibonacci direction
float3 GetSphericalFibonacciDirection(uint sampleIndex, uint numSamples) {
    float b = 2.0f * sampleIndex + 1.0f - numSamples;
    float phi = 2.39996f * sampleIndex;
    float theta = acos(b / numSamples);
    return float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
}

[shader("raygeneration")]
void RayGenMain() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint probeIndex = launchIndex.y;
    uint rayIndex = launchIndex.x;
    
    if (rayIndex >= raysPerProbe) return;
    
    // Compute probe grid position
    uint3 coord;
    uint slices = probeCounts.x * probeCounts.y;
    coord.z = probeIndex / slices;
    uint rem = probeIndex % slices;
    coord.y = rem / probeCounts.x;
    coord.x = rem % probeCounts.x;
    
    float3 probePos = volumeOrigin + coord * probeSpacing;
    float3 rayDir = GetSphericalFibonacciDirection(rayIndex, raysPerProbe);
    
    RayDesc ray;
    ray.Origin = probePos;
    ray.Direction = rayDir;
    ray.TMin = 0.001f;
    ray.TMax = maximumRayDistance;
    
    RayPayload payload;
    payload.color = float3(0.f, 0.f, 0.f);
    payload.distance = -1.f;
    payload.instanceId = 0xFFFFFFFF;
    payload.geometryId = 0xFFFFFFFF;
    payload.bounces = 0;
    
    TraceRay(Scene, RAY_FLAGS_NONE, 0xFF, 0, 0, 0, ray, payload);
    
    float3 radiance = float3(0.f, 0.f, 0.f);
    float hitDistance = maximumRayDistance;
    
    if (payload.distance >= 0.f) {
        radiance = payload.color;
        hitDistance = payload.distance;
    } else {
        radiance = EnvMap.SampleLevel(LinearSampler, rayDir, 0).rgb;
    }
    
    OutRadianceDistance[launchIndex] = float4(radiance, hitDistance);
}
