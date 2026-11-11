#include "../common/ze_light_types.hlsli"

cbuffer Phase7Constants : register(b0)
{
    float4 cameraPositionTime;
    float4 cameraForwardFar;
    float4 cameraRightNear;
    float4 cameraUpAspect;
    float4 sunDirectionIntensity;
    float4 atmosphereParams0;
    float4 atmosphereParams1;
    float4 fogParams;
    float4 fogColor;
    float4 cloudParams0;
    float4 cloudParams1;
    float4 waterShallow;
    float4 waterDeep;
    float4 waterParams;
    float4 lensParams0;
    float4 lensParams1;
    float4 lensParams2;
    uint4 outputSizeFrame;
    uint4 volumeSize;
    float4 volumetricParams;
    uint4 localFogMeta;
    float4 localFogPositionDensity[8];
    float4 localFogExtentFalloff[8];
    float4 localFogColorAnisotropy[8];
    float4 sunRadiance;
    row_major float4x4 shadowViewProjection[4];
    float4 shadowScaleBias[4];
    float4 shadowSplits;
    uint4 shadowInfo;
    float4 compositeParams;
    float4 oceanHorizonBounds;
    float4 oceanHorizonParams;
    float4 weatherFog;
    float4 weatherColor;
    float4 weatherMotion;
};

// Bindless resource arrays
Texture2D<float4> BindlessTextures2D[] : register(t0, space0);
Texture3D<float4> BindlessTextures3D[] : register(t0, space1);
RWTexture2D<float4> BindlessUAVs2D[] : register(u0, space0);
RWTexture3D<float4> BindlessUAVs3D[] : register(u0, space1);
Texture2D<float> BindlessDepthTextures[] : register(t0, space2);
StructuredBuffer<GpuLocalLight> BindlessBuffers[] : register(t0, space3);
RWTexture2D<float> BindlessUAVsFloat[] : register(u0, space2);

SamplerState samplerLinearClamp : register(s0);
SamplerState samplerLinearWrap : register(s1);

static const float PI = 3.14159265359f;

// Quadratic depth allocates slices to near-field canopy shadows rather than
// spending the first 54 metres in one voxel at a 2.6 km medium-quality range.
float froxelDistance(float coordinate) {
    return coordinate * coordinate * max(volumetricParams.z, 1.0f);
}
float froxelCoordinate(float distanceMeters) {
    return sqrt(saturate(distanceMeters / max(volumetricParams.z, 1.0f)));
}

// Sample the same cascaded atlas as surface lighting. Uncovered voxels stay lit;
// atlas taps are clamped inside their page to avoid neighbouring-cascade leaks.
float sunVisibility(float3 position, float distanceMeters)
{
    if (shadowInfo.x == 0 || shadowInfo.y == 0xffffffffu) return 1.0f;
    uint page = 0;
    while (page + 1 < shadowInfo.x && distanceMeters > shadowSplits[page]) ++page;
    float4 clip = mul(float4(position, 1), shadowViewProjection[page]);
    if (abs(clip.w) < 0.00001f) return 1.0f;
    float3 p = clip.xyz / clip.w;
    float2 uv = float2(p.x * .5f + .5f, .5f - p.y * .5f);
    if (any(uv < 0) || any(uv > 1) || p.z < 0 || p.z > 1) return 1.0f;
    float4 box = shadowScaleBias[page];
    uv = uv * box.xy + box.zw;
    float2 texel = 1.0f / float2(shadowInfo.zw);
    float2 lo = box.zw + texel * .5f;
    float2 hi = box.zw + box.xy - texel * .5f;
    float result = 0;
    [unroll] for (int y=0; y<2; ++y)
        [unroll] for (int x=0; x<2; ++x) {
            float2 tap = clamp(uv + (float2(x,y)-.5f)*texel, lo, hi);
            float depth = BindlessDepthTextures[shadowInfo.y].Load(int3(int2(tap*float2(shadowInfo.zw)),0));
            result += p.z - compositeParams.w <= depth ? .25f : 0;
        }
    return result;
}

