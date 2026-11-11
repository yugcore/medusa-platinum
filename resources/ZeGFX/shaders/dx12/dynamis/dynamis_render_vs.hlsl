// ============================================================================
// Dynamis GPU Compute VFX - Billboard Vertex Shader
// Clean-Room Implementation for ZeGFX / Blueman Engine
// Expands camera-facing quads via SV_VertexID with zero CPU vertex streaming
// ============================================================================

#include "dynamis_common.hlsli"

StructuredBuffer<DynamisParticle> g_Particles : register(t0);

struct VSOutput {
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 uv        : TEXCOORD0;
    float  viewDepth : TEXCOORD1;
};

// 6-vertex quad layout for D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
static const float2 kQuadOffsets[6] = {
    float2(-1.0f, -1.0f),
    float2( 1.0f, -1.0f),
    float2( 1.0f,  1.0f),
    float2(-1.0f, -1.0f),
    float2( 1.0f,  1.0f),
    float2(-1.0f,  1.0f)
};

static const float2 kQuadUVs[6] = {
    float2(0.0f, 1.0f),
    float2(1.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 0.0f)
};

VSOutput VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VSOutput output;

    DynamisParticle p = g_Particles[instanceID];

    // If particle is inactive/dead, collapse to degenerate clip coords
    if (p.age < 0.0f)
    {
        output.position = float4(0.0f, 0.0f, 0.0f, 0.0f);
        output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        output.uv = float2(0.0f, 0.0f);
        output.viewDepth = 0.0f;
        return output;
    }

    // Normalized age [0.0, 1.0]
    float t = saturate(p.age / max(0.001f, p.lifetime));

    // Interpolate size and color over particle life
    float currentRadius = lerp(p.sizeStart, p.sizeEnd, t);
    float4 currentColor = lerp(p.colorStart, p.colorEnd, t);

    // Quad vertex index (0..5)
    uint quadVertIdx = vertexID % 6;
    float2 rawOffset = kQuadOffsets[quadVertIdx];

    // Rotate quad offset in billboard plane
    float cosR = cos(p.rotation);
    float sinR = sin(p.rotation);
    float2 rotatedOffset = float2(
        rawOffset.x * cosR - rawOffset.y * sinR,
        rawOffset.x * sinR + rawOffset.y * cosR
    ) * currentRadius;

    // Transform world origin to view space
    float4 viewPos = mul(g_ViewMatrix, float4(p.position, 1.0f));

    // Camera-facing expansion in view space
    viewPos.xy += rotatedOffset;

    // Project to clip space
    output.position = mul(g_ProjectionMatrix, viewPos);
    output.color = currentColor;
    output.uv = kQuadUVs[quadVertIdx];
    output.viewDepth = viewPos.z;

    return output;
}
