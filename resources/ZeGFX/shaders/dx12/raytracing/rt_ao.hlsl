#include "../common/ze_raytracing_common.hlsli"

RaytracingAccelerationStructure Scene : register(t0);
Texture2D DepthBuffer : register(t1);
Texture2D NormalMap : register(t2);
SamplerState PointSampler : register(s0);
RWTexture2D<float> OutAOMask : register(u0);

cbuffer AoCB : register(b0) {
    float aoRadius;
    float aoNormalBias;
    uint samplesPerPixel;
};

// Simple pseudo-random number generator
float Hash(uint2 seed) {
    return frac(sin(dot(float2(seed), float2(12.9898f, 78.233f))) * 43758.5453f);
}

float3 CosineSampleHemisphere(float2 u, float3 N) {
    float phi = 2.0f * 3.14159f * u.x;
    float cosTheta = sqrt(u.y);
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    
    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    return tangent * H.x + bitangent * H.y + N * H.z;
}

[shader("raygeneration")]
void RayGenMain() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    float depth = DepthBuffer[launchIndex].r;
    
    if (depth >= 1.0f) {
        OutAOMask[launchIndex] = 1.0f;
        return;
    }
    
    float3 P = float3(launchIndex.x / 1280.f, launchIndex.y / 720.f, depth);
    float3 N = normalize(NormalMap[launchIndex].xyz * 2.f - 1.f);
    
    float ao = 0.0f;
    for (uint i = 0; i < samplesPerPixel; ++i) {
        float2 u = float2(Hash(launchIndex + i), Hash(launchIndex + i + 100));
        float3 rayDir = CosineSampleHemisphere(u, N);
        
        RayDesc ray;
        ray.Origin = P + N * aoNormalBias;
        ray.Direction = rayDir;
        ray.TMin = 0.001f;
        ray.TMax = aoRadius;
        
        RayPayload payload;
        payload.color = float3(0.f, 0.f, 0.f);
        payload.distance = -1.f;
        payload.instanceId = 0xFFFFFFFF;
        payload.geometryId = 0xFFFFFFFF;
        payload.bounces = 0;
        
        TraceRay(Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT, 0xFF, 0, 0, 0, ray, payload);
        
        if (payload.distance < 0.f) {
            ao += 1.0f;
        }
    }
    
    OutAOMask[launchIndex] = ao / float(samplesPerPixel);
}