// Bindless resource helpers
Texture2D<float4> getSceneTexture() {
    return BindlessTextures2D[asuint(waterShallow.x)];
}
Texture2D<float4> getAuxiliaryTexture() {
    return BindlessTextures2D[asuint(waterShallow.y)];
}
Texture2D<float4> getEffectTexture() {
    return BindlessTextures2D[asuint(waterShallow.z)];
}
Texture2D<float4> getSkyTexture() {
    return BindlessTextures2D[asuint(waterShallow.w)];
}
Texture3D<float4> getAerialTexture() {
    return BindlessTextures3D[asuint(waterDeep.x)];
}
Texture3D<float4> getFroxelTexture() {
    return BindlessTextures3D[asuint(waterDeep.y)];
}
RWTexture2D<float4> getOutputTexture() {
    return BindlessUAVs2D[asuint(waterDeep.z)];
}
RWTexture3D<float4> getOutputVolume() {
    return BindlessUAVs3D[asuint(waterDeep.w)];
}
RWTexture2D<float> getExposureTexture() {
    return BindlessUAVsFloat[asuint(waterParams.x)];
}

// Special case for WaterCompositeCS
Texture2D<float4> getWaterSceneTexture() {
    return BindlessTextures2D[asuint(cloudParams0.x)];
}
Texture2D<float4> getWaterAuxiliaryTexture() {
    return BindlessTextures2D[asuint(cloudParams0.y)];
}
Texture2D<float4> getWaterSkyTexture() {
    return BindlessTextures2D[asuint(cloudParams0.z)];
}
RWTexture2D<float4> getWaterOutputTexture() {
    return BindlessUAVs2D[asuint(cloudParams0.w)];
}

float hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float valueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = hash12(cell);
    float b = hash12(cell + float2(1.0f, 0.0f));
    float c = hash12(cell + float2(0.0f, 1.0f));
    float d = hash12(cell + 1.0f);
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm(float2 p)
{
    float sum = 0.0f;
    float amplitude = 0.5f;
    [unroll]
    for (uint octave = 0; octave < 5; ++octave)
    {
        sum += valueNoise(p) * amplitude;
        p = p * 2.03f + 17.17f;
        amplitude *= 0.5f;
    }
    return sum;
}

float3 viewRay(float2 uv)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    const float tanHalfFov = cameraRightNear.w;
    return normalize(
        cameraForwardFar.xyz +
        cameraRightNear.xyz * ndc.x * tanHalfFov * cameraUpAspect.w +
        cameraUpAspect.xyz * ndc.y * tanHalfFov);
}

// A background ocean is drawn at sky depth so normal geometry occludes it.
// Atmosphere must nevertheless stop at its actual sea intersection, rather
// than integrating to the sky's far plane and exposing the world boundary.
float oceanBackgroundDistance(float2 uv)
{
    float3 ray=viewRay(uv);
    if(oceanHorizonParams.x<.5f || cameraPositionTime.y<=oceanHorizonParams.y || ray.y>=-.00001f)
        return -1;
    float distanceMeters=(oceanHorizonParams.y-cameraPositionTime.y)/ray.y;
    float2 intersectionXZ=cameraPositionTime.xz+ray.xz*distanceMeters;
    return any(intersectionXZ<oceanHorizonBounds.xy)||any(intersectionXZ>oceanHorizonBounds.zw) ? distanceMeters : -1;
}

float3 physicalSky(float3 direction)
{
    const float3 sunDirection = normalize(sunDirectionIntensity.xyz);
    const float up = saturate(direction.y * 0.5f + 0.5f);
    const float horizon = 1.0f - abs(direction.y);
    const float sun = pow(saturate(dot(direction, sunDirection)), 256.0f);
    const float rayleigh = atmosphereParams0.z;
    const float mie = atmosphereParams0.w;
    const float ozone = atmosphereParams1.x;
    float3 color =
        float3(0.18f, 0.36f, 0.86f) * (0.25f + up * 0.85f) * rayleigh +
        float3(0.95f, 0.68f, 0.38f) * (horizon * 0.22f + sun * 8.0f) * mie +
        float3(0.12f, 0.18f, 0.42f) * up * ozone;
    return color * atmosphereParams1.z * sunDirectionIntensity.w;
}

