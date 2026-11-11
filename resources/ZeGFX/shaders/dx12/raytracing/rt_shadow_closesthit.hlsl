// ============================================================================
// ZeGFX — DXR Shadow Closest Hit Shader (SM 6.3)
// ============================================================================

struct ShadowPayload
{
    bool Hit;
};

struct BuiltInTriangleIntersectionAttributes
{
    float2 Barycentrics;
};

[shader("closesthit")]
void ShadowClosestHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // A hit represents shadowed point
    payload.Hit = true;
}
