#include "../common/ze_camera_projection.hlsli"
#include "../common/ze_outdoor_fog.hlsli"
#include "../common/ze_water_surface.hlsli"
// ============================================================================
// ZeGFX — DirectX 12 PBR Material and Outdoor Lighting Shader
// ============================================================================

#include "../common/ze_surface_data.hlsli"
#include "../common/ze_brdf.hlsli"
#include "../common/ze_indirect_lighting.hlsli"
#include "../common/ze_thin_surface.hlsli"
#include "../common/octahedral.hlsli"
#include "../common/ze_pbr_surface.hlsli"
#include "../common/ze_shadow_receiver.hlsli"
#include "../common/ze_shadow_gaussian.hlsli"


cbuffer FrameConstants : register(b0)
{
    row_major float4x4 viewProj;
    float4 cameraPos;
    float4 lightDir;      // Directional light (sun) direction
    float4 lightColor;    // Directional light color * intensity
    
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
    // Keep the same prefix as the native object payload. Water is a material
    // specialization; unrelated PBR/foliage shaders do not consume this tail.
    row_major float4x4 foliagePreviousViewProj;
    float4 foliageMotionDepth;
    float4 waterSurfaceParams[8];
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
Texture2D shadowMapTex          : register(t5);
TextureCube envMapTex           : register(t6);
Texture2D brdfLutTex            : register(t7);
TextureCube irradianceTex       : register(t8);
Texture2D lightmapTex           : register(t9);

#include "../common/ze_light_types.hlsli"
StructuredBuffer<GpuLocalLight> localLights : register(t10);
StructuredBuffer<uint> tileLightIndexBuffer : register(t11);
StructuredBuffer<uint2> tileLightOffsetCountBuffer : register(t12);
Texture2D screenAoTex : register(t13);
StructuredBuffer<float4> zgiProbeIrradiance : register(t14);
StructuredBuffer<float4> zgiProbeDistanceMoments : register(t15);
Texture2D<float4> giGrid:register(t16);
Texture2D<float4> localRadiance:register(t17);
cbuffer VisualProbeConstants:register(b3){float4 volumeData[128];float4 visualProbeParams;float4 visualCamera;}

SamplerState baseSampler        : register(s0);
SamplerState anisoSampler       : register(s1);
SamplerState clampSampler       : register(s2);
#define linearClamp clampSampler
#include "../common/visual_probe_sampling.hlsli"
#undef linearClamp
#include "../common/forward_reflections.hlsli"
#ifdef ZE_FORWARD_RAYS
#include "../raytracing/surface_rays.hlsl"
#endif

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
    float4 pos        : SV_POSITION;
    float3 worldPos   : POSITION;
    float3 worldNormal: NORMAL;
    float3 worldTangent : TANGENT;
    float3 worldBitan : BITANGENT;
    float2 uv         : TEXCOORD0;
    float2 uv1        : TEXCOORD1;
    float4 color      : COLOR0;
    float4 clipPos    : TEXCOORD5;
    float2 undeformedXZ : TEXCOORD6;
};

#include "../common/ze_grass_wind_v2.hlsli"
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
    #ifdef ZE_FOLIAGE_GRASS
    input.uv.x += input.instanceRow1.w;
    #endif
    input.color *= lerp(input.instanceColor0,input.instanceColor1,saturate(input.uv.y));
    inTangent.xyz = normalize(float3(dot(input.instanceRow0.xyz, inTangent.xyz),
                                     dot(input.instanceRow1.xyz, inTangent.xyz),
                                     dot(input.instanceRow2.xyz, inTangent.xyz)));
    inBitangent=cross(inNormal,inTangent.xyz)*inTangent.w;
#endif

    if (isSkinned > 0.5f)
    {
        float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
        float3 skinnedTangent = float3(0.0f, 0.0f, 0.0f);
        float3 skinnedBitangent = float3(0.0f, 0.0f, 0.0f);
        float totalWeight = 0.0f;
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
                totalWeight += w;
            }
        }
        if (totalWeight > 0.001f)
        {
            inPos = skinnedPos.xyz;
            inNormal = skinnedNormal;
            inTangent.xyz = skinnedTangent;
            inBitangent = skinnedBitangent;
        }
    }

    PSInput output;
    float4 wPos = mul(float4(inPos, 1.0f), world);
    float plantSlope;
    wPos.xyz = ZeTreeVertex(wPos.xyz,
                            plantWeight,
                            plantRoot,
                            plantHeight,
                            grassWindDirectionSpeed,
                            grassWindParams,
                            foliageWindField,
                            foliageWindGeometry,
                            foliageWindClimate,
                            plantSlope);
    output.undeformedXZ=wPos.xz;
#ifdef ZE_FOLIAGE_GRASS
    float bendSlope;
    wPos.xyz = ZeGrassVertex(wPos.xyz, inPos.y, input.uv, cameraPos.xyz,
                            max(alphaCutoff*180.0,20.0), grassWindDirectionSpeed, grassWindParams, foliageWindField, bendSlope);
    inNormal.y -= dot(inNormal.xz, grassWindDirectionSpeed.xz) * bendSlope;
#endif
    output.pos        = ZeProjectCamera(wPos, viewProj, float2(lightDir.w, lightColor.w));
    output.worldPos   = wPos.xyz;
    output.worldNormal  = ZeTransformNormal(inNormal, (float3x3)world);
    if (foliageWindGeometry.z > 0) {
        output.worldNormal.y -= dot(output.worldNormal.xz, grassWindDirectionSpeed.xz) * plantSlope;
        output.worldNormal = normalize(output.worldNormal);
    }
    output.worldTangent = normalize(mul(float4(inTangent.xyz,  0.0f), world).xyz);
    output.worldBitan   = normalize(mul(float4(inBitangent,    0.0f), world).xyz);
    output.uv    = input.uv;
    output.uv1   = input.uv1;
    output.color = input.color * objectColor;
    output.clipPos = output.pos;
    return output;
}

float4x4 InverseMatrix(float4x4 m) {
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3];
    float a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3];

    float b00 = a00 * a11 - a01 * a10;
    float b01 = a00 * a12 - a02 * a10;
    float b02 = a00 * a13 - a03 * a10;
    float b03 = a01 * a12 - a02 * a11;
    float b04 = a01 * a13 - a03 * a11;
    float b05 = a02 * a13 - a03 * a12;
    float b06 = a20 * a31 - a21 * a30;
    float b07 = a20 * a32 - a22 * a30;
    float b08 = a20 * a33 - a23 * a30;
    float b09 = a21 * a32 - a22 * a31;
    float b10 = a21 * a33 - a23 * a31;
    float b11 = a22 * a33 - a23 * a32;

    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

    if (abs(det) < 1e-8f) {
        return m;
    }

    float invDet = 1.0f / det;

    float4x4 dst;
    dst[0][0] = (a11 * b11 - a12 * b10 + a13 * b09) * invDet;
    dst[0][1] = (a02 * b10 - a01 * b11 - a03 * b09) * invDet;
    dst[0][2] = (a31 * b05 - a32 * b04 + a33 * b03) * invDet;
    dst[0][3] = (a22 * b04 - a21 * b05 - a23 * b03) * invDet;

    dst[1][0] = (a12 * b08 - a10 * b11 - a13 * b07) * invDet;
    dst[1][1] = (a00 * b11 - a02 * b08 + a03 * b07) * invDet;
    dst[1][2] = (a32 * b02 - a30 * b05 - a33 * b01) * invDet;
    dst[1][3] = (a20 * b05 - a22 * b02 + a23 * b01) * invDet;

    dst[2][0] = (a10 * b10 - a11 * b08 + a13 * b06) * invDet;
    dst[2][1] = (a01 * b08 - a00 * b10 - a03 * b06) * invDet;
    dst[2][2] = (a30 * b04 - a31 * b02 + a33 * b00) * invDet;
    dst[2][3] = (a21 * b02 - a20 * b04 - a23 * b00) * invDet;

    dst[3][0] = (a11 * b07 - a10 * b09 - a12 * b06) * invDet;
    dst[3][1] = (a00 * b09 - a01 * b07 + a02 * b06) * invDet;
    dst[3][2] = (a31 * b01 - a30 * b03 - a32 * b00) * invDet;
    dst[3][3] = (a20 * b03 - a21 * b01 + a22 * b00) * invDet;

    return dst;
}