[numthreads(8, 8, 1)]
void TransmittanceCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float height = uv.y * atmosphereParams0.y;
    float density = exp(-height / max(1.0f, atmosphereParams0.y * 0.12f));
    float opticalDepth = density * (0.6f * atmosphereParams0.z +
                                    0.35f * atmosphereParams0.w +
                                    0.05f * atmosphereParams1.x);
    float transmittance = exp(-opticalDepth * (0.2f + uv.x * 3.0f));
    getOutputTexture()[id.xy] = float4(
        transmittance,
        exp(-opticalDepth * 0.72f),
        exp(-opticalDepth * 0.48f),
        1.0f);
}

[numthreads(8, 8, 1)]
void MultiScatteringCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float altitude = uv.y;
    float horizon = 1.0f - abs(uv.x * 2.0f - 1.0f);
    float3 scattering =
        float3(0.16f, 0.30f, 0.62f) * atmosphereParams0.z * (0.3f + altitude) +
        float3(0.62f, 0.34f, 0.14f) * atmosphereParams0.w * horizon * 0.25f;
    getOutputTexture()[id.xy] = float4(scattering * atmosphereParams1.z, 1.0f);
}

[numthreads(8, 8, 1)]
void SkyViewCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float azimuth = (uv.x * 2.0f - 1.0f) * PI;
    float elevation = (uv.y * 2.0f - 1.0f) * PI * 0.5f;
    float3 direction = float3(
        cos(elevation) * sin(azimuth),
        sin(elevation),
        cos(elevation) * cos(azimuth));
    getOutputTexture()[id.xy] = float4(physicalSky(direction), 1.0f);
}

[numthreads(4, 4, 4)]
void AerialLutCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id >= volumeSize.xyz))
        return;
    float3 uvw = (float3(id) + 0.5f) / float3(volumeSize.xyz);
    float distanceMeters = uvw.z * atmosphereParams1.y;
    float density = exp(-max(0.0f, cameraPositionTime.y) * 0.001f /
                        max(1.0f, atmosphereParams0.y * 0.12f));
    float opticalDepth =
        distanceMeters / max(1.0f, atmosphereParams1.y) *
        density *
        (0.6f * atmosphereParams0.z +
         0.35f * atmosphereParams0.w +
         0.05f * atmosphereParams1.x);
    float transmittance = exp(-opticalDepth);
    float3 direction = viewRay(uvw.xy);
    float3 scattering = physicalSky(direction) * (1.0f - transmittance);
    getOutputVolume()[id] = float4(scattering, transmittance);
}

