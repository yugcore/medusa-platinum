#ifndef ZE_GBUFFER_HLSLI
#define ZE_GBUFFER_HLSLI

struct ZeGBufferData {
    float3 baseColor;
    float3 worldNormal;
    float perceptualRoughness;
    float metallic;
    float materialAO;
    float3 emissive;
    uint shadingModel;
    uint flags;
};

void ZeEncodeGBuffer(ZeGBufferData value,
                   out float4 gbufferA,
                   out float4 gbufferB,
                   out float4 gbufferC,
                   out float4 gbufferD)
{
    gbufferA = float4(value.baseColor, value.materialAO);
    gbufferB = float4(value.worldNormal, value.perceptualRoughness);
    
    float metallicByte = saturate(value.metallic);
    float flagsByte = float(value.flags & 0xFF) / 255.0;
    float modelByte = float(value.shadingModel & 0xFF) / 255.0;
    gbufferC = float4(metallicByte, flagsByte, modelByte, 0.0);
    
    gbufferD = float4(value.emissive, 1.0);
}

void ZeEncodeGBufferUInt(ZeGBufferData value,
                       out float4 gbufferA,
                       out float4 gbufferB,
                       out uint4 gbufferC,
                       out float4 gbufferD)
{
    float3 safeEmissive =
        all(isfinite(value.emissive))
        ? max(value.emissive, 0.0f)
        : 0.0f;
    gbufferA = float4(saturate(value.baseColor), saturate(value.materialAO));
    gbufferB = float4(normalize(value.worldNormal), saturate(value.perceptualRoughness));
    gbufferC = uint4(
        (uint)round(saturate(value.metallic) * 255.0f),
        value.flags & 0xFFu,
        value.shadingModel & 0xFFu,
        0u);
    // GBufferD is RGBA16F; valid values above 1 remain HDR.
    gbufferD = float4(safeEmissive, 1.0f);
}

ZeGBufferData ZeDecodeGBuffer(float4 gbufferA,
                         float4 gbufferB,
                         float4 gbufferC,
                         float4 gbufferD)
{
    ZeGBufferData value;
    value.baseColor = gbufferA.rgb;
    value.materialAO = gbufferA.a;
    value.worldNormal = gbufferB.rgb;
    value.perceptualRoughness = gbufferB.a;
    value.metallic = gbufferC.r;
    value.flags = uint(gbufferC.g * 255.0 + 0.5);
    value.shadingModel = uint(gbufferC.b * 255.0 + 0.5);
    value.emissive = gbufferD.rgb;
    return value;
}

ZeGBufferData ZeDecodeGBufferUInt(float4 gbufferA,
                             float4 gbufferB,
                             uint4 gbufferC,
                             float4 gbufferD)
{
    ZeGBufferData value;
    value.baseColor = gbufferA.rgb;
    value.materialAO = gbufferA.a;
    value.worldNormal = normalize(gbufferB.rgb);
    value.perceptualRoughness = gbufferB.a;
    value.metallic = float(gbufferC.r) / 255.0f;
    value.flags = gbufferC.g;
    value.shadingModel = gbufferC.b;
    value.emissive = gbufferD.rgb;
    return value;
}

#endif
