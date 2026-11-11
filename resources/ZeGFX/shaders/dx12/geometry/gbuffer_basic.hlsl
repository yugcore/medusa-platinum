#include "../common/ze_pbr_surface.hlsli"
#include "../common/ze_camera_projection.hlsli"
#include "../common/ze_gbuffer.hlsli"

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
    float4 color : COLOR;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 worldNormal : NORMAL;
    float4 color : COLOR;
};

struct GBufferOutput
{
    float4 a : SV_Target0;
    float4 b : SV_Target1;
    uint4 c : SV_Target2;
    float4 d : SV_Target3;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPosition = mul(float4(input.pos, 1.0f), world);
    output.pos = ZeProjectCamera(worldPosition, viewProj, float2(lightDir.w, lightColor.w));
    output.worldNormal = ZeTransformNormal(input.normal, (float3x3)world);
    output.color = input.color * objectColor;
    return output;
}

GBufferOutput PSMain(PSInput input)
{
    ZeGBufferData data;
    data.baseColor = input.color.rgb;
    data.worldNormal = normalize(input.worldNormal);
    data.perceptualRoughness = 0.5f;
    data.metallic = 0.0f;
    data.materialAO = 1.0f;
    data.emissive = 0.0f;
    data.shadingModel = 1u;
    data.flags = 0u;

    GBufferOutput output;
    ZeEncodeGBufferUInt(data, output.a, output.b, output.c, output.d);
    return output;
}
