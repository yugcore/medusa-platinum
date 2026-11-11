// ============================================================================
// ZeGFX — DXR Miss Shader (SM 6.3)
// ============================================================================

struct RayPayload
{
    float4 Color;
    float Distance;
};

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // Simple fallback background sky color
    payload.Color = float4(0.25f, 0.38f, 0.52f, 1.0f);
    payload.Distance = -1.0f;
}
