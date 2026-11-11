// ============================================================================
// ZeGFX — DXR Shadow Miss Shader (SM 6.3)
// ============================================================================

struct ShadowPayload
{
    bool Hit;
};

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload)
{
    // No hit means not shadowed
    payload.Hit = false;
}
