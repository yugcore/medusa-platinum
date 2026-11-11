#include "../common/ze_raytracing_common.hlsli"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> Output : register(u0);

[shader("raygeneration")]
void RayGenMain() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 d = (((launchIndex + 0.5f) / launchDim) * 2.f - 1.f);
    d.y = -d.y;

    RayDesc ray;
    ray.Origin = float3(0.f, 0.f, -2.f);
    ray.Direction = normalize(float3(d.x, d.y, 1.f));
    ray.TMin = 0.001f;
    ray.TMax = 1000.f;

    RayPayload payload;
    payload.color = float3(0.f, 0.f, 0.f);
    payload.distance = -1.f;
    payload.instanceId = 0xFFFFFFFF;
    payload.geometryId = 0xFFFFFFFF;
    payload.bounces = 0;

    TraceRay(Scene, RAY_FLAGS_NONE, 0xFF, 0, 0, 0, ray, payload);

    if (payload.distance >= 0.f) {
        // Deterministic color based on InstanceID
        float r = ((payload.instanceId + 1) & 1) ? 1.0f : 0.0f;
        float g = ((payload.instanceId + 1) & 2) ? 1.0f : 0.0f;
        float b = ((payload.instanceId + 1) & 4) ? 1.0f : 0.0f;
        Output[launchIndex] = float4(r, g, b, 1.f);
    } else {
        // Miss shader background color
        Output[launchIndex] = float4(0.2f, 0.2f, 0.25f, 1.f);
    }
}
