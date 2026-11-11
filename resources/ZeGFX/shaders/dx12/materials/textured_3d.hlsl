#include "../common/ze_pbr_surface.hlsli"
#include "../common/ze_camera_projection.hlsli"
// ============================================================================
// ZeGFX — DirectX 12 Textured 3D Shader (DX8 GLTF Models)
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

#include "../common/octahedral.hlsli"

Texture2D baseColorTexture : register(t0);
SamplerState baseSampler : register(s0);

struct VSInput
{
    float3 pos : POSITION;
    float2 octNormal : NORMAL;
    float2 octTangent : TANGENT;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float3 norm = DecodeOctahedralSNORM(input.octNormal);
    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    output.pos = ZeProjectCamera(worldPos, viewProj, float2(lightDir.w, lightColor.w));
    output.worldNormal = ZeTransformNormal(norm, (float3x3)world);
    output.uv = input.uv;
    output.color = input.color * objectColor;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 texColor = baseColorTexture.Sample(baseSampler, input.uv);
    float4 baseColor = texColor * input.color;

    float3 N = normalize(input.worldNormal);
    float3 L = normalize(-lightDir.xyz);
    float ndotl = saturate(dot(N, L));

    float3 ambient = baseColor.rgb * 0.18f;
    float3 diffuse = baseColor.rgb * lightColor.rgb * ndotl;

    return float4(ambient + diffuse, baseColor.a);
}
