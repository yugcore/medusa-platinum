#include "../common/ze_gbuffer.hlsli"
#include "../common/ze_outdoor_fog.hlsli"
#include "../common/ze_brdf.hlsli"
#include "../common/ze_indirect_lighting.hlsli"
#include "../common/ze_thin_surface.hlsli"
#include "../common/ze_light_types.hlsli"
#include "../common/ze_shadow_receiver.hlsli"

Texture2D<float4> gbufferATex : register(t0);
Texture2D<float4> gbufferBTex : register(t1);
Texture2D<uint4> gbufferCTex : register(t2);
Texture2D<float4> gbufferDTex : register(t3);
Texture2D<float> depthTex : register(t4);
StructuredBuffer<GpuLocalLight> localLights : register(t5);
Texture2D<float> directionalShadowMap : register(t6);
Texture2D<float> ambientVisibilityFiltered : register(t7);
Texture2D<float> ambientVisibilityRaw : register(t8);
SamplerState pointSampler : register(s0);
TextureCube environmentSpecular : register(t9);
TextureCube environmentDiffuse : register(t10);
Texture2D<float2> environmentBrdf : register(t11);
StructuredBuffer<uint4> lightTileRanges : register(t12);
StructuredBuffer<uint> lightTileIndices : register(t13);
Texture2D<float> localShadowAtlas : register(t14);
struct LocalShadowLight {
    row_major float4x4 matrices[6];
    float4 rectangles[6];
    float4 settings;
};
StructuredBuffer<LocalShadowLight> localShadowLights : register(t15);
Texture2D<float4> rayVisibility : register(t16);
SamplerState environmentSampler : register(s1);

static const float SHADOW_MAX_RECEIVER_BIAS = 0.005f;
static const float SHADOW_MAX_NORMAL_BIAS_TEXELS = 2.0f;

cbuffer DeferredConstants : register(b0)
{
    row_major float4x4 inverseViewProjection;
    float4 cameraPosition;
    float4 directionalDirection;
    float4 directionalColorIntensity;
    float4 ambientColorIntensity;
    float2 viewportSize;
    uint localLightCount;
    uint debugMode;
    row_major float4x4 worldToShadowClip;
    float2 shadowTexelSize;
    float shadowConstantBias;
    float shadowSlopeBias;
    float shadowNormalBias;
    float shadowStrength;
    float shadowPcfRadius;
    float shadowSoftness;
    float shadowTexelWorldSize;
    float shadowEnabled;
    float shadowDebugMode;
    float shadowStabilizationEnabled;
    float aoEnabled;
    uint finalImageDebugMode;
    float2 deferredPadding0;

    float4 skyHorizonColor;
    float4 skyZenithColor;
    float skyIntensity;
    float minAmbientFloor;
    float2 deferredPadding1;
    row_major float4x4 shadowViewProjection;
    row_major float4x4 cascadeViewProjection[4];
    float4 cascadeScaleBias[4];
    float4 cascadeSplits;
    float cascadeCount;
    float3 cascadePadding;
    float4 environmentParams;
    float4 fogColorDensity;
    float4 fogDistancesHeight;
    float4 fogEnabledPadding;
    uint4 lightTiles;
    float4 localShadowSettings;
    float4 rayVisibilitySettings;
};