// ACES filmic tone mapping curve — Stephen Hill fit
float3 ACESFilmic(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x*(a*x+b)) / (x*(c*x+d)+e));
}

float Luminance(float3 color)
{
    return dot(max(color, float3(0.0f, 0.0f, 0.0f)), float3(0.2126f, 0.7152f, 0.0722f));
}

float3 ApplyDiffuseGiContrastGuard(float3 rawProbeDiffuse, float3 directDiffuse)
{
    float probeLuma = Luminance(rawProbeDiffuse);
    if (probeLuma <= 0.00001f) {
        return rawProbeDiffuse;
    }

    float directLuma = Luminance(directDiffuse);
    float shadeWeight = 1.0f - saturate(directLuma / 0.55f);
    float allowedRatio = lerp(0.28f, 1.15f, shadeWeight);
    float allowedLuma = 0.02f + directLuma * allowedRatio + 0.18f * shadeWeight;
    if (probeLuma <= allowedLuma) {
        return rawProbeDiffuse;
    }

    return rawProbeDiffuse * (allowedLuma / probeLuma);
}

uint ProbeVolumeIndex(uint x, uint y, uint z, uint3 counts)
{
    return z * counts.y * counts.x + y * counts.x + x;
}

float4 SampleProbeVolume(float3 worldPos)
{
    if (zgiProbeCountsEnabled.w > 0.5f) {
        uint3 counts = max((uint3)zgiProbeCountsEnabled.xyz, uint3(1, 1, 1));
        float3 spacing = max(zgiProbeSpacing.xyz, float3(0.0001f, 0.0001f, 0.0001f));
        float3 minBounds = zgiVolumeOrigin.xyz;
        float3 maxBounds = minBounds + spacing * (float3)(counts - 1);
        if (all(worldPos >= minBounds) && all(worldPos <= maxBounds)) {
            float3 gridPosition = (worldPos - minBounds) / spacing;
            uint3 c0 = (uint3)floor(gridPosition);
            uint3 c1 = min(c0 + uint3(1, 1, 1), counts - 1);
            float3 blend = saturate(gridPosition - floor(gridPosition));
            float3 accumulatedIrradiance = 0.0f;
            float accumulatedWeight = 0.0f;

            [unroll]
            for (uint z = 0; z < 2; ++z) {
                [unroll]
                for (uint y = 0; y < 2; ++y) {
                    [unroll]
                    for (uint x = 0; x < 2; ++x) {
                        uint3 coord = uint3(
                            x == 0 ? c0.x : c1.x,
                            y == 0 ? c0.y : c1.y,
                            z == 0 ? c0.z : c1.z);
                        float weight =
                            (x == 0 ? 1.0f - blend.x : blend.x) *
                            (y == 0 ? 1.0f - blend.y : blend.y) *
                            (z == 0 ? 1.0f - blend.z : blend.z);
                        uint probeIndex = ProbeVolumeIndex(coord.x, coord.y, coord.z, counts);
                        float4 irradiance = zgiProbeIrradiance[probeIndex];
                        float4 distanceMoments = zgiProbeDistanceMoments[probeIndex];
                        float3 probePosition = minBounds + float3(coord) * spacing;
                        float receiverDistance = length(worldPos - probePosition);
                        float meanDistance = distanceMoments.x;
                        float variance = max(distanceMoments.y - meanDistance * meanDistance, 0.0001f);
                        float delta = max(receiverDistance - meanDistance, 0.0f);
                        float visibility = receiverDistance <= meanDistance
                            ? 1.0f
                            : variance / (variance + delta * delta);
                        weight *= visibility * irradiance.a;
                        accumulatedIrradiance += irradiance.rgb * weight;
                        accumulatedWeight += weight;
                    }
                }
            }

            if (accumulatedWeight > 0.0001f) {
                return float4(
                    accumulatedIrradiance / accumulatedWeight,
                    saturate(accumulatedWeight) * zgiSamplingParams.z);
            }
        }
    }

    if (probeVolumeCountsEnabled.w < 0.5f) {
        return float4(probeAmbientR, probeAmbientG, probeAmbientB, probeAmbientIntensity);
    }

    float3 minB = probeVolumeMin.xyz;
    float3 maxB = probeVolumeMax.xyz;
    float3 extent = max(maxB - minB, float3(0.0001f, 0.0001f, 0.0001f));
    if (any(worldPos < minB) || any(worldPos > maxB)) {
        return float4(probeAmbientR, probeAmbientG, probeAmbientB, probeAmbientIntensity);
    }

    uint3 counts = max((uint3)probeVolumeCountsEnabled.xyz, uint3(1, 1, 1));
    float3 f = ((worldPos - minB) / extent) * (float3)counts - 0.5f;
    float3 baseCoord = floor(f);
    float3 t = saturate(f - baseCoord);

    uint3 c0 = (uint3)clamp(baseCoord, float3(0.0f, 0.0f, 0.0f), (float3)(counts - 1));
    uint3 c1 = min(c0 + uint3(1, 1, 1), counts - 1);

    float4 p000 = probeVolumeProbes[ProbeVolumeIndex(c0.x, c0.y, c0.z, counts)];
    float4 p100 = probeVolumeProbes[ProbeVolumeIndex(c1.x, c0.y, c0.z, counts)];
    float4 p010 = probeVolumeProbes[ProbeVolumeIndex(c0.x, c1.y, c0.z, counts)];
    float4 p110 = probeVolumeProbes[ProbeVolumeIndex(c1.x, c1.y, c0.z, counts)];
    float4 p001 = probeVolumeProbes[ProbeVolumeIndex(c0.x, c0.y, c1.z, counts)];
    float4 p101 = probeVolumeProbes[ProbeVolumeIndex(c1.x, c0.y, c1.z, counts)];
    float4 p011 = probeVolumeProbes[ProbeVolumeIndex(c0.x, c1.y, c1.z, counts)];
    float4 p111 = probeVolumeProbes[ProbeVolumeIndex(c1.x, c1.y, c1.z, counts)];

    float4 x00 = lerp(p000, p100, t.x);
    float4 x10 = lerp(p010, p110, t.x);
    float4 x01 = lerp(p001, p101, t.x);
    float4 x11 = lerp(p011, p111, t.x);
    float4 y0 = lerp(x00, x10, t.y);
    float4 y1 = lerp(x01, x11, t.y);
    return lerp(y0, y1, t.z);
}

#if defined(DREAMMARE_GROUND) || defined(DREAMMARE_WATER)
#include "../common/dreammare_surface.hlsli"
#endif
// Interpolate comparison results, never filtered depths. Each of the four
// samples follows the receiver plane, keeping sloped ground free of PCF acne.
float ZeBilinearShadowCompare(float2 uv, float2 receiverUv, float2 size,
    float depth, float2 gradient, float bias, float2 minUv, float2 maxUv) {
    float2 pixel = uv * size - 0.5f;
    float2 origin = floor(pixel), blend = frac(pixel);
    float result = 0.0f;
    [unroll] for (int y=0; y<2; ++y) [unroll] for (int x=0; x<2; ++x) {
        float2 tapUv = clamp((origin + float2(x,y) + 0.5f) / size, minUv, maxUv);
        int2 tap = clamp(int2(tapUv*size), int2(0,0), int2(size)-1);
        tapUv = (float2(tap)+0.5f)/size;
        float receiver = ZeShadowReceiverTapDepth(depth, gradient, tapUv-receiverUv);
        float weight = (x ? blend.x : 1.0f-blend.x) * (y ? blend.y : 1.0f-blend.y);
        result += weight * (receiver-bias <= shadowMapTex.Load(int3(tap,0)).r ? 1.0f : 0.0f);
    }
    return result;
}

