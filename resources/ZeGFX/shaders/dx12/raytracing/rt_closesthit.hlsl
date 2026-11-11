// ============================================================================
// ZeGFX — DXR Closest Hit Shader (SM 6.3)
// ============================================================================

struct RayPayload
{
    float4 Color;
    float Distance;
};

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Color using Barycentric coordinates for visual debugging
    payload.Color = float4(attr.barycentrics.x, attr.barycentrics.y, 1.0 - attr.barycentrics.x - attr.barycentrics.y, 1.0);
    payload.Distance = RayTCurrent();
}