float LocalShadowVisibility(uint index,float3 world,float3 normal,float3 lightPosition) {
    if(localShadowSettings.w<=0) return 1;
    LocalShadowLight light=localShadowLights[index];
    if(light.settings.x<.5) return 1;
    float3 d=world-lightPosition, a=abs(d);
    uint face=0;
    if(light.settings.x>1.5) face=a.x>=a.y&&a.x>=a.z?(d.x>=0?0:1):(a.y>=a.z?(d.y>=0?2:3):(d.z>=0?4:5));
    float4 clip=mul(float4(world+normal*localShadowSettings.z,1),light.matrices[face]);
    if(clip.w<=0) return 1;
    float3 ndc=clip.xyz/clip.w;
    float2 uv=ndc.xy*float2(.5,-.5)+.5;
    if(any(uv<0)||any(uv>1)||ndc.z<0||ndc.z>1) return 1;
    float4 rect=light.rectangles[face];uv=uv*rect.xy+rect.zw;
    float visibility=0;
    [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
        float2 tap=clamp(uv+float2(x,y)*localShadowSettings.w,rect.zw+localShadowSettings.w*.5,rect.zw+rect.xy-localShadowSettings.w*.5);
        visibility += ndc.z-localShadowSettings.y <= localShadowAtlas.SampleLevel(pointSampler,tap,0) ? 1 : 0;
    }
    return lerp(1,visibility/9,saturate(localShadowSettings.x));
}

// Simplified Preetham analytical sky model
float3 preethamSky(float3 rayDir, float3 sunDir, float turbidity)
{
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float  cosTheta = saturate(dot(rayDir, up));
    float  cosGamma = saturate(dot(rayDir, sunDir));

    // Blue sky base using zenith/horizon blend
    float3 zenithColor  = skyZenithColor.rgb;
    float3 horizonColor = skyHorizonColor.rgb;

    // Compute sun disc + atmospheric glow
    float sunDisk    = pow(saturate(cosGamma), 512.0f);
    float sunGlow    = pow(saturate(cosGamma), 8.0f) * 0.3f;
    float3 sunColor  = (directionalColorIntensity.rgb * directionalColorIntensity.a) * (sunDisk * 5.0f + sunGlow);

    // Sky gradient
    float heightFactor = pow(saturate(cosTheta), 0.5f);
    float3 skyColor    = lerp(horizonColor, zenithColor, heightFactor);

    // Horizon haze
    float haze = pow(1.0f - saturate(cosTheta), 4.0f) * 0.6f;
    skyColor   = lerp(skyColor, horizonColor * 1.2f, haze);

    return (skyColor + sunColor) * skyIntensity;
}

struct FullscreenVertex
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    FullscreenVertex output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(
        output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f),
        0.0f,
        1.0f);
    return output;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float4 clip = float4(
        uv.x * 2.0f - 1.0f,
        1.0f - uv.y * 2.0f,
        depth,
        1.0f);
    clip.xy -= deferredPadding0;
    float4 worldPosition = mul(clip, inverseViewProjection);
    float3 position=worldPosition.xyz / max(abs(worldPosition.w), 0.00001f);
    // Zero preserves the legacy absolute inverse payload/constant-buffer ABI.
    return position + (deferredPadding1.x>.5f ? cameraPosition.xyz : 0.0f);
}

float3 Heatmap(float value)
{
    float3 cold = float3(0.0f, 0.1f, 1.0f);
    float3 warm = float3(1.0f, 0.8f, 0.0f);
    return lerp(cold, warm, saturate(value));
}

float3 BiasHeatmap(float value)
{
    float t = saturate(value);
    float3 low = float3(0.0f, 0.2f, 1.0f);
    float3 medium = float3(1.0f, 1.0f, 0.0f);
    float3 high = float3(1.0f, 0.0f, 0.0f);
    return t < 0.5f
        ? lerp(low, medium, t * 2.0f)
        : lerp(medium, high, (t - 0.5f) * 2.0f);
}

float BilinearShadow(float2 uv, float2 receiverUv, float depth,
    float2 gradient, float bias, float2 minUv, float2 maxUv) {
    float2 pixel = uv / shadowTexelSize - .5f;
    float2 origin = floor(pixel), blend = frac(pixel);
    float visibility = 0;
    [unroll] for (int y=0; y<2; ++y) [unroll] for (int x=0; x<2; ++x) {
        float2 tapUv = clamp((origin+float2(x,y)+.5f)*shadowTexelSize, minUv, maxUv);
        float sampleDepth = directionalShadowMap.SampleLevel(pointSampler,tapUv,0);
        float receiver = ZeShadowReceiverTapDepth(depth,gradient,tapUv-receiverUv);
        float weight = (x ? blend.x : 1-blend.x)*(y ? blend.y : 1-blend.y);
        visibility += weight*(receiver-bias <= sampleDepth ? 1.f : 0.f);
    }
    return visibility;
}

float EvaluateDirectionalShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDirection,
    out float2 shadowUv,
    out float receiverDepth,
    out float closestDepth,
    out float uvInBounds,
    out float appliedBias,
    out float filteredVisibility)
{
    shadowUv = 0.0f;
    receiverDepth = 0.0f;
    closestDepth = 1.0f;
    uvInBounds = 0.0f;
    appliedBias = 0.0f;
    filteredVisibility = 1.0f;
    if (shadowEnabled < 0.5f)
    {
        return 1.0f;
    }

    if (!all(isfinite(worldPosition)))
    {
        return 1.0f;
    }

    uint cascade = 0;
    row_major float4x4 projection = worldToShadowClip;
    float4 scaleBias = float4(1,1,0,0);
    if (cascadeCount > 0) {
        float viewDistance = abs(shadowViewProjection[3][3]-1) < .001f
            ? distance(worldPosition,cameraPosition.xyz)
            : mul(float4(worldPosition,1),shadowViewProjection).w;
        [unroll] for (uint i=0; i<3; ++i)
            if (i+1 < (uint)cascadeCount && viewDistance > cascadeSplits[i]) cascade=i+1;
        projection = cascadeViewProjection[cascade];
        scaleBias = cascadeScaleBias[cascade];
    }
    float ndotl = saturate(dot(normal, lightDirection));
    float4 rawClip = mul(float4(worldPosition, 1.0f), projection);
    float3 rawProjected = rawClip.xyz / rawClip.w;
    float2 rawUv = (rawProjected.xy*float2(.5f,-.5f)+.5f)*scaleBias.xy+scaleBias.zw;
    float2 receiverGradient = ZeShadowPlaneDepthGradient(normal,projection,scaleBias.xy);
    float texelWorldSize = max(2*shadowTexelSize.x/scaleBias.x/
        max(length(float3(projection[0][0],projection[1][0],projection[2][0])),1e-6f),
        2*shadowTexelSize.y/scaleBias.y/
        max(length(float3(projection[0][1],projection[1][1],projection[2][1])),1e-6f));
    float slopeTerm = saturate(1.0f - ndotl);
    float normalOffsetWorld =
        clamp(
            shadowNormalBias,
            0.0f,
            SHADOW_MAX_NORMAL_BIAS_TEXELS) *
        texelWorldSize *
        slopeTerm;
    float3 biasedWorldPosition =
        worldPosition + normal * normalOffsetWorld;

    float4 shadowClip = mul(
        float4(biasedWorldPosition, 1.0f),
        projection);
    if (abs(shadowClip.w) < 0.00001f || !isfinite(shadowClip.w))
    {
        return 1.0f;
    }

    float3 shadowNdc = shadowClip.xyz / shadowClip.w;
    shadowUv = float2(
        shadowNdc.x * 0.5f + 0.5f,
        0.5f - shadowNdc.y * 0.5f);
    receiverDepth = shadowNdc.z;
    float2 localUv = shadowUv;
    shadowUv = localUv*scaleBias.xy+scaleBias.zw;
    float2 minUv=scaleBias.zw+shadowTexelSize*2;
    float2 maxUv=scaleBias.zw+scaleBias.xy-shadowTexelSize*2;

    if (!all(isfinite(shadowUv)) || !isfinite(receiverDepth))
    {
        return 1.0f;
    }

    uvInBounds =
        localUv.x >= 0.0f && localUv.x <= 1.0f &&
        localUv.y >= 0.0f && localUv.y <= 1.0f &&
        receiverDepth >= 0.0f && receiverDepth <= 1.0f
        ? 1.0f
        : 0.0f;
    if (uvInBounds < 0.5f)
    {
        return 1.0f;
    }

    appliedBias = clamp(
        max(shadowConstantBias, 0.0f) +
        max(shadowSlopeBias, 0.0f) * slopeTerm,
        0.0f,
        SHADOW_MAX_RECEIVER_BIAS);
    closestDepth = directionalShadowMap.SampleLevel(
        pointSampler,
        clamp(
            shadowUv,
            minUv, maxUv),
        0);

    if (!isfinite(closestDepth))
    {
        closestDepth = 1.0f;
    }

    int radius = shadowPcfRadius < 0.5f
        ? 0
        : (shadowPcfRadius < 1.5f ? 1 : 2);
    float sampleRadius = round(
        clamp(shadowSoftness, 0.25f, 4.0f) * 4.0f) * 0.25f;
    float visibility = 0.0f;
    float totalWeight = 0.0f;
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            if (abs(x) > radius || abs(y) > radius)
            {
                continue;
            }
            float2 sampleUv = clamp(
                shadowUv +
                    float2(x, y) * shadowTexelSize * sampleRadius,
                minUv, maxUv);
            visibility += BilinearShadow(sampleUv,shadowUv,receiverDepth,
                receiverGradient,appliedBias,minUv,maxUv);
            totalWeight += 1.0f;
        }
    }
    filteredVisibility = visibility / max(totalWeight, 1.0f);
    if (!isfinite(filteredVisibility))
    {
        filteredVisibility = 1.0f;
    }
    return lerp(
        1.0f,
        filteredVisibility,
        saturate(shadowStrength));
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    if (shadowDebugMode > 0.5f && shadowDebugMode < 1.5f)
    {
        float shadowDepth = directionalShadowMap.SampleLevel(
            pointSampler, input.uv, 0);
        float visibleDepth = shadowDepth < 0.999999f
            ? saturate((shadowDepth - 0.2f) / 0.6f)
            : 0.0f;
        return float4(visibleDepth.xxx, 1.0f);
    }

    int2 pixel = int2(input.position.xy);
    float depth = depthTex.Load(int3(pixel, 0));
    if (finalImageDebugMode == 7u)
        return ambientVisibilityRaw.Load(int3(pixel, 0)).xxxx;
    if (finalImageDebugMode == 8u)
        return ambientVisibilityFiltered.Load(int3(pixel, 0)).xxxx;
    if (depth >= 1.0f)
    {
        discard;
    }

    ZeGBufferData surface = ZeDecodeGBufferUInt(
        gbufferATex.Load(int3(pixel, 0)),
        gbufferBTex.Load(int3(pixel, 0)),
        gbufferCTex.Load(int3(pixel, 0)),
        gbufferDTex.Load(int3(pixel, 0)));

    float3 worldPosition = ReconstructWorldPosition(input.uv, depth);
    float3 N = normalize(surface.worldNormal);
    float2 thin=surface.shadingModel==2u?ZeThinUnpack(surface.flags):float2(0,0);
    float3 V = normalize(cameraPosition.xyz - worldPosition);
    float3 directionalDiffuse = 0.0f;
    float3 directionalSpecular = 0.0f;
    float directionalNdotL = 0.0f;
    float2 shadowUv = 0.0f;
    float shadowReceiverDepth = 0.0f;
    float shadowClosestDepth = 1.0f;
    float shadowUvInBounds = 0.0f;
    float shadowBias = 0.0f;
    float shadowVisibility = 1.0f;
    float filteredShadowVisibility = 1.0f;

    float3 L = normalize(-directionalDirection.xyz);
    directionalNdotL = saturate(dot(N, L));
    if (directionalColorIntensity.a > 0.0f && (directionalNdotL > 0.0f || any(thin>0)))
    {
        BrdfInput brdfInput;
        brdfInput.N = N;
        brdfInput.V = V;
        brdfInput.L = L;
        brdfInput.baseColor = surface.baseColor;
        brdfInput.metallic = surface.metallic;
        brdfInput.perceptualRoughness = surface.perceptualRoughness;
        BrdfResult result = EvaluateThinSurfaceBrdf(brdfInput,thin);
        float3 radiance =
            directionalColorIntensity.rgb * directionalColorIntensity.a;
        directionalDiffuse = result.diffuse * radiance;
        directionalSpecular = result.specular * radiance;
    }
    shadowVisibility = EvaluateDirectionalShadow(
        worldPosition,
        N,
        L,
        shadowUv,
        shadowReceiverDepth,
        shadowClosestDepth,
        shadowUvInBounds,
        shadowBias,
        filteredShadowVisibility);
    if(rayVisibilitySettings.x>.5)
        shadowVisibility=lerp(1,saturate(rayVisibility.Load(int3(pixel,0)).x),saturate(shadowStrength));
    directionalDiffuse *= shadowVisibility;
    directionalSpecular *= shadowVisibility;

    float3 pointDiffuse = 0.0f;
    float3 pointSpecular = 0.0f;
    uint influencingLights = 0u;
    uint4 tile = lightTiles.x != 0 ? lightTileRanges[
        min(pixel.y / lightTiles.w, lightTiles.z - 1) * lightTiles.y +
        min(pixel.x / lightTiles.w, lightTiles.y - 1)] : uint4(0, localLightCount, 1, 0);
    bool allLights = lightTiles.x == 0 || tile.z != 0;
    uint count = allLights ? localLightCount : tile.y;
    for (uint listIndex = 0u; listIndex < count; ++listIndex)
    {
        uint lightIndex = allLights ? listIndex : lightTileIndices[tile.x + listIndex];
        GpuLocalLight light = localLights[lightIndex];
        float3 toLight = light.positionRange.xyz - worldPosition;
        float distanceToLight = length(toLight);
        float range = max(light.positionRange.w, 0.001f);
        if (distanceToLight >= range)
        {
            continue;
        }

        float3 localL = toLight / max(distanceToLight, 0.0001f);
        float cutoff = saturate(1.0f - pow(distanceToLight / range, 4.0f));
        float attenuation =
            cutoff * cutoff / max(distanceToLight * distanceToLight, 0.01f);

        if (light.directionType.w > 1.5f)
        {
            float cone = dot(-localL, normalize(light.directionType.xyz));
            float innerCone = light.coneRectFlags.x;
            float outerCone = light.coneRectFlags.y;
            float coneAttenuation = saturate(
                (cone - outerCone) / max(innerCone - outerCone, 0.0001f));
            attenuation *= coneAttenuation * coneAttenuation;
        }
        if (attenuation <= 0.0f || (dot(N, localL) <= 0.0f && !any(thin>0)))
        {
            continue;
        }

        BrdfInput brdfInput;
        brdfInput.N = N;
        brdfInput.V = V;
        brdfInput.L = localL;
        brdfInput.baseColor = surface.baseColor;
        brdfInput.metallic = surface.metallic;
        brdfInput.perceptualRoughness = surface.perceptualRoughness;
        BrdfResult result = EvaluateThinSurfaceBrdf(brdfInput,thin);
        float3 radiance =
            light.colorIntensity.rgb * light.colorIntensity.a * attenuation;
        radiance *= LocalShadowVisibility(lightIndex,worldPosition,N,light.positionRange.xyz);
        pointDiffuse += result.diffuse * radiance;
        pointSpecular += result.specular * radiance;
        ++influencingLights;
    }

    float screenAO = aoEnabled > 0.5f
        ? ambientVisibilityFiltered.Load(int3(pixel, 0))
        : 1.0f;
    float combinedAO = saturate(surface.materialAO * screenAO);

    // Unified ambient model matching the forward PBR sky-gradient fallback
    float3 skyColor = lerp(skyHorizonColor.rgb, skyZenithColor.rgb,
                           saturate(N.y * 0.5f + 0.5f)) * skyIntensity;
    float3 hemiAmbient = skyColor * (1.0f / 3.14159265f);

    float NoV = saturate(dot(N, V));
    float3 F0 = lerp(0.04f.xxx, surface.baseColor, surface.metallic);
    float2 skyDfg=environmentBrdf.SampleLevel(environmentSampler,
        float2(NoV,surface.perceptualRoughness),0).rg;
    float skyF90 = saturate(dot(F0, 0.33333f) * 50.0f * (1.0f - surface.perceptualRoughness) + surface.metallic);
    float3 ambientFresnel=saturate(F0*skyDfg.x+skyF90*skyDfg.y);
    float3 indirectDiffuse = ZeIndirectDiffuseWeight(surface.baseColor,
        surface.metallic, ambientFresnel) * hemiAmbient;
    float3 indirectSpecular = ambientFresnel * hemiAmbient;
    if(environmentParams.x>.5f){
        float ec=cos(environmentParams.w),es=sin(environmentParams.w);
        float3 envN=float3(ec*N.x-es*N.z,N.y,es*N.x+ec*N.z);
        float3 R=reflect(-V,N);
        R=float3(ec*R.x-es*R.z,R.y,es*R.x+ec*R.z);
        float3 irradiance=environmentDiffuse.SampleLevel(environmentSampler,envN,0).rgb;
        if(thin.y>0)irradiance=lerp(irradiance,environmentDiffuse.SampleLevel(environmentSampler,-envN,0).rgb,thin.y*.5);

        float2 brdf=environmentBrdf.SampleLevel(environmentSampler,
            float2(NoV,surface.perceptualRoughness),0);
        float f90 = saturate(dot(F0, 0.33333f) * 50.0f * (1.0f - surface.perceptualRoughness) + surface.metallic);
        float3 response = F0 * brdf.x + f90 * brdf.y;
        indirectDiffuse = irradiance * ZeIndirectDiffuseWeight(surface.baseColor,
            surface.metallic, response) * environmentParams.y;
        float3 iblSpecSample = environmentSpecular.SampleLevel(environmentSampler,R,
            surface.perceptualRoughness*environmentParams.z).rgb;
        float maxIblSpec = lerp(4.0f, 100.0f, surface.metallic);
        iblSpecSample = min(iblSpecSample, maxIblSpec.xxx);
        indirectSpecular = iblSpecSample * response * environmentParams.y;
    }

    // RuntimeScene's ambient setting is independent of the selected sky/IBL.
    indirectDiffuse += max(ambientColorIntensity.rgb, 0.0f) *
        max(ambientColorIntensity.a, 0.0f) *
        ZeIndirectDiffuseWeight(surface.baseColor, surface.metallic, ambientFresnel) /
        3.14159265f;
    float3 indirectBeforeAo = indirectDiffuse + indirectSpecular;
    float3 indirectAfterAo =
        indirectDiffuse * combinedAO +
        indirectSpecular * combinedAO;
    
    // Apply minAmbientFloor clamp to guarantee vertical/occluded surfaces don't drop to black
    indirectAfterAo = max(indirectAfterAo, surface.baseColor * minAmbientFloor);
    float3 emissiveHdr =
        all(isfinite(surface.emissive))
        ? max(surface.emissive, 0.0f)
        : 0.0f;
    // Emissive is self-illumination. Add it after AO, shadows, and all direct
    // light evaluation so those visibility terms cannot attenuate it.
    float3 hdrColor =
        indirectAfterAo + directionalDiffuse + directionalSpecular +
        pointDiffuse + pointSpecular + emissiveHdr;

    if (finalImageDebugMode == 9u)
        return float4(indirectBeforeAo, 1.0f);
    if (finalImageDebugMode == 10u)
        return float4(indirectAfterAo, 1.0f);
    if (finalImageDebugMode == 11u)
        return float4(
            directionalDiffuse + directionalSpecular +
            pointDiffuse + pointSpecular,
            1.0f);
    if (finalImageDebugMode == 12u)
        return float4(hdrColor, 1.0f);

    float3 directLighting =
        directionalDiffuse + directionalSpecular +
        pointDiffuse + pointSpecular;

    if (debugMode == 1u) return float4(surface.baseColor, 1.0f);
    if (debugMode == 2u) return float4(N * 0.5f + 0.5f, 1.0f);
    if (debugMode == 3u) return surface.perceptualRoughness.xxxx;
    if (debugMode == 4u) return surface.metallic.xxxx;
    if (debugMode == 5u) return combinedAO.xxxx;
    if (debugMode == 6u) return float4(emissiveHdr, 1.0f);
    if (debugMode == 7u) return depth.xxxx;
    if (debugMode == 28u) return float4(localLights[0].directionType.www / 2.0f, 1.0f);
    if (debugMode == 29u) return float4(float3(localLightCount, localLightCount, localLightCount) / 8.0f, 1.0f);
    if (debugMode == 9u) return float4(directionalDiffuse + pointDiffuse, 1.0f);
    if (debugMode == 10u) return float4(directionalSpecular + pointSpecular, 1.0f);
    if (debugMode == 11u) return directionalNdotL.xxxx;
    if (debugMode == 13u) return float4(Heatmap(float(influencingLights) / 8.0f), 1.0f);
    if (debugMode == 15u) return float4(Heatmap(float(influencingLights) / 8.0f), 1.0f);
    if (debugMode == 17u) return float4(pointDiffuse, 1.0f);
    if (debugMode == 18u) return float4(pointSpecular, 1.0f);
    // Lift the isolated indirect diagnostic slightly so low-key scenes
    // remain readable after the production tone-map.
    if (debugMode == 22u) return float4(indirectAfterAo * 1.25f, 1.0f);
    if (debugMode == 23u) return float4(hdrColor, 1.0f);
    if (debugMode == 24u)
        return float4(indirectBeforeAo + directLighting + emissiveHdr, 1.0f);
    if (debugMode == 25u) return float4(directLighting, 1.0f);
    if (debugMode == 26u) return float4(directionalDiffuse + directionalSpecular, 1.0f);
    if (shadowDebugMode > 2.5f && shadowDebugMode < 3.5f)
        return shadowVisibility.xxxx;
    if (shadowDebugMode > 4.5f && shadowDebugMode < 5.5f)
        return float4(
            BiasHeatmap(
                shadowBias /
                    max(
                        shadowConstantBias + shadowSlopeBias,
                        0.000001f)),
            1.0f);
    if (shadowDebugMode > 5.5f && shadowDebugMode < 6.5f)
        return filteredShadowVisibility.xxxx;
    if (shadowDebugMode > 6.5f && shadowDebugMode < 7.5f)
    {
        if (shadowUvInBounds < 0.5f)
            return float4(1.0f, 0.0f, 0.1f, 1.0f);

        float edgeDistance = min(
            min(shadowUv.x, 1.0f - shadowUv.x),
            min(shadowUv.y, 1.0f - shadowUv.y));
        if (edgeDistance < max(shadowTexelSize.x, shadowTexelSize.y) * 2.0f)
            return float4(0.8f, 0.35f, 0.0f, 1.0f);

        return float4(
            0.0f,
            0.25f + shadowUv.y * 0.25f,
            0.05f + shadowUv.x * 0.10f,
            1.0f);
    }
    if(fogEnabledPadding.x>0.5) {
        float factor=ZeOutdoorFog(cameraPosition.xyz,worldPosition,fogColorDensity.w,
            fogDistancesHeight.x,fogDistancesHeight.y,fogDistancesHeight.z,fogDistancesHeight.w);
        hdrColor=lerp(hdrColor,fogColorDensity.rgb,factor);
    }
    return float4(hdrColor, 1.0f);
}