#ifdef ZE_TERRAIN_LAYERS
#include "../common/terrain_layers.hlsli"
#endif
#include "../common/ze_virtual_texture.hlsli"
float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // 1. Fetch Textures
    float4 albedo = input.color;
    #ifndef ZE_TERRAIN_LAYERS
    if (hasBaseColorTexture > 0.5f) {
        albedo *= baseColorTex.Sample(anisoSampler, input.uv);
    }
#endif

    // MASK is alpha-tested. BLEND is handled by a transparent PSO with depth writes disabled.
    if (alphaMode > 0.5f && alphaMode < 1.5f) {
        if (albedo.a < alphaCutoff) discard;
    }

    float3 N = normalize(input.worldNormal);
#ifdef ZE_FOLIAGE_GRASS
    albedo.rgb = pow(saturate(albedo.rgb),2.2);
    float3 groundColor;
    if(ZeSampleVirtualColor(emissiveTex,occlusionTex,clampSampler,input.undeformedXZ,virtualTextureParams,groundColor)) {
        float height=saturate(input.uv.y);
        // Root inherits the ground; tips retain a soft authored highlight.
        float blend=virtualTextureParams.w*lerp(1,.65,height);
        albedo.rgb=lerp(albedo.rgb,groundColor*lerp(.88,1.12,height),blend);
    }
    // Apply the authored root shade after VT so terrain hue cannot erase it.
    float rootCover=1-smoothstep(0,.72,saturate(input.uv.y));
    albedo.rgb *= 1-saturate(terrainSurfaceParams.x)*rootCover;
#endif

    #ifndef ZE_TERRAIN_LAYERS
    if (hasNormalMap > 0.5f) {
        float4 normalSample = normalMapTex.Sample(anisoSampler, input.uv);
        N = DecodeWorldNormalFromMap(normalSample, input.worldNormal, input.worldTangent, input.worldBitan);
    }
#endif

    bool thinSurface=false;
    float2 thin=0;
#if !defined(ZE_TERRAIN_LAYERS) && !defined(ZE_FOLIAGE_GRASS) && !defined(DREAMMARE_WATER) && !defined(DREAMMARE_GROUND)
    thinSurface=terrainSurfaceParams.w<-.5;
    if(thinSurface) {
        thin=ZeThinQuantize(terrainSurfaceParams.xy);
        N=ZeThinNormal(N,terrainSurfaceParams.z);
    }
#endif
    if (doubleSided > 0.5f && !thinSurface && dot(N,cameraPos.xyz-input.worldPos)<0) {
        N = -N;
    }
#ifdef ZE_FOLIAGE_GRASS
    // A soft upright normal keeps thin folded blades readable on both faces.
    N = normalize(N + float3(0,terrainSurfaceParams.w,0));
#endif

    // PBR Roughness/Metallic
    float rough = roughness;
    float metal = metallic;
    #ifndef ZE_TERRAIN_LAYERS
    if (hasMetallicRoughnessTexture > 0.5f) {
        float4 mrSample = metallicRoughnessTex.Sample(anisoSampler, input.uv);
        rough *= mrSample.g; // Green channel is roughness
        metal *= mrSample.b; // Blue channel is metallic
    }
#endif
    rough = saturate(rough);
    metal = saturate(metal);
#if defined(DREAMMARE_GROUND) || defined(DREAMMARE_WATER)
    DreamSurface(input.worldPos,albedo.rgb,N,rough);
