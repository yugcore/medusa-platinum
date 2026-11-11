#include "../common/ze_camera_projection.hlsli"
// ============================================================================
// ZeGFX — G-Buffer Material Pass (Deferred Pipeline)
// ============================================================================
#include "../common/ze_gbuffer.hlsli"
#include "../common/ze_thin_surface.hlsli"
#include "../common/octahedral.hlsli"
#include "../common/ze_pbr_surface.hlsli"

cbuffer FrameConstants : register(b0)
{
    row_major float4x4 viewProj;
    float4 cameraPos;
    float4 lightDir;
    float4 lightColor;
    
    // Sky Ambient
    float4 skyHorizonColor;
    float4 skyZenithColor;
    float skyIntensity;
    float exposure;
    float gammaValue;
    float lightingDebugMode;

    // Fog Parameters
    float4 fogColor;
    float fogDensity;
    float fogStart;
    float fogEnd;
    float fogEnabled;

    // DX12 Shadow Constants
    row_major float4x4 lightViewProj;
    float shadowConstantBias;
    float shadowSlopeBias;
    float shadowStrength;
    float shadowEnabled;
    
    float shadowPcfRadius;
    float shadowCascadesCount;
    float shadowDebugMode;
    float minAmbientFloor;

    // DX13 additions
    float postProcessEnabled;
    float nearPlane;
    float farPlane;
    float shadowNormalBias;

    // DX15 IBL additions
    float iblEnabled;
    float iblIntensity;
    float envMapMaxMip;
    float giDebugMode;

    // Local light culling additions
    float viewportWidth;
    float viewportHeight;
    float numTilesX;
    float numTilesY;

    float numLocalLights;
    float lightGridTileSize;
    float maxLightsPerTile;
    float lightGridEnabled;

    row_major float4x4 shadowPageViewProj[4];
    float4 shadowPageScaleBias[4];
    float4 shadowCascadeSplits;
    float2 shadowAtlasInvSize;
    float shadowPageCount;
    float shadowPageEnabled;

    float aoEnabled;
    float aoIntensity;
    float aoRadius;
    float aoPower;

    float4 probeVolumeMin;
    float4 probeVolumeMax;
    float4 probeVolumeCountsEnabled;
    float4 probeVolumeProbes[32];

    float4 zgiVolumeOrigin;
    float4 zgiProbeSpacing;
    float4 zgiProbeCountsEnabled;
    float4 zgiSamplingParams;
    float4 environmentParams;
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 world;
    float4 objectColor; // Base Tint Color

    // Material Parameters
    float metallic;
    float roughness;
    float emissiveStrength;
    float hasNormalMap;

    float hasBaseColorTexture;
    float hasMetallicRoughnessTexture;
    float hasEmissiveMap;
    float hasOcclusionTexture;

    float4 emissiveColor;

    float alphaCutoff;
    float alphaMode; // 0 = OPAQUE, 1 = MASK, 2 = BLEND
    float doubleSided;
    float hasLightmap;

    float lightmapIntensity;
    float lightmapOcclusionBlend;
    float probeAmbientR;
    float probeAmbientG;

    float probeAmbientB;
    float probeAmbientIntensity;
    float isSkinned;
    float lmPadding1;
    float4 grassWindDirectionSpeed;
    float4 grassWindParams;
    float4 terrainSurfaceParams;
    float4 terrainVariationParams;
    float4 virtualTextureParams;
    float4 foliageWindField;
    float4 foliageWindGeometry;
    float4 foliageWindClimate;
};

cbuffer SkinningMatrices : register(b2)
{
    row_major float4x4 boneMatrices[256];
};

Texture2D baseColorTex          : register(t0);
Texture2D normalMapTex          : register(t1);
Texture2D metallicRoughnessTex  : register(t2);
Texture2D emissiveTex           : register(t3);
Texture2D occlusionTex          : register(t4);

SamplerState baseSampler        : register(s0);
SamplerState anisoSampler       : register(s1);
SamplerState clampSampler       : register(s2);