[numthreads(8, 8, 1)]
void FroxelCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id >= volumeSize.xyz))
        return;
    float3 uvw = (float3(id) + 0.5f) / float3(volumeSize.xyz);
    float distanceMeters = froxelDistance(uvw.z);
    float3 direction = viewRay(uvw.xy);
    float3 worldPos = cameraPositionTime.xyz + direction * distanceMeters;
    
    float sampleHeight = worldPos.y;
    float heightDensity = exp(-max(0.0f, sampleHeight) * max(0.0f, volumetricParams.w));
    float density = max(0.0f, volumetricParams.x) * heightDensity;
    float3 mediumColor = fogColor.rgb;
    float anisotropy = volumetricParams.y;
    for (uint volumeIndex = 0; volumeIndex < min(localFogMeta.x, 8u); ++volumeIndex)
    {
        float3 center = localFogPositionDensity[volumeIndex].xyz;
        float3 extents = max(localFogExtentFalloff[volumeIndex].xyz, 0.01f);
        float edge = max(localFogExtentFalloff[volumeIndex].w, 0.001f);
        float3 local = abs(worldPos - center) / extents;
        float boundary = max(local.x, max(local.y, local.z));
        float weight = saturate((1.0f - boundary) / edge);
        float localDensity = localFogPositionDensity[volumeIndex].w * weight;
        float combined = density + localDensity;
        mediumColor = combined > 0.00001f
            ? (mediumColor * density + localFogColorAnisotropy[volumeIndex].rgb * localDensity) / combined
            : mediumColor;
        anisotropy = combined > 0.00001f
            ? (anisotropy * density + localFogColorAnisotropy[volumeIndex].w * localDensity) / combined
            : anisotropy;
        density = combined;
    }
    
    float3 totalScattering = 0.0f;
    float totalExtinction = density;
    
    float3 sunDir = normalize(sunDirectionIntensity.xyz);
    float phaseDot = dot(direction, sunDir);
    float phaseDenominator = max(0.001f, pow(1.0f + anisotropy * anisotropy - 2.0f * anisotropy * phaseDot, 1.5f));
    float sunPhase = (1.0f - anisotropy * anisotropy) / (4.0f * PI * phaseDenominator);
    
    float3 sunLight = sunDirectionIntensity.w * sunPhase * sunRadiance.rgb *
        sunVisibility(worldPos, distanceMeters);
    // Independent art controls keep extinction/visibility unchanged while
    // allowing low-density shafts to read. Defaults 1/0 retain legacy output.
    totalScattering += (sunLight * oceanHorizonParams.z + oceanHorizonParams.w) * mediumColor;
    
    uint lightCount = volumeSize.w;
    uint lightBufferIdx = asuint(waterParams.z);
    if (lightBufferIdx != 0xFFFFFFFF && lightCount > 0)
    {
        StructuredBuffer<GpuLocalLight> lights = BindlessBuffers[lightBufferIdx];
        for (uint i = 0; i < lightCount; ++i)
        {
            GpuLocalLight light = lights[i];
            float3 toLight = light.positionRange.xyz - worldPos;
            float dist = length(toLight);
            float range = light.positionRange.w;
            if (dist < range)
            {
                float atten = saturate(1.0f - (dist / range));
                atten *= atten;
                
                float3 lightDir = normalize(toLight);
                float cosTheta = dot(-lightDir, light.directionType.xyz);
                
                float type = light.directionType.w;
                float spotAtten = 1.0f;
                if (type > 1.5f)
                {
                    float innerCos = light.coneRectFlags.x;
                    float outerCos = light.coneRectFlags.y;
                    spotAtten = saturate((cosTheta - outerCos) / max(0.0001f, innerCos - outerCos));
                }
                
                float localPhase = 0.25f + 0.75f * pow(saturate(dot(direction, lightDir)), 4.0f);
                totalScattering += light.colorIntensity.rgb * light.colorIntensity.w * atten * spotAtten * localPhase;
            }
        }
    }
    
    // Dispatch slot u1 (8) is the 3D output, packed in waterDeep.w.
    RWTexture3D<float4> injectedGrid = getOutputVolume();
    injectedGrid[id] = float4(totalScattering * totalExtinction, totalExtinction);
}

[numthreads(8, 8, 1)]
void IntegrateFroxelVolumeCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= volumeSize.xy))
        return;
        
    float4 accumulatedScattering = float4(0, 0, 0, 1);
    
    Texture3D<float4> injectedGrid = BindlessTextures3D[asuint(waterShallow.x)];
    RWTexture3D<float4> integratedVolume = getOutputVolume();
    
    
    for (uint z = 0; z < volumeSize.z; ++z)
    {
        float sliceThickness = froxelDistance(float(z + 1) / float(volumeSize.z)) -
                               froxelDistance(float(z) / float(volumeSize.z));
        uint3 voxelIndex = uint3(id.xy, z);
        float4 sliceData = injectedGrid.Load(int4(voxelIndex, 0));
        
        float3 scattering = sliceData.rgb;
        float extinction = sliceData.a;
        
        float transmittance = exp(-extinction * sliceThickness);
        float3 integralScattering = scattering * (1.0f - transmittance) / max(extinction, 0.0000001f);
        
        accumulatedScattering.rgb += integralScattering * accumulatedScattering.a;
        accumulatedScattering.a *= transmittance;
        
        integratedVolume[voxelIndex] = accumulatedScattering;
    }
}

