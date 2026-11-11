#ifndef ZE_BLOOM_COMMON_HLSLI
#define ZE_BLOOM_COMMON_HLSLI

cbuffer BloomConstants : register(b0)
{
    float2 outputSize;
    float2 sourceSize;
    float threshold;
    float intensity;
    float radius;
    float padding0;
};

Texture2D<float4> sourceTexture : register(t0);
Texture2D<float4> detailTexture : register(t1);
SamplerState linearClampSampler : register(s0);

struct VSInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float2 ndc;
    ndc.x = input.position.x / outputSize.x * 2.0f - 1.0f;
    ndc.y = 1.0f - input.position.y / outputSize.y * 2.0f;
    output.position = float4(ndc, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}

float3 SampleBloomBox13(
    Texture2D<float4> textureSource,
    float2 uv,
    float2 texel)
{
    float3 a = textureSource.Sample(linearClampSampler, uv + texel * float2(-2,  2)).rgb;
    float3 b = textureSource.Sample(linearClampSampler, uv + texel * float2( 0,  2)).rgb;
    float3 c = textureSource.Sample(linearClampSampler, uv + texel * float2( 2,  2)).rgb;
    float3 d = textureSource.Sample(linearClampSampler, uv + texel * float2(-2,  0)).rgb;
    float3 e = textureSource.Sample(linearClampSampler, uv).rgb;
    float3 f = textureSource.Sample(linearClampSampler, uv + texel * float2( 2,  0)).rgb;
    float3 g = textureSource.Sample(linearClampSampler, uv + texel * float2(-2, -2)).rgb;
    float3 h = textureSource.Sample(linearClampSampler, uv + texel * float2( 0, -2)).rgb;
    float3 i = textureSource.Sample(linearClampSampler, uv + texel * float2( 2, -2)).rgb;
    float3 j = textureSource.Sample(linearClampSampler, uv + texel * float2(-1,  1)).rgb;
    float3 k = textureSource.Sample(linearClampSampler, uv + texel * float2( 1,  1)).rgb;
    float3 l = textureSource.Sample(linearClampSampler, uv + texel * float2(-1, -1)).rgb;
    float3 m = textureSource.Sample(linearClampSampler, uv + texel * float2( 1, -1)).rgb;

    return (a + c + g + i) * 0.03125f +
           (b + d + f + h) * 0.0625f +
           (j + k + l + m) * 0.125f +
           e * 0.125f;
}

float3 SampleBloomTent9(
    Texture2D<float4> textureSource,
    float2 uv,
    float2 texel)
{
    float3 result = textureSource.Sample(linearClampSampler, uv).rgb * 4.0f;
    result += textureSource.Sample(linearClampSampler, uv + float2( texel.x, 0.0f)).rgb * 2.0f;
    result += textureSource.Sample(linearClampSampler, uv + float2(-texel.x, 0.0f)).rgb * 2.0f;
    result += textureSource.Sample(linearClampSampler, uv + float2(0.0f,  texel.y)).rgb * 2.0f;
    result += textureSource.Sample(linearClampSampler, uv + float2(0.0f, -texel.y)).rgb * 2.0f;
    result += textureSource.Sample(linearClampSampler, uv + float2( texel.x,  texel.y)).rgb;
    result += textureSource.Sample(linearClampSampler, uv + float2(-texel.x,  texel.y)).rgb;
    result += textureSource.Sample(linearClampSampler, uv + float2( texel.x, -texel.y)).rgb;
    result += textureSource.Sample(linearClampSampler, uv + float2(-texel.x, -texel.y)).rgb;
    return result * (1.0f / 16.0f);
}

#endif