#endif

    // Occlusion
    float ao = 1.0f;
    if (hasOcclusionTexture > 0.5f) {
        ao = occlusionTex.Sample(anisoSampler, input.uv).r;
    }

    #ifdef ZE_TERRAIN_LAYERS
    ZeTerrainSurface(input,albedo,N,rough,metal,ao);
    #endif

    // Emissive contribution
    float3 emissive = emissiveColor.rgb * emissiveStrength;
    if (hasEmissiveMap > 0.5f) {
        emissive *= emissiveTex.Sample(anisoSampler, input.uv).rgb;
    }

    // Lightmap contribution (baked indirect lighting)
    float3 lightmapContrib = float3(0.0f, 0.0f, 0.0f);
    if (hasLightmap > 0.5f) {
        float3 lmColor = lightmapTex.Sample(clampSampler, input.uv1).rgb;
        lightmapContrib = lmColor * lightmapIntensity;
 
        // Optionally blend lightmap as occlusion replacement
        float lmLuminance = dot(lmColor, float3(0.2126f, 0.7152f, 0.0722f));
        ao = lerp(ao, lmLuminance, lightmapOcclusionBlend);
    }

    bool finalNoAoDebug = lightingDebugMode > 23.5f && lightingDebugMode < 24.5f;
    if (aoEnabled > 0.5f && !finalNoAoDebug) {
        float2 screenUv = input.pos.xy / max(float2(viewportWidth, viewportHeight), float2(1.0f, 1.0f));
        float screenAo = screenAoTex.Sample(clampSampler, screenUv).r;
        ao *= saturate(screenAo);
    }
    if (finalNoAoDebug) {
        ao = 1.0f;
    }

    rough = ZeFilterRoughness(rough, ddx(N), ddy(N));

    // Build unified SurfaceData
    SurfaceData surface;
    surface.baseColor = albedo.rgb;
    surface.normalWS = N;
    surface.metallic = metal;
    surface.perceptualRoughness = rough;
    surface.ambientOcclusion = ao;
    surface.emissive = emissive;
    surface.shadingModel = 1; // DefaultLit
    surface.flags = 0;

    // 2. Lighting calculation
    float3 V = normalize(cameraPos.xyz - input.worldPos);
    float3 L = normalize(-lightDir.xyz);

    BrdfInput brdfInput;
    float3 directN = surface.normalWS;
    brdfInput.N = directN;
    brdfInput.V = V;
    brdfInput.L = L;
    brdfInput.baseColor = surface.baseColor;
    brdfInput.metallic = surface.metallic;
    brdfInput.perceptualRoughness = surface.perceptualRoughness;

    BrdfResult brdfResult = EvaluateThinSurfaceBrdf(brdfInput,thin);
    float3 diffuseLighting = brdfResult.diffuse * lightColor.rgb;
    float3 specularLighting = brdfResult.specular * lightColor.rgb;

    // 2.5 Shadow calculation
    float shadowFactor = 1.0f;
    float closestDepth = 1.0f;
    float currentDepth = 0.0f;
    float bias = 0.0f;
    float2 shadowUV = float2(0.0f, 0.0f);
    float2 shadowPageLocalUV = float2(0.0f, 0.0f);
    uint shadowPageIndex = 0;

    if (shadowEnabled > 0.5f) {
        uint activeShadowPageCount = min((uint)shadowPageCount, 4u);
        row_major float4x4 activeLightViewProj = lightViewProj;
        float4 activePageScaleBias = float4(1.0f, 1.0f, 0.0f, 0.0f);
        uint shadowWidth, shadowHeight;
        shadowMapTex.GetDimensions(shadowWidth, shadowHeight);
        float2 shadowSize = max(float2(shadowWidth, shadowHeight), 1.0f);
        float2 texelSize = 1.0f / shadowSize;
        if (shadowPageEnabled > 0.5f && activeShadowPageCount > 0) {
            float receiverDistance = abs(viewProj[3][3] - 1.0f) < 0.001f
                ? distance(input.worldPos, cameraPos.xyz)
                : mul(float4(input.worldPos, 1.0f), viewProj).w;
            [unroll]
            for (uint pageIndex = 0; pageIndex < 3; ++pageIndex) {
                if (pageIndex + 1 < activeShadowPageCount &&
                    receiverDistance > shadowCascadeSplits[pageIndex]) {
                    shadowPageIndex = pageIndex + 1;
                }
            }
            activeLightViewProj = shadowPageViewProj[shadowPageIndex];
            activePageScaleBias = shadowPageScaleBias[shadowPageIndex];
            texelSize = max(shadowAtlasInvSize, float2(0.000001f, 0.000001f));
        }

        float4 rawLightSpacePos = mul(float4(input.worldPos, 1.0f), activeLightViewProj);
        float3 rawProjected = rawLightSpacePos.xyz / rawLightSpacePos.w;
        float2 rawUv = (rawProjected.xy * float2(0.5f, -0.5f) + 0.5f) * activePageScaleBias.xy + activePageScaleBias.zw;
        float2 receiverGradient = ZeShadowPlaneDepthGradient(
            normalize(input.worldNormal),activeLightViewProj,activePageScaleBias.xy);
        float3 projectionX = float3(activeLightViewProj[0][0], activeLightViewProj[1][0], activeLightViewProj[2][0]);
        float3 projectionY = float3(activeLightViewProj[0][1], activeLightViewProj[1][1], activeLightViewProj[2][1]);
        float2 pageTexelSize = texelSize / max(activePageScaleBias.xy, 1e-6f);
        float texelWorldSize = max(2.0f * pageTexelSize.x / max(length(projectionX), 1e-6f),
                                  2.0f * pageTexelSize.y / max(length(projectionY), 1e-6f));
        float ndotl = saturate(dot(surface.normalWS, L));
        float normalOffset = clamp(shadowNormalBias, 0.0f, 2.0f) * texelWorldSize * (1.0f - ndotl);
        float3 biasedPosition = input.worldPos + surface.normalWS * normalOffset;
        float4 lightSpacePos = mul(float4(biasedPosition, 1.0f), activeLightViewProj);
        float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
        shadowPageLocalUV = projCoords.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
        shadowUV = shadowPageLocalUV * activePageScaleBias.xy + activePageScaleBias.zw;
        
        if (saturate(shadowPageLocalUV.x) == shadowPageLocalUV.x &&
            saturate(shadowPageLocalUV.y) == shadowPageLocalUV.y &&
            projCoords.z >= 0.0f &&
            projCoords.z <= 1.0f) {
            currentDepth = projCoords.z;
            bias = shadowConstantBias + shadowSlopeBias * saturate(1.0f - ndotl);
            float2 pageMinUv = activePageScaleBias.zw + texelSize * 2.0f;
            float2 pageMaxUv = activePageScaleBias.zw + activePageScaleBias.xy - texelSize * 2.0f;
            
            if (abs(shadowPcfRadius-1.0f)<1e-6f &&
                all(shadowUV >= pageMinUv + texelSize*2.5f) &&
                all(shadowUV <= pageMaxUv - texelSize*2.5f)) {
                shadowFactor=ZeGaussianShadowUnitSpacing(shadowMapTex,shadowUV,
                    shadowSize,currentDepth,receiverGradient,bias);
                int2 texel=clamp(int2(floor(shadowUV*shadowSize)),int2(0,0),int2(shadowSize)-1);
                closestDepth=shadowMapTex.Load(int3(texel,0)).r;
            } else if (shadowPcfRadius >= 1.0f) {
                float shadowSum = 0.0f;
                float totalWeight = 0.0f;

                // 5x5 Gaussian kernel weights
                static const float kernel[5] = {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f};

                for (int x = -2; x <= 2; ++x) {
                    for (int y = -2; y <= 2; ++y) {
                        float w = kernel[x+2] * kernel[y+2];
                        float2 sampleUv = clamp(
                            shadowUV + float2(x, y) * texelSize * shadowPcfRadius,
                            pageMinUv,
                            pageMaxUv);
                        shadowSum += w * ZeBilinearShadowCompare(sampleUv, shadowUV,
                            shadowSize, currentDepth, receiverGradient, bias, pageMinUv, pageMaxUv);
                        totalWeight += w;
                    }
                }
                shadowFactor = shadowSum / totalWeight;
                int2 texel = clamp(int2(floor(clamp(shadowUV, pageMinUv, pageMaxUv) * shadowSize)), int2(0, 0), int2(shadowSize) - 1);
                closestDepth = shadowMapTex.Load(int3(texel, 0)).r;
            } else {
                int2 texel = clamp(int2(floor(clamp(shadowUV, pageMinUv, pageMaxUv) * shadowSize)), int2(0, 0), int2(shadowSize) - 1);
                closestDepth = shadowMapTex.Load(int3(texel, 0)).r;
                shadowFactor = ZeBilinearShadowCompare(shadowUV, shadowUV, shadowSize,
                    currentDepth, receiverGradient, bias, pageMinUv, pageMaxUv);
            }
        }
        shadowFactor = lerp(1.0f, shadowFactor, shadowStrength);
    }

    // 2.6 Local Light Culling and Evaluation
    float3 localDiffuseLighting = float3(0.0f, 0.0f, 0.0f);
    float3 localSpecularLighting = float3(0.0f, 0.0f, 0.0f);
    float3 localLightBoundsColor = float3(0.0f, 0.0f, 0.0f);
    float3 pointDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 pointSpecular = float3(0.0f, 0.0f, 0.0f);
    float3 spotDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 spotSpecular = float3(0.0f, 0.0f, 0.0f);

    uint offset = 0;
    uint count = 0;
    uint tilesX = (uint)numTilesX;
    uint tilesY = (uint)numTilesY;
    uint tileSize = max((uint)lightGridTileSize, 1u);
    uint2 pixelPos = uint2(input.pos.xy);
    uint2 tileId = uint2(0, 0);
    if (lightGridEnabled > 0.5f && tilesX > 0 && tilesY > 0) {
        tileId = min(pixelPos / tileSize, uint2(tilesX - 1, tilesY - 1));
        uint tileIdx = tileId.y * tilesX + tileId.x;
        uint2 offsetCount = tileLightOffsetCountBuffer[tileIdx];
        offset = offsetCount.x;
        count = offsetCount.y;
    }

    uint clampedCount = min(count, (uint)maxLightsPerTile);
    for (uint i = 0; i < clampedCount; ++i) {
        uint lightIdx = tileLightIndexBuffer[offset + i];
        if (lightIdx >= (uint)numLocalLights) {
            continue;
        }
        GpuLocalLight light = localLights[lightIdx];
        
        float3 lightPos = light.positionRange.xyz;
        float range = light.positionRange.w;
        float3 lightColorLinear = light.colorIntensity.xyz;
        float intensity = light.colorIntensity.w;
        
        float3 toLight = lightPos - input.worldPos;
        float dist = length(toLight);
        
        // Accumulate bounds visualization
        float boundsFactor = saturate(1.0f - abs(dist - range) * 1.5f);
        localLightBoundsColor += boundsFactor * float3(1.0f, 1.0f, 0.0f);

        if (dist > range) continue;
        
        float3 localL = toLight / dist;
        
        // Physically-motivated attenuation: inverse-square with smooth finite-radius cutoff
        float attenuation = saturate(1.0f - pow(dist / range, 4.0f));
        attenuation = (attenuation * attenuation) / (dist * dist + 1.0f);
        
        // Spot cone attenuation
        float type = light.directionType.w;
        if (type == 2.0f) { // Spot light
            float3 spotDir = normalize(light.directionType.xyz);
            float cosAngle = dot(-localL, spotDir);
            float innerCone = light.coneRectFlags.x;
            float outerCone = light.coneRectFlags.y;
            float coneAttenuation = saturate((cosAngle - outerCone) / max(innerCone - outerCone, 0.0001f));
            attenuation *= coneAttenuation * coneAttenuation;
        }
        
        float3 radiance = lightColorLinear * intensity * attenuation;
        
        BrdfInput localBrdfInput;
        float3 localN = surface.normalWS;

        localBrdfInput.N = localN;
        localBrdfInput.V = V;
        localBrdfInput.L = localL;
        localBrdfInput.baseColor = surface.baseColor;
        localBrdfInput.metallic = surface.metallic;
        localBrdfInput.perceptualRoughness = surface.perceptualRoughness;
        
        BrdfResult localBrdfResult = EvaluateThinSurfaceBrdf(localBrdfInput,thin);
        
        if (type == 1.0f) { // Point
            pointDiffuse += localBrdfResult.diffuse * radiance;
            pointSpecular += localBrdfResult.specular * radiance;
        } else if (type == 2.0f) { // Spot
            spotDiffuse += localBrdfResult.diffuse * radiance;
            spotSpecular += localBrdfResult.specular * radiance;
        }
    }

    localDiffuseLighting = pointDiffuse + spotDiffuse;
    localSpecularLighting = pointSpecular + spotSpecular;

    // Brute-force reference loop for difference visualization
    float3 bruteForceDiff = float3(0.0f, 0.0f, 0.0f);
    float3 bruteForceSpec = float3(0.0f, 0.0f, 0.0f);
    if (lightingDebugMode > 20.5f && lightingDebugMode < 21.5f) {
        for (uint l = 0; l < (uint)numLocalLights; ++l) {
            GpuLocalLight light = localLights[l];
            float3 lightPos = light.positionRange.xyz;
            float range = light.positionRange.w;
            float3 lightColorLinear = light.colorIntensity.xyz;
            float intensity = light.colorIntensity.w;
            
            float3 toLight = lightPos - input.worldPos;
            float dist = length(toLight);
            if (dist > range) continue;
            
            float3 localL = toLight / dist;
            
            float attenuation = saturate(1.0f - pow(dist / range, 4.0f));
            attenuation = (attenuation * attenuation) / (dist * dist + 1.0f);
            
            float type = light.directionType.w;
            if (type == 2.0f) {
                float3 spotDir = normalize(light.directionType.xyz);
                float cosAngle = dot(-localL, spotDir);
                float innerCone = light.coneRectFlags.x;
                float outerCone = light.coneRectFlags.y;
                float coneAttenuation = saturate((cosAngle - outerCone) / max(innerCone - outerCone, 0.0001f));
                attenuation *= coneAttenuation * coneAttenuation;
            }
            
            float3 radiance = lightColorLinear * intensity * attenuation;
            
            BrdfInput localBrdfInput;
            float3 bruteN = surface.normalWS;
    
            localBrdfInput.N = bruteN;
            localBrdfInput.V = V;
            localBrdfInput.L = localL;
            localBrdfInput.baseColor = surface.baseColor;
            localBrdfInput.metallic = surface.metallic;
            localBrdfInput.perceptualRoughness = surface.perceptualRoughness;
            
            BrdfResult localBrdfResult = EvaluateThinSurfaceBrdf(localBrdfInput,thin);
            bruteForceDiff += localBrdfResult.diffuse * radiance;
            bruteForceSpec += localBrdfResult.specular * radiance;
        }
    }

    if (shadowDebugMode > 0.5f) {
        if (shadowDebugMode < 1.5f) {
            return float4(closestDepth, closestDepth, closestDepth, 1.0f);
        } else if (shadowDebugMode < 2.5f) {
            float cascadeT = shadowPageCount > 1.0f
                ? (float)shadowPageIndex / max(shadowPageCount - 1.0f, 1.0f)
                : 0.0f;
            return float4(cascadeT, 1.0f - cascadeT, 0.25f, 1.0f);
        } else if (shadowDebugMode < 3.5f) {
            bool inBounds = saturate(shadowUV.x) == shadowUV.x &&
                            saturate(shadowUV.y) == shadowUV.y &&
                            currentDepth >= 0.0f && currentDepth <= 1.0f;
            return inBounds ? float4(0.0f, 0.75f, 0.0f, 1.0f) : float4(1.0f, 0.0f, 0.0f, 1.0f);
        } else if (shadowDebugMode < 4.5f) {
            return float4(0.0f, 0.0f, 1.0f, 1.0f);
        } else if (shadowDebugMode < 5.5f) {
            return float4(bias * 100.0f, bias * 100.0f, bias * 100.0f, 1.0f);
        } else if (shadowDebugMode < 6.5f) {
            return float4(shadowFactor, shadowFactor, shadowFactor, 1.0f);
        } else if (shadowDebugMode < 7.5f) {
            bool outOfBounds = shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
                               shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
                               currentDepth < 0.0f || currentDepth > 1.0f;
            return outOfBounds ? float4(1.0f, 0.15f, 0.0f, 1.0f)
                               : float4(shadowUV, saturate(currentDepth), 1.0f);
        } else if (shadowDebugMode < 8.5f) {
            float edge = min(min(shadowUV.x, 1.0f - shadowUV.x), min(shadowUV.y, 1.0f - shadowUV.y));
            float boundary = 1.0f - smoothstep(0.0f, 0.02f, edge);
            return float4(boundary, shadowFactor, 0.0f, 1.0f);
        }
    }

    float3 ambient;
 
    if (iblEnabled > 0.5f) {
        // --- IBL Diffuse: sample irradiance cubemap ---
        float ec=cos(environmentParams.x), es=sin(environmentParams.x);
        float3 envN=float3(ec*surface.normalWS.x-es*surface.normalWS.z,
            surface.normalWS.y,es*surface.normalWS.x+ec*surface.normalWS.z);
        float3 irradiance = irradianceTex.SampleLevel(baseSampler, envN, 0).rgb;
        if(thin.y>0)irradiance=lerp(irradiance,irradianceTex.SampleLevel(baseSampler,-envN,0).rgb,thin.y*.5);

 
        // --- IBL Specular: split-sum approximation ---
        float3 R = reflect(-V, surface.normalWS);
        R=float3(ec*R.x-es*R.z,R.y,es*R.x+ec*R.z);
        float  mipLevel = surface.perceptualRoughness * envMapMaxMip;
        float3 prefilteredColor = envMapTex.SampleLevel(baseSampler, R, mipLevel).rgb;
 
        // BRDF LUT: x = NdotV, y = roughness
        float ndotv = saturate(dot(surface.normalWS, V));
        float2 brdfUV    = float2(saturate(ndotv), surface.perceptualRoughness);
        float2 brdfScale = brdfLutTex.Sample(clampSampler, brdfUV).rg;
 
        float3 F0  = lerp(float3(0.04f, 0.04f, 0.04f), surface.baseColor, surface.metallic);
        float f90  = saturate(dot(F0, 0.33333f) * 50.0f * (1.0f - surface.perceptualRoughness) + surface.metallic);
        float3 FIbl = F0 * brdfScale.x + f90 * brdfScale.y;
        float3 iblSpecular = prefilteredColor * FIbl;
        float maxIblSpec = lerp(4.0f, 100.0f, surface.metallic);
        iblSpecular = min(iblSpecular, maxIblSpec.xxx);
        float3 iblDiffuse = irradiance * ZeIndirectDiffuseWeight(surface.baseColor, surface.metallic, FIbl);
 
        ambient = (iblDiffuse + iblSpecular) * surface.ambientOcclusion * iblIntensity;
        ambient = max(ambient, surface.baseColor * minAmbientFloor);
    } else {
        // Fallback: simple sky color gradient
        float3 skyColor = lerp(skyHorizonColor.rgb, skyZenithColor.rgb,
                               saturate(surface.normalWS.y * 0.5f + 0.5f)) * skyIntensity;
        // Convert sky irradiance to Lambertian diffuse radiance, as for direct light.
        float2 skyDfg=brdfLutTex.SampleLevel(clampSampler,
            float2(saturate(dot(surface.normalWS,V)),surface.perceptualRoughness),0).rg;
        float3 skyF0=lerp(.04f.xxx,surface.baseColor,surface.metallic);
        float skyF90 = saturate(dot(skyF0, 0.33333f) * 50.0f * (1.0f - surface.perceptualRoughness) + surface.metallic);
        float3 skyResponse = saturate(skyF0 * skyDfg.x + skyF90 * skyDfg.y);
        ambient=ZeSkyAmbientResponse(skyColor,surface.baseColor,surface.metallic,
            skyResponse)*surface.ambientOcclusion;
        ambient = max(ambient, surface.baseColor * minAmbientFloor);
    }
 
    float3 localSpecularBaseline=-1;
    if(visualProbeParams.x>0&&lightingDebugMode<.5){
        float3 p=input.worldPos-visualCamera.xyz,N=surface.normalWS,R=reflect(-V,N),gi=0,reflections=0;float giWeight=0,reflectionWeight=0;
        [loop]for(uint i=0;i<uint(visualProbeParams.x);++i){uint offset=i*8;float3 local=Local(offset,p);if(any(local<0)||any(local>1))continue;
            float3 edge = min(local, 1 - local) * volumeData[offset + 4].xyz;
            float blend = volumeData[offset + 4].w;
            float weight = blend > 0 ? saturate(min(edge.x, min(edge.y, edge.z)) / blend) : 1;
            if(volumeData[offset+3].w<1.5){gi+=SampleGi(offset,local,p,N)*weight*volumeData[offset+6].z;giWeight+=weight;
            } else {
                float ready;
                float3 radiance = SampleProbe(offset, p, R, surface.perceptualRoughness, ready);
                reflections += radiance * weight * ready * volumeData[offset + 6].z;
                reflectionWeight += weight * ready;
            }
        }
        float3 f0 = lerp(.04.xxx, surface.baseColor, surface.metallic);
        float2 dfg = brdfLutTex
                         .SampleLevel(clampSampler,
                                      float2(saturate(dot(N, V)), surface.perceptualRoughness),
                                      0)
                         .rg;
        float f90 = saturate(dot(f0, 0.33333f) * 50.0f * (1.0f - surface.perceptualRoughness) + surface.metallic);
        float3 response = saturate(f0 * dfg.x + f90 * dfg.y);
        ambient+=gi/max(giWeight,1)*ZeIndirectDiffuseWeight(surface.baseColor,surface.metallic,response)*visualProbeParams.y*surface.ambientOcclusion;
        if(reflectionWeight>0){float3 baseline=lerp(skyHorizonColor.rgb,skyZenithColor.rgb,saturate(N.y*.5+.5))*skyIntensity/3.14159265;
            if (iblEnabled > .5) {
                float c = cos(environmentParams.x), s = sin(environmentParams.x);
                float3 d = float3(c * R.x - s * R.z, R.y, s * R.x + c * R.z);
                baseline =
                    envMapTex
                        .SampleLevel(baseSampler, d, surface.perceptualRoughness * envMapMaxMip)
                        .rgb *
                    iblIntensity;
            }
            ambient -= min(ambient, baseline * response * surface.ambientOcclusion) *
                       saturate(reflectionWeight);
            ambient += reflections / max(reflectionWeight, 1) * response * visualProbeParams.z *
                       surface.ambientOcclusion;
            localSpecularBaseline=baseline*(1-saturate(reflectionWeight))+reflections/max(reflectionWeight,1)*visualProbeParams.z;
        }
    }
    float4 surfaceReflection=lightingDebugMode<.5?ForwardScreenReflection(input.worldPos,surface.normalWS,V,surface.perceptualRoughness):0;