[numthreads(8, 8, 1)]
void CloudMapCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float2 wind = normalize(cloudParams1.xy + 0.0001f) *
                  cloudParams1.z * cameraPositionTime.w * 0.00004f;
    float shape = fbm(uv * 7.0f + wind);
    float detail = fbm(uv * 29.0f - wind * 2.3f);
    float coverage = smoothstep(
        1.0f - cloudParams0.x,
        1.0f,
        shape * 0.82f + detail * 0.18f);
    float density = coverage * cloudParams0.y;
    float light = 0.55f + 0.45f * saturate(
        dot(normalize(sunDirectionIntensity.xyz), float3(0.0f, 1.0f, 0.0f)));
    getOutputTexture()[id.xy] = float4(
        lerp(float3(0.42f, 0.48f, 0.56f), float3(1.0f, 0.96f, 0.88f), light),
        density);
}

float3 snowHash(int3 cell) {
    uint3 p=asuint(cell);uint n=p.x*1597334677u^p.y*3812015801u^p.z*2798796415u;
    n^=n>>16;n*=2246822519u;n^=n>>13;
    uint3 h=uint3(n,n*3266489917u+668265263u,n*374761393u+1274126177u);
    h^=h>>15;return float3(h&0x00ffffffu)/16777216.f;
}
// Analytic world-space flakes in advected cells. Each sphere is fully inside
// its cell, so bounded DDA cannot miss neighbouring flakes. One existing HDR
// composite, no per-flake scene objects/uploads. After TAA: no false motion
// history; analytic pixel coverage keeps subpixel flakes stable.
float snowCoverage(float3 ray,float sceneDistance) {
    const float cellSize=1.6f;
    float time=weatherMotion.x;
    float3 velocity=float3(weatherMotion.y,-weatherMotion.w,weatherMotion.z);
    float3 origin=cameraPositionTime.xyz-velocity*time;
    int3 cell=int3(floor(origin/cellSize));
    float3 stepDir=float3(ray.x>=0?1:-1,ray.y>=0?1:-1,ray.z>=0?1:-1);
    float3 inverse=1.f/max(abs(ray),.00001f);
    float3 boundary=(float3(cell)+step(.0f,ray))*cellSize;
    float3 next=abs(boundary-origin)*inverse;
    float3 delta=cellSize*inverse;
    float end=min(sceneDistance,weatherFog.w);
    float sum=0;
    [loop] for(uint visit=0;visit<40;++visit) {
        float3 h=snowHash(cell);
        if(h.z<.62f) {
            float phase=6.2831853f*h.z;
            float3 center=(float3(cell)+.2f+.6f*h)*cellSize;
            center.x+=.10f*sin(time*1.1f+phase+cell.y*.7f);
            center.z+=.10f*cos(time*.9f+phase+cell.x*.6f);
            float3 relative=center-origin;
            float along=dot(relative,ray);
            float radius=weatherColor.w*lerp(.65f,1.4f,h.y);
            float pixelRadius=max(.0005f,along*cameraRightNear.w*2.f/outputSizeFrame.y*.65f);
            if(along>.45f && along<end && along<sceneDistance-radius) {
                float closest=length(relative-ray*along);
                float coverage=saturate((radius+pixelRadius-closest)/(2*pixelRadius));
                coverage*=min(1.f,radius*radius/(pixelRadius*pixelRadius));
                float fade=smoothstep(.45f,1.1f,along)*(1-smoothstep(weatherFog.w*.8f,weatherFog.w,along));
                float visibility=exp(-weatherFog.x*max(0.f,along-weatherFog.y));
                sum+=coverage*fade*visibility*.85f;
            }
        }
        float nearest=min(next.x,min(next.y,next.z));
        if(nearest>end)break;
        // Advance ties together; no negative-cell or axis-aligned singularity.
        float3 advance=step(next,nearest+.00001f);
        cell+=int3(stepDir*advance);next+=delta*advance;
    }
    return saturate(sum)*weatherFog.z;
}