struct VSInput
{
    float3 pos          : POSITION;
    float2 octNormal    : NORMAL;
    float2 octTangent   : TANGENT;
    float2 uv           : TEXCOORD0;
    float2 uv1          : TEXCOORD1;
    float4 color        : COLOR0;
    uint4 joints        : JOINTS;
    float4 weights      : WEIGHTS;
    #include "../common/ze_instance_input.hlsli"
};

struct PSInput
{
    float4 pos          : SV_POSITION;
    float3 worldPos     : POSITION;
    float3 worldNormal  : NORMAL;
    float3 worldTangent : TANGENT;
    float3 worldBitan   : BITANGENT;
    float2 uv           : TEXCOORD0;
    float2 uv1          : TEXCOORD1;
    float4 color        : COLOR0;
};

struct GBufferOutput
{
    float4 a : SV_Target0;
    float4 b : SV_Target1;
    uint4 c  : SV_Target2;
    float4 d : SV_Target3;
};

#include "../common/ze_plant_wind.hlsli"
#include "../common/ze_instancing.hlsli"
PSInput VSMain(VSInput input)
{
    float plantWeight=saturate((input.pos.y-foliageWindGeometry.x)/max(.001,foliageWindGeometry.y));
    float3 plantRoot=float3(0,foliageWindGeometry.x,0);
    float plantHeight=foliageWindGeometry.y;
#ifdef ZE_GPU_INSTANCING
    plantRoot=float3(dot(input.instanceRow0.xyz,plantRoot)+input.instanceRow0.w,
                     dot(input.instanceRow1.xyz,plantRoot)+input.instanceRow1.w,
                     dot(input.instanceRow2.xyz,plantRoot)+input.instanceRow2.w);
    plantHeight*=length(float3(input.instanceRow0.y,input.instanceRow1.y,input.instanceRow2.y));
#endif
    plantRoot=mul(float4(plantRoot,1),world).xyz;
    plantHeight*=length(world[1].xyz);

    float3 inPos = input.pos;
    float3 inNormal = DecodeOctahedralSNORM(input.octNormal);
    float4 inTangent = DecodeOctahedralTangentSNORM(input.octTangent);
    float3 inBitangent = cross(inNormal, inTangent.xyz) * inTangent.w;
#ifdef ZE_GPU_INSTANCING
    ZeInstanceVertex(input.instanceRow0,input.instanceRow1,input.instanceRow2,inPos,inNormal,input.uv);
    inTangent.xyz=float3(dot(input.instanceRow0.xyz,inTangent.xyz),
        dot(input.instanceRow1.xyz,inTangent.xyz),dot(input.instanceRow2.xyz,inTangent.xyz));
    inTangent.xyz=ZeUnitVector(inTangent.xyz-inNormal*dot(inNormal,inTangent.xyz),float3(1,0,0));
    inBitangent=cross(inNormal,inTangent.xyz)*inTangent.w;
#endif

    if (isSkinned > 0.5f)
    {
        float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
        float3 skinnedTangent = float3(0.0f, 0.0f, 0.0f);
        float3 skinnedBitangent = float3(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < 4; ++i)
        {
            float w = input.weights[i];
            if (w > 0.0f)
            {
                uint jointIdx = input.joints[i];
                row_major float4x4 boneMat = boneMatrices[jointIdx];
                skinnedPos += mul(float4(inPos, 1.0f), boneMat) * w;
                skinnedNormal += mul(float4(inNormal, 0.0f), boneMat).xyz * w;
                skinnedTangent += mul(float4(inTangent.xyz, 0.0f), boneMat).xyz * w;
                skinnedBitangent += mul(float4(inBitangent, 0.0f), boneMat).xyz * w;
            }
        }
        inPos = skinnedPos.xyz;
        inNormal = skinnedNormal;
        inTangent.xyz = skinnedTangent;
        inBitangent = skinnedBitangent;
    }

    PSInput output;
    float4 worldPosition = mul(float4(inPos, 1.0f), world);
    float plantSlope;
    worldPosition.xyz = ZeTreeVertex(worldPosition.xyz,
                                     plantWeight,
                                     plantRoot,
                                     plantHeight,
                                     grassWindDirectionSpeed,
                                     grassWindParams,
                                     foliageWindField,
                                     foliageWindGeometry,
                                     foliageWindClimate,
                                     plantSlope);
    output.pos = ZeProjectCamera(worldPosition, viewProj, float2(lightDir.w, lightColor.w));
    output.worldPos = worldPosition.xyz;
    output.worldNormal = ZeTransformNormal(inNormal, (float3x3)world);
    if (foliageWindGeometry.z > 0) {
        output.worldNormal.y -= dot(output.worldNormal.xz, grassWindDirectionSpeed.xz) * plantSlope;
        output.worldNormal = normalize(output.worldNormal);
    }
    output.worldTangent = normalize(mul(float4(inTangent.xyz, 0.0f), world).xyz);
    output.worldBitan = normalize(mul(float4(inBitangent, 0.0f), world).xyz);
    output.uv = input.uv;
    output.uv1 = input.uv1;
    output.color = input.color * objectColor;
    return output;
}