#ifdef ZE_FORWARD_RAYS
    if(options.w>0&&surface.perceptualRoughness<=options.z&&lightingDebugMode<.5){
        float3 p = input.worldPos - worldCamera.xyz, N = surface.normalWS, R = reflect(-V, N),
               sum = 0;
        float hits = 0;
        uint seed = uint(input.pos.x) + uint(input.pos.y) * uint(size.x) + metadata.y * 104729;
        [loop] for (uint sample = 0; sample < uint(options.w); ++sample) {
            bool hit;
            float distance;
            float3 radiance =
                Radiance(p + N * options.y,
                         Cone(R, surface.perceptualRoughness * surface.perceptualRoughness, seed),
                         seed,
                         hit,
                         distance);
            if (hit) {
                sum += radiance;
                hits += 1;
            }
        }
        if (hits > 0) {
            float coverage = hits / options.w;
            float combined = coverage + surfaceReflection.a * (1 - coverage);
            surfaceReflection = float4(
                (sum / options.w + surfaceReflection.rgb * surfaceReflection.a * (1 - coverage)) /
                    max(combined, .00001),
                combined);
        }
    }
#endif
    if (surfaceReflection.a > 0) {
        float3 N = surface.normalWS, R = reflect(-V, N);
        float2 dfg = brdfLutTex
                         .SampleLevel(clampSampler,
                                      float2(saturate(dot(N, V)), surface.perceptualRoughness),
                                      0)
                         .rg;
        float f90 = saturate(dot(lerp(.04.xxx, surface.baseColor, surface.metallic), 0.33333f) * 50.0f * (1.0f - surface.perceptualRoughness) + surface.metallic);
        float3 response =
            saturate(lerp(.04.xxx, surface.baseColor, surface.metallic) * dfg.x + f90 * dfg.y);
        float3 baseline = lerp(skyHorizonColor.rgb, skyZenithColor.rgb, saturate(N.y * .5 + .5)) *
                          skyIntensity / 3.14159265;
        if (iblEnabled > .5) {
            float c = cos(environmentParams.x), s = sin(environmentParams.x);
            float3 d = float3(c * R.x - s * R.z, R.y, s * R.x + c * R.z);
            baseline =
                envMapTex.SampleLevel(baseSampler, d, surface.perceptualRoughness * envMapMaxMip)
                    .rgb *
                iblIntensity;
        }
        if(localSpecularBaseline.x>=0)baseline=localSpecularBaseline;
        ambient=max(ambient-min(ambient,baseline*response)*surfaceReflection.a+surfaceReflection.rgb*surfaceReflection.a*response,0);
    }
    bool finalNoGiDebug = lightingDebugMode > 22.5f && lightingDebugMode < 23.5f;
    float3 probeContribution = float3(0.0f, 0.0f, 0.0f);

    // After ambient calculation, add probe contribution for dynamic objects
    if (!finalNoGiDebug && (
        probeAmbientIntensity > 0.001f ||
        probeVolumeCountsEnabled.w > 0.5f ||
        zgiProbeCountsEnabled.w > 0.5f)) {
        float4 probeSample = SampleProbeVolume(input.worldPos);
        float3 probeColor = probeSample.rgb;
        float3 rawProbeContribution = probeColor * probeSample.a * surface.baseColor * (1.0f - surface.metallic) * surface.ambientOcclusion;
        float3 directDiffuseForGuard = diffuseLighting * shadowFactor + localDiffuseLighting;
        probeContribution = ApplyDiffuseGiContrastGuard(rawProbeContribution, directDiffuseForGuard);
        ambient += probeContribution;
    }
 
    float3 lightmapFinal = finalNoGiDebug ? float3(0.0f, 0.0f, 0.0f) : lightmapContrib * surface.baseColor;
    float3 indirectLighting = ambient + lightmapFinal;
    float3 directLighting = (diffuseLighting + specularLighting) * shadowFactor
                           + (localDiffuseLighting + localSpecularLighting);