[numthreads(8, 8, 1)]
void AerialCompositeCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float4 scene = getSceneTexture().Load(int3(id.xy, 0));
    float linearDepth = saturate(getAuxiliaryTexture().Load(int3(id.xy, 0)).a);
    float distanceMeters = lerp(compositeParams.x, cameraForwardFar.w, linearDepth);
    float oceanDistance=oceanBackgroundDistance(uv);
    if(linearDepth>=.99999f && oceanDistance>0)
        distanceMeters=min(distanceMeters,oceanDistance);
    float3 color = scene.rgb;
    if (compositeParams.y > .5f) {
        float4 aerial = getAerialTexture().SampleLevel(samplerLinearClamp,
            float3(uv, saturate(distanceMeters / max(atmosphereParams1.y, 1))), 0);
        color = color * aerial.a + aerial.rgb;
    }
    if (compositeParams.z > .5f) {
        // Integrated slices are written at their far boundary, not their centre.
        float volumeDepth = froxelCoordinate(distanceMeters);
        float4 fog = getFroxelTexture().SampleLevel(samplerLinearClamp,
            float3(uv, max(0, volumeDepth - .5f / max(volumeSize.z, 1u))), 0);
        float nearWeight = saturate(volumeDepth * max(volumeSize.z, 1u));
        fog = lerp(float4(0,0,0,1), fog, nearWeight);
        color = color * fog.a + fog.rgb;
    }
    float weatherTransmission=exp(-weatherFog.x*max(0.f,distanceMeters-weatherFog.y));
    color=lerp(weatherColor.rgb,color,weatherTransmission);
    if(weatherFog.z>.0001f) {
        float snow=snowCoverage(viewRay(uv),distanceMeters);
        color=lerp(color,float3(.92f,.96f,1.f),snow);
    }
    getOutputTexture()[id.xy] = float4(color, scene.a);
}

[numthreads(8, 8, 1)]
void CloudCompositeCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float4 scene = getSceneTexture().Load(int3(id.xy, 0));
    if (cloudParams0.y <= 0) { getOutputTexture()[id.xy] = scene; return; }
    float linearDepth = getAuxiliaryTexture().Load(int3(id.xy, 0)).a;
    float4 cloud = getEffectTexture().SampleLevel(samplerLinearWrap, uv, 0.0f);
    float skyMask = smoothstep(0.985f, 1.0f, linearDepth);
    if(oceanBackgroundDistance(uv)>0) skyMask=0;
    float horizonFade = smoothstep(0.0f, 0.35f, 1.0f - abs(uv.y - 0.42f));
    float opacity = cloud.a * skyMask * horizonFade;
    float shadow = 1.0f - opacity * cloudParams1.w;
    float3 color = lerp(scene.rgb * shadow, cloud.rgb, opacity);
    getOutputTexture()[id.xy] = float4(color, scene.a);
}

[numthreads(8, 8, 1)]
void WaterCompositeCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float4 scene = getWaterSceneTexture().Load(int3(id.xy, 0));
    if (waterParams.w < .5f) { getWaterOutputTexture()[id.xy] = scene; return; }
    float linearDepth = getWaterAuxiliaryTexture().Load(int3(id.xy, 0)).a;
    float3 ray = viewRay(uv);
    float denominator = ray.y;
    float waterDistance = denominator < -0.0001f
        ? -cameraPositionTime.y / denominator
        : -1.0f;
    float sceneDistance = linearDepth * cameraForwardFar.w;
    float visible = waterDistance > 0.0f &&
                    waterDistance + 0.35f < sceneDistance
        ? 1.0f
        : 0.0f;
    float2 worldUv =
        (cameraPositionTime.xz + ray.xz * max(waterDistance, 0.0f)) * 0.035f;
    float wave = fbm(
        worldUv +
        cameraPositionTime.w * float2(0.021f, 0.013f));
    float fresnel = pow(1.0f - saturate(abs(ray.y)), 5.0f);
    float depthFactor = saturate(waterDistance / 120.0f);
    float3 waterColor = lerp(waterShallow.rgb, waterDeep.rgb, depthFactor);
    float3 reflection = physicalSky(reflect(ray, float3(0.0f, 1.0f, 0.0f)));
    float foam = smoothstep(0.72f, 0.92f, wave) * waterParams.z;
    float3 composed = lerp(
        scene.rgb * waterParams.y,
        lerp(waterColor, reflection, saturate(fresnel * waterParams.x + 0.15f)),
        0.82f);
    composed += foam.xxx;
    getWaterOutputTexture()[id.xy] = float4(lerp(scene.rgb, composed, visible), scene.a);
}