#include "../common/terrain_layers.hlsli"

GBufferOutput PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace)
{
    // Layer count is populated only for the explicitly declared canonical
    // layered surface. Other custom vertex/material programs stay forward.
    bool layeredTerrain=terrainVariationParams.w>=3.0f&&terrainSurfaceParams.x>0.0f;
    float4 albedo = input.color;
    if (!layeredTerrain&&hasBaseColorTexture > 0.5f) {
        albedo *= baseColorTex.Sample(anisoSampler, input.uv);
    }

    // MASK is alpha-tested
    if (alphaMode > 0.5f && alphaMode < 1.5f) {
        if (albedo.a < alphaCutoff) discard;
    }

    float3 N = normalize(input.worldNormal);
    if (!layeredTerrain&&hasNormalMap > 0.5f) {
        float4 normalSample = normalMapTex.Sample(anisoSampler, input.uv);
        N = DecodeWorldNormalFromMap(normalSample, input.worldNormal, input.worldTangent, input.worldBitan);
    }

    bool thinSurface=terrainSurfaceParams.w<-.5;
    if(thinSurface)N=ZeThinNormal(N,terrainSurfaceParams.z);
    if (doubleSided > 0.5f && !thinSurface && dot(N,cameraPos.xyz-input.worldPos)<0) {
        N = -N;
    }

    float rough = roughness;
    float metal = metallic;
    if (!layeredTerrain&&hasMetallicRoughnessTexture > 0.5f) {
        float4 mrSample = metallicRoughnessTex.Sample(anisoSampler, input.uv);
        rough *= mrSample.g;
        metal *= mrSample.b;
    }
    rough = saturate(rough);
    metal = saturate(metal);

    float ao = 1.0f;
    if (hasOcclusionTexture > 0.5f) {
        ao = occlusionTex.Sample(anisoSampler, input.uv).r;
    }
    [branch] if(layeredTerrain)ZeTerrainSurface(input,albedo,N,rough,metal,ao);

    float3 emissive = emissiveColor.rgb * emissiveStrength;
    if (hasEmissiveMap > 0.5f) {
        emissive *= emissiveTex.Sample(anisoSampler, input.uv).rgb;
    }

    rough = ZeFilterRoughness(rough, ddx(N), ddy(N));

    ZeGBufferData data;
    data.baseColor = albedo.rgb;
    data.worldNormal = N;
    data.perceptualRoughness = rough;
    data.metallic = metal;
    data.materialAO = ao;
    data.emissive = emissive;
    data.shadingModel = thinSurface?2u:1u;
    data.flags = thinSurface?ZeThinPack(terrainSurfaceParams.xy):0u;

    GBufferOutput output;
    ZeEncodeGBufferUInt(data, output.a, output.b, output.c, output.d);
    return output;
}