#ifdef ZE_FOLIAGE_GRASS
    // Analytic tuft occlusion supplements screen AO; transmission follows the sun.
    indirectLighting *= 1-saturate(terrainSurfaceParams.y)*rootCover;
    float backlight=pow(saturate(dot(-L,V)),2);
    float thinLeaf=1-abs(dot(normalize(input.worldNormal),L));
    directLighting += surface.baseColor*lightColor.rgb*(1.0/3.14159265)
        *saturate(terrainSurfaceParams.z)*backlight*lerp(.35,1,thinLeaf)
        *smoothstep(0,.55,saturate(input.uv.y))*shadowFactor;
#endif

    // Direct diffuse and specular are NOT attenuated by AO (resolving Direct Lighting AO Bug)
    float3 finalColor = indirectLighting
                      + directLighting
                      + surface.emissive;
 
    if (giDebugMode > 0.5f) {
        if (giDebugMode < 1.5f) { // lightmap
            if (hasLightmap > 0.5f) {
                float3 lmColor = lightmapTex.Sample(clampSampler, input.uv1).rgb * lightmapIntensity;
                return float4(lmColor, 1.0f);
            } else {
                return float4(0.0f, 0.0f, 0.0f, 1.0f);
            }
        } else if (giDebugMode < 2.5f) { // uv
            return float4(input.uv1, 0.0f, 1.0f);
        } else if (giDebugMode > 4.5f && giDebugMode < 5.5f) { // dynamic_sample
            return float4(SampleProbeVolume(input.worldPos).rgb, 1.0f);
        }
    }

    // 2.7 Debug views
    if (lightingDebugMode > 0.5f) {
        if (lightingDebugMode < 1.5f) { // BaseColor (1)
            return float4(surface.baseColor, albedo.a);
        }
        else if (lightingDebugMode < 2.5f) { // Normal (2)
            return float4(surface.normalWS * 0.5f + 0.5f, 1.0f);
        }
        else if (lightingDebugMode < 3.5f) { // Roughness (3)
            return float4(surface.perceptualRoughness, surface.perceptualRoughness, surface.perceptualRoughness, 1.0f);
        }
        else if (lightingDebugMode < 4.5f) { // Metallic (4)
            return float4(surface.metallic, surface.metallic, surface.metallic, 1.0f);
        }
        else if (lightingDebugMode < 5.5f) { // AmbientOcclusion (5)
            return float4(surface.ambientOcclusion, surface.ambientOcclusion, surface.ambientOcclusion, 1.0f);
        }
        else if (lightingDebugMode < 6.5f) { // Emissive (6)
            return float4(surface.emissive, 1.0f);
        }
        else if (lightingDebugMode < 7.5f) { // Depth (7)
            float d = input.pos.z; // SV_Position z is depth in [0, 1]
            return float4(d, d, d, 1.0f);
        }
        else if (lightingDebugMode < 8.5f) { // ReconstructionError (8)
            float4x4 invViewProj = InverseMatrix(viewProj);
            float4 ndc = float4(input.clipPos.xyz / input.clipPos.w, 1.0f);
            ndc.xy -= float2(lightDir.w, lightColor.w);
            float4 reconstructed = mul(ndc, invViewProj);
            float3 reconstructedWS = reconstructed.xyz / reconstructed.w;
            float error = distance(reconstructedWS, input.worldPos);
            // Visual scale: black for 0 error, red for 0.05 or higher
            return float4(error * 20.0f, 0.0f, 0.0f, 1.0f);
        }
        else if (lightingDebugMode < 9.5f) { // DirectDiffuse (9)
            return float4(diffuseLighting, 1.0f);
        }
        else if (lightingDebugMode < 10.5f) { // DirectSpecular (10)
            return float4(specularLighting, 1.0f);
        }
        else if (lightingDebugMode < 11.5f) { // DirectionalNdotL (11)
            float ndotl = saturate(dot(surface.normalWS, L));
            return float4(ndotl, ndotl, ndotl, 1.0f);
        }
        else if (lightingDebugMode < 12.5f) { // RawHDR (12)
            return float4(finalColor, albedo.a);
        }
        else if (lightingDebugMode < 13.5f) { // local_light_bounds (13)
            return float4(localLightBoundsColor, 1.0f);
        }
        else if (lightingDebugMode < 14.5f) { // tile_or_cluster_id (14)
            float3 tileColor = float3(
                frac(sin(tileId.x * 12.9898f) * 43758.5453f),
                frac(sin(tileId.y * 78.233f) * 43758.5453f),
                frac(sin((tileId.x + tileId.y) * 45.164f) * 43758.5453f)
            );
            bool isBorder = (pixelPos.x % tileSize == 0) || (pixelPos.y % tileSize == 0);
            if (isBorder) tileColor = float3(1.0f, 1.0f, 1.0f);
            return float4(tileColor, 1.0f);
        }
        else if (lightingDebugMode < 15.5f) { // light_count_heatmap (15)
            float3 heatmap = float3(0.0f, 0.0f, 0.0f);
            if (count > 0) {
                float t = saturate((float)count / 16.0f);
                heatmap = lerp(float3(0.0f, 0.0f, 1.0f), float3(0.0f, 1.0f, 0.0f), saturate(t * 2.0f));
                heatmap = lerp(heatmap, float3(1.0f, 0.0f, 0.0f), saturate((t - 0.5f) * 2.0f));
            }
            return float4(heatmap, 1.0f);
        }
        else if (lightingDebugMode < 16.5f) { // light_list_overflow (16)
            float3 overflowColor = float3(0.0f, 0.0f, 0.0f);
            if (count >= (uint)maxLightsPerTile) {
                overflowColor = float3(1.0f, 0.0f, 0.0f);
            } else {
                overflowColor = float3(0.0f, (float)count / 256.0f, 0.0f);
            }
            return float4(overflowColor, 1.0f);
        }
        else if (lightingDebugMode < 17.5f) { // point_diffuse (17)
            return float4(pointDiffuse, 1.0f);
        }
        else if (lightingDebugMode < 18.5f) { // point_specular (18)
            return float4(pointSpecular, 1.0f);
        }
        else if (lightingDebugMode < 19.5f) { // spot_diffuse (19)
            return float4(spotDiffuse, 1.0f);
        }
        else if (lightingDebugMode < 20.5f) { // spot_specular (20)
            return float4(spotSpecular, 1.0f);
        }
        else if (lightingDebugMode < 21.5f) { // bruteforce_vs_culled_difference (21)
            float3 diff = abs((localDiffuseLighting + localSpecularLighting) - (bruteForceDiff + bruteForceSpec));
            return float4(diff * 1000.0f, 1.0f); // Scale difference to make small errors stand out
        }
        else if (lightingDebugMode < 22.5f) { // IndirectOnly (22)
            return float4(indirectLighting, 1.0f);
        }
        else if (lightingDebugMode < 23.5f) { // FinalNoGI (23)
            return float4(finalColor, albedo.a);
        }
        else if (lightingDebugMode < 24.5f) { // FinalNoAO (24)
            return float4(finalColor, albedo.a);
        }
        else if (lightingDebugMode < 25.5f) { // DirectOnly (25)
            return float4(directLighting, albedo.a);
        }
    }

    if (fogEnabled > 0.5f) {
        float factor=ZeOutdoorFog(cameraPos.xyz,input.worldPos,fogDensity,fogStart,fogEnd,
                                 environmentParams.y,environmentParams.z);
        finalColor=lerp(finalColor,fogColor.rgb,factor);
    }

    // Only apply exposure, tone map, and output encoding inline when there is
    // NO post-process final-output pass.
    // When postProcessEnabled > 0, tonemap_colorgrade.hlsl handles this.
    // Exposure follows the same boundary: it is applied here only for the
    // inline fallback, otherwise in the final output pass.
    if (postProcessEnabled < 0.5f) {
        finalColor *= exposure;
        finalColor = ACESFilmic(finalColor);
        finalColor = pow(saturate(finalColor), 1.0f / max(gammaValue, 0.001f));
    }
    // When post-process IS enabled, output raw HDR linear.

    return float4(finalColor, albedo.a);
}