[numthreads(8, 8, 1)]
void BloomCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    uint sourceWidth = 0;
    uint sourceHeight = 0;
    getSceneTexture().GetDimensions(sourceWidth, sourceHeight);
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    float2 texel = 1.0f / float2(sourceWidth, sourceHeight);
    float3 color = 0.0f;
    float weight = 0.0f;
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float sampleWeight = exp(-0.7f * float(x * x + y * y));
            color += getSceneTexture().SampleLevel(
                samplerLinearClamp,
                uv + float2(x, y) * texel * 2.0f,
                0.0f).rgb * sampleWeight;
            weight += sampleWeight;
        }
    }
    color /= max(weight, 0.0001f);
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float contribution = max(0.0f, luminance - lensParams0.x);
    getOutputTexture()[id.xy] = float4(
        color * contribution / max(luminance, 0.0001f),
        1.0f);
}

[numthreads(1, 1, 1)]
void ExposureCS(uint3 id : SV_DispatchThreadID)
{
    uint sourceWidth = 0;
    uint sourceHeight = 0;
    getSceneTexture().GetDimensions(sourceWidth, sourceHeight);
    float logLuminance = 0.0f;
    uint sampleCount = 0;
    [unroll]
    for (uint y = 0; y < 9; ++y)
    {
        [unroll]
        for (uint x = 0; x < 16; ++x)
        {
            uint2 pixel = uint2(
                (x * sourceWidth + sourceWidth / 2u) / 16u,
                (y * sourceHeight + sourceHeight / 2u) / 9u);
            float3 color = getSceneTexture().Load(int3(pixel, 0)).rgb;
            float luminance = max(
                0.0001f,
                dot(color, float3(0.2126f, 0.7152f, 0.0722f)));
            logLuminance += log2(luminance);
            ++sampleCount;
        }
    }
    float average = exp2(logLuminance / max(1u, sampleCount));
    float exposure = lensParams0.z;
    if (lensParams0.w > 0.5f)
    {
        float ev = clamp(
            log2(0.18f / max(average, 0.0001f)) + atmosphereParams1.w,
            fogColor.w,
            waterShallow.w);
        exposure *= exp2(ev);
    }
    getExposureTexture()[uint2(0, 0)] = max(0.001f, exposure);
}

[numthreads(8, 8, 1)]
void LensCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= outputSizeFrame.xy))
        return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(outputSizeFrame.xy);
    if (sunRadiance.w < .5f) {
        getOutputTexture()[id.xy] = getSceneTexture().Load(int3(id.xy,0));
        return;
    }
    float2 centered = uv - 0.5f;
    float aberration = lensParams2.y * 0.002f;
    float3 color;
    color.r = getSceneTexture().SampleLevel(
        samplerLinearClamp,
        uv + centered * aberration,
        0.0f).r;
    color.g = getSceneTexture().SampleLevel(samplerLinearClamp, uv, 0.0f).g;
    color.b = getSceneTexture().SampleLevel(
        samplerLinearClamp,
        uv - centered * aberration,
        0.0f).b;

    float2 texel = 1.0f / float2(outputSizeFrame.xy);
    float3 neighbors =
        getSceneTexture().SampleLevel(samplerLinearClamp, uv + float2(texel.x, 0), 0).rgb +
        getSceneTexture().SampleLevel(samplerLinearClamp, uv - float2(texel.x, 0), 0).rgb +
        getSceneTexture().SampleLevel(samplerLinearClamp, uv + float2(0, texel.y), 0).rgb +
        getSceneTexture().SampleLevel(samplerLinearClamp, uv - float2(0, texel.y), 0).rgb;
    color += (color * 4.0f - neighbors) * lensParams2.w;

    uint bloomWidth = 0;
    uint bloomHeight = 0;
    getAuxiliaryTexture().GetDimensions(bloomWidth, bloomHeight);
    float3 bloom = getAuxiliaryTexture().SampleLevel(
        samplerLinearClamp,
        uv,
        0.0f).rgb;
    color += bloom * lensParams0.y;

    float exposure = getEffectTexture().Load(int3(0, 0, 0)).r;
    color *= exposure;
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(luminance.xxx, color, lensParams1.y);
    color = max(0.0f, (color - 0.18f) * lensParams1.x + 0.18f);
    color.r += lensParams1.z * 0.05f;
    color.b -= lensParams1.z * 0.05f;
    color.g += lensParams1.w * 0.02f;

    float vignette = smoothstep(0.82f, 0.18f, dot(centered, centered));
    color *= lerp(1.0f, vignette, lensParams2.x);
    float grain = hash12(float2(id.xy) + outputSizeFrame.z * 17.0f) - 0.5f;
    color += grain * lensParams2.z;
    getOutputTexture()[id.xy] = float4(max(color,0),1);
}

