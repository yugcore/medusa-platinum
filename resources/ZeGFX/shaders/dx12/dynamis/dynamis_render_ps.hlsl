// ============================================================================
// Dynamis GPU Compute VFX - Billboard Pixel Shader
// Clean-Room Implementation for ZeGFX / Blueman Engine
// Radial softness, color ramps, and emissive HDR output for Bloom
// ============================================================================

#include "dynamis_common.hlsli"

struct PSInput {
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 uv        : TEXCOORD0;
    float  viewDepth : TEXCOORD1;
};

// Scene depth texture for optional soft-particle geometry intersection
Texture2D<float> g_SceneDepth : register(t1);
SamplerState g_DepthSampler   : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    // Center-offset coordinates in [-1.0, 1.0]
    float2 centerUV = input.uv * 2.0f - 1.0f;
    float distSq = dot(centerUV, centerUV);

    // Circular particle boundary clipping
    if (distSq > 1.0f)
        discard;

    // Smooth Gaussian/polynomial radial core falloff
    float radialAlpha = saturate(1.0f - distSq) * exp(-distSq * 2.0f);

    float4 outColor = input.color;
    outColor.a *= radialAlpha;

    // Soft-particle depth intersection test
    if (g_SoftDepthDistance > 0.001f)
    {
        float2 screenUV = input.position.xy * g_ViewportSize.zw;
        float sceneDepth = g_SceneDepth.SampleLevel(g_DepthSampler, screenUV, 0);

        // Linearize depth if non-linear reverse-Z / standard depth
        float nearZ = g_CameraPosition.w;
        float farZ = g_CameraForward.w;
        if (sceneDepth > 0.0f && sceneDepth < 1.0f && farZ > nearZ)
        {
            float sceneLinear = (nearZ * farZ) / (farZ - sceneDepth * (farZ - nearZ));
            float particleLinear = input.viewDepth;
            float depthDiff = sceneLinear - particleLinear;

            if (depthDiff >= 0.0f)
            {
                float softFactor = saturate(depthDiff / g_SoftDepthDistance);
                outColor.a *= softFactor;
            }
            else
            {
                outColor.a = 0.0f;
            }
        }
    }

    // Blend Mode handling:
    // 0: Additive (fire, sparks, magic, lasers) - premultiply alpha for HDR bloom
    // 1: AlphaBlend (smoke, dust, clouds, steam)
    if (g_BlendMode == 0u)
    {
        outColor.rgb *= outColor.a;
    }

    return outColor;
}