// Simplified Preetham analytical sky model
float3 preethamSky(float3 rayDir, float3 sunDir, float turbidity)
{
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float  cosTheta = saturate(dot(rayDir, up));
    float  cosGamma = saturate(dot(rayDir, sunDir));
    float  gamma    = acos(cosGamma);
    float  theta    = acos(cosTheta);

    // Perez functions for Y, x, y
    float t  = turbidity;
    float t2 = t * t;

    // Simplified coefficients for luminance (Y)
    float4 AY = float4(-0.0193f, -0.2592f, 0.2055f, -0.8167f);
    float4 BY = float4(-0.0665f,  0.0008f, 0.1168f, -0.3554f);
    float4 CY = float4(-0.0004f,  0.2125f,-0.2979f, -0.4275f);
    float4 DY = float4(-0.0641f, -0.8989f, 0.0959f, -0.2885f);
    float4 EY = float4(-0.0033f,  0.0452f,-0.1469f,  0.1534f);

    float chi   = (4.0f / 9.0f - t / 120.0f) * (3.14159265f - 2.0f * theta);
    float Yz    = (4.0453f * t - 4.9710f) * tan(chi) - 0.2155f * t + 2.4192f;

    float sunElevation = saturate(sunDir.y);

    // Blue sky base using zenith/horizon blend
    float3 zenithColor  = skyZenithColor.rgb;
    float3 horizonColor = skyHorizonColor.rgb;

    // Compute sun disc + atmospheric glow
    float sunDisk    = pow(saturate(cosGamma), 512.0f);
    float sunGlow    = pow(saturate(cosGamma), 8.0f) * 0.15f;
    float3 sunColor  = lightColor.rgb * (sunDisk * 0.8f + sunGlow);
    sunColor         = min(sunColor, 4.0f);

    // Y-up sky dome: bright horizon grading into the deeper zenith.
    float heightFactor = saturate(cosTheta);
    float3 skyColor = lerp(horizonColor * 1.10f, zenithColor * 0.95f,
                           sqrt(heightFactor));

    // Continue the horizon into the lower hemisphere so a finite editor ground
    // plane never exposes a black ring at its distant edges.
    if (rayDir.y < 0.0f) {
        float lowerHemisphere = saturate(-rayDir.y * 2.0f);
        skyColor = lerp(skyColor, horizonColor * 0.55f, lowerHemisphere);
    }

    // Apply fog overlay if fog is active
    if (fogEnabled > 0.5f && fogDensity > 0.0f) {
        float fogFactor = saturate(exp(-max(0.0f, rayDir.y) * 8.0f) * fogDensity * 50.0f);
        skyColor = lerp(skyColor, fogColor.rgb, fogFactor);
    }

    return (skyColor + sunColor) * skyIntensity;
}