Texture2D<float> standaloneShadowMap : register(t0);
StructuredBuffer<GpuLocalLight> standaloneLocalLights : register(t1);
Texture3D<float4> standaloneInjectedGrid : register(t2);

RWTexture3D<float4> standaloneOutputVolume : register(u0);

[numthreads(8, 8, 1)]
void StandaloneInjectCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id >= volumeSize.xyz))
        return;
    float3 uvw = (float3(id) + 0.5f) / float3(volumeSize.xyz);
    float distanceMeters = uvw.z * volumetricParams.z;
    float3 direction = viewRay(uvw.xy);
    float3 worldPos = cameraPositionTime.xyz + direction * distanceMeters;
    
    float sampleHeight = worldPos.y;
    float heightDensity = exp(-max(0.0f, sampleHeight) * max(0.0f, volumetricParams.w));
    float density = max(0.0f, volumetricParams.x) * heightDensity;
    
    float3 totalScattering = 0.0f;
    float totalExtinction = density;
    
    float3 sunDir = normalize(sunDirectionIntensity.xyz);
    float sunPhase = 0.35f + 0.65f * pow(saturate(dot(direction, sunDir)), 8.0f);
    
    float3 sunLight = sunDirectionIntensity.w * sunPhase * physicalSky(sunDir);
    totalScattering += sunLight;
    
    uint lightCount = volumeSize.w;
    if (lightCount > 0)
    {
        for (uint i = 0; i < lightCount; ++i)
        {
            GpuLocalLight light = standaloneLocalLights[i];
            float3 toLight = light.positionRange.xyz - worldPos;
            float dist = length(toLight);
            float range = light.positionRange.w;
            if (dist < range)
            {
                float atten = saturate(1.0f - (dist / range));
                atten *= atten;
                
                float3 lightDir = normalize(toLight);
                float cosTheta = dot(-lightDir, light.directionType.xyz);
                
                float type = light.directionType.w;
                float spotAtten = 1.0f;
                if (type > 1.5f)
                {
                    float innerCos = light.coneRectFlags.x;
                    float outerCos = light.coneRectFlags.y;
                    spotAtten = saturate((cosTheta - outerCos) / max(0.0001f, innerCos - outerCos));
                }
                
                float localPhase = 0.25f + 0.75f * pow(saturate(dot(direction, lightDir)), 4.0f);
                totalScattering += light.colorIntensity.rgb * light.colorIntensity.w * atten * spotAtten * localPhase;
            }
        }
    }
    
    standaloneOutputVolume[id] = float4(totalScattering * totalExtinction, totalExtinction);
}

[numthreads(8, 8, 1)]
void StandaloneIntegrateCS(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= volumeSize.xy))
        return;
        
    float4 accumulatedScattering = float4(0, 0, 0, 1);
    float sliceThickness = volumetricParams.z / float(volumeSize.z);
    
    for (uint z = 0; z < volumeSize.z; ++z)
    {
        uint3 voxelIndex = uint3(id.xy, z);
        float4 sliceData = standaloneInjectedGrid.Load(int4(voxelIndex, 0));
        
        float3 scattering = sliceData.rgb;
        float extinction = sliceData.a;
        
        float transmittance = exp(-extinction * sliceThickness);
        float3 integralScattering = scattering * (1.0f - transmittance) / max(extinction, 0.0001f);
        
        accumulatedScattering.rgb += integralScattering * accumulatedScattering.a;
        accumulatedScattering.a *= transmittance;
        
        standaloneOutputVolume[voxelIndex] = accumulatedScattering;
    }
}

