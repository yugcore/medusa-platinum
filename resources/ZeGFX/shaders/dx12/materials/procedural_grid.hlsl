#include "../common/ze_camera_projection.hlsli"
// ============================================================================
// ZeGFX — Analytical Antialiased Procedural Grid Shader (DirectX 12)
// Derivative-based pixel-aware world-space XZ grid with distance fading,
// major/minor subdivisions, subpixel moiré suppression, and origin axes.
// ============================================================================

cbuffer FrameConstants : register(b0)
{
    row_major float4x4 viewProj;
    float4 cameraPos;
    float4 lightDir;
    float4 lightColor;
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 world;
    float4 objectColor;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    output.worldPos = worldPos.xyz;
    output.pos = ZeProjectCamera(worldPos, viewProj, float2(lightDir.w, lightColor.w));
    output.color = input.color * objectColor;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 worldPos = input.worldPos;
    float2 coord = worldPos.xz;

    // Minor grid: 1.0 unit spacing
    const float minorSpacing = 1.0f;
    float2 minorCoord = coord / minorSpacing;
    float2 minorDeriv = max(fwidth(minorCoord), 0.00001f);
    float2 minorDist = abs(frac(minorCoord - 0.5f) - 0.5f);
    // 1.2 pixel line width
    float2 minorCoverage = 1.0f - saturate(minorDist / (minorDeriv * 0.6f));
    float minorLine = max(minorCoverage.x, minorCoverage.y);
    // Subpixel fade: as projected cell approaches 1 pixel, fade out to eliminate moire
    float minorSubpixel = saturate(1.0f - max(minorDeriv.x, minorDeriv.y) * 1.5f);
    minorLine *= minorSubpixel;

    // Major grid: 10.0 unit spacing
    const float majorSpacing = 10.0f;
    float2 majorCoord = coord / majorSpacing;
    float2 majorDeriv = max(fwidth(majorCoord), 0.00001f);
    float2 majorDist = abs(frac(majorCoord - 0.5f) - 0.5f);
    // 1.8 pixel line width
    float2 majorCoverage = 1.0f - saturate(majorDist / (majorDeriv * 0.9f));
    float majorLine = max(majorCoverage.x, majorCoverage.y);
    float majorSubpixel = saturate(1.0f - max(majorDeriv.x, majorDeriv.y) * 1.5f);
    majorLine *= majorSubpixel;

    // Origin axes: X axis (Z = 0) and Z axis (X = 0)
    float2 worldDeriv = max(fwidth(coord), 0.00001f);
    float axisX = 1.0f - saturate(abs(coord.y) / (worldDeriv.y * 1.2f)); // along X axis, z=0
    float axisZ = 1.0f - saturate(abs(coord.x) / (worldDeriv.x * 1.2f)); // along Z axis, x=0

    // Distance falloff from camera
    float distToCam = length(worldPos - cameraPos.xyz);
    float distFade = 1.0f - smoothstep(30.0f, 150.0f, distToCam);

    // Glancing angle falloff: fade when looking near parallel to grid plane
    float3 viewDir = normalize(cameraPos.xyz - worldPos);
    float angleFade = saturate(abs(viewDir.y) * 8.0f);
    float fade = distFade * angleFade;

    if (fade <= 0.001f) discard;

    // Grid line styling:
    float4 baseColor = input.color;
    float3 minorColor = baseColor.rgb * 0.85f;
    float minorAlpha = 0.25f * minorLine;

    float3 majorColor = baseColor.rgb * 1.4f;
    float majorAlpha = 0.55f * majorLine;

    float3 gridColor = lerp(minorColor, majorColor, majorLine);
    float gridAlpha = max(minorAlpha, majorAlpha);

    // Origin axes: Coral Red for X (#F53352), Sky Blue for Z (#298CF5)
    float3 axisColor = float3(0, 0, 0);
    float axisAlpha = 0.0f;
    if (axisX > 0.001f)
    {
        axisColor = float3(0.96f, 0.20f, 0.32f);
        axisAlpha = max(axisAlpha, axisX * 0.9f);
    }
    if (axisZ > 0.001f)
    {
        axisColor = lerp(axisColor, float3(0.16f, 0.55f, 0.96f), axisZ);
        axisAlpha = max(axisAlpha, axisZ * 0.9f);
    }

    float3 finalRgb = lerp(gridColor, axisColor, axisAlpha);
    float finalAlpha = max(gridAlpha, axisAlpha) * fade;

    if (finalAlpha <= 0.002f) discard;

    return float4(finalRgb, finalAlpha);
}