struct SkyVSInput
{
    float3 pos      : POSITION;
};

struct SkyPSInput
{
    float4 pos : SV_POSITION;
    float3 localPos : TEXCOORD0;
};

SkyPSInput SkyVS(SkyVSInput input)
{
    SkyPSInput output;
    float4 wPos = mul(float4(input.pos, 1.0f), world);
    output.pos = ZeProjectCamera(wPos, viewProj, float2(lightDir.w, lightColor.w));
    output.pos.z = output.pos.w; // force depth to 1.0
    // Sphere vertices are already Y-up. Preserve that convention so the
    // zenith renders above the horizon and the ground void stays below it.
    output.localPos = input.pos;
    return output;
}

float4 SkyPS(SkyPSInput input) : SV_TARGET
{
    float3 rayDir = normalize(input.localPos);
    float3 sunDir = normalize(-lightDir.xyz);
    float3 color = preethamSky(rayDir, sunDir, 2.5f);
    if (hasBaseColorTexture > 0.5f) {
        // Longitude wraps at the panorama seam, latitude clamps at both poles.
        const float SKY_PI = 3.14159265359f;
        uint width, height;
        baseColorTex.GetDimensions(width, height);
        float2 uv = float2(frac(atan2(rayDir.z, rayDir.x) / (2.0f * SKY_PI) + 0.5f + metallic),
                           acos(clamp(rayDir.y, -1.0f, 1.0f)) / SKY_PI);
        uv.y = clamp(uv.y, 0.5f / height, 1.0f - 0.5f / height);
        float3 painted = baseColorTex.SampleLevel(baseSampler, uv, 0.0f).rgb;
        // PNGs are display-referred sRGB. Decode before HDR grading/bloom.
        float3 linearPaint = lerp(painted / 12.92f,
            pow((painted + 0.055f) / 1.055f, 2.4f), step(0.04045f, painted));
        if(hasNormalMap>.5f)linearPaint=painted; // Floating-point HDR panoramas are already linear.
        color = linearPaint * roughness * skyIntensity;
        // A narrow horizon veil joins the authored clouds to the terrain haze.
        if (fogEnabled > 0.5f) {
            float veil = exp(-abs(rayDir.y) * 24.0f) * emissiveStrength;
            color = lerp(color, fogColor.rgb * skyIntensity, veil);
        }
    }
    
    // Ocean continuation is background only, outside the configured playable
    // rectangle. It cannot replace shoreline meshes, lake/river surfaces, depth
    // ownership, collision or queries; normal geometry occludes it at depth 1.
    float seaLevel=terrainVariationParams.w;
    if(hasOcclusionTexture>.5 && cameraPos.y>seaLevel && rayDir.y<-.00001) {
        float rayDistance=(seaLevel-cameraPos.y)/rayDir.y;
        float3 waterPoint=cameraPos.xyz+rayDir*min(rayDistance,1e7);
        float footprint=length(fwidth(waterPoint.xz));
        bool outside=any(waterPoint.xz<terrainSurfaceParams.xy)||any(waterPoint.xz>terrainSurfaceParams.zw);
        if(outside) {
            float rippleFade=1-smoothstep(.7,3,footprint);
            float2 gradient=ZeWaterAdvectedGradient(waterPoint.xz,float2(.32,.16),lmPadding1);
            float3 N=normalize(float3(-gradient.x*rippleFade,1,-gradient.y*rippleFade));
            float3 V=-rayDir,R=reflect(rayDir,N);
            float fresnel=.0204+.9796*pow(1-saturate(abs(dot(N,V))),5);
            float skyBlend=smoothstep(0,.85,max(R.y,0));
            float3 reflection=lerp(skyHorizonColor.rgb,skyZenithColor.rgb,skyBlend)*skyIntensity;
            if(iblEnabled>.5) {
                float ec=cos(environmentParams.x),es=sin(environmentParams.x);
                float3 envR=float3(ec*R.x-es*R.z,R.y,es*R.x+ec*R.z);
                reflection=envMapTex.SampleLevel(clampSampler,envR,min(envMapMaxMip,1.4)).rgb*iblIntensity;
            }
            float3 waterColor=lerp(terrainVariationParams.xyz*max(skyIntensity,.25),reflection,fresnel);
            float3 H=normalize(V+sunDir);
            waterColor+=pow(saturate(dot(N,H)),lerp(240,600,rippleFade))*lerp(.025,.09,rippleFade)*lightColor.rgb;
            if(fogEnabled>.5) {
                float fog=ZeOutdoorFog(cameraPos.xyz,waterPoint,fogDensity,fogStart,fogEnd,environmentParams.y,environmentParams.z);
                waterColor=lerp(waterColor,fogColor.rgb,fog);
            }
            // Grazing reflections merge continuously with the actual sky at the
            // visual horizon, independently of the camera's finite far plane.
            color=lerp(color,waterColor,smoothstep(.003,.015,-rayDir.y));
        }
    }

    // Same guard as PSMain: only inline exposure/tone map when no post-process pass.
    if (postProcessEnabled < 0.5f) {
        color *= exposure;
        color = ACESFilmic(color);
        color = pow(saturate(color), 1.0f / max(gammaValue, 0.001f));
    }
    // When post-process IS enabled, output raw HDR linear sky color.

    return float4(color, 1.0f);
}
