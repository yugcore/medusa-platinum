// ============================================================================
// ZeGFX — DXR Ray Generation Shader (SM 6.3)
// ============================================================================

struct RayPayload
{
    float4 Color;
    float Distance;
};

RaytracingAccelerationStructure Scene : register(t0);

[shader("raygeneration")]
void RayGen()
{
    RayDesc ray;
    ray.Origin = float3(0, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.Color = float4(0, 0, 0, 0);
    payload.Distance = 0.0;

    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
}
