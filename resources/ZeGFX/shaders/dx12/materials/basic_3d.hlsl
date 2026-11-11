#include "../common/ze_pbr_surface.hlsli"
#include "../common/ze_camera_projection.hlsli"
// ============================================================================
// ZeGFX — DirectX 12 Basic 3D Shader
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
    float3 normal : NORMAL;
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    output.pos = ZeProjectCamera(worldPos, viewProj, float2(lightDir.w, lightColor.w));
    output.normal = ZeTransformNormal(input.normal, (float3x3)world);
    output.color = input.color * objectColor;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float3 L = normalize(-lightDir.xyz);
    float ndotl = saturate(dot(N, L));
    float3 ambient = input.color.rgb * 0.18f;
    float3 diffuse = input.color.rgb * lightColor.rgb * ndotl;
    return float4(ambient + diffuse, input.color.a);
}
