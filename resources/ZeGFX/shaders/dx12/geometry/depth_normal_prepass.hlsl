#include "../common/ze_pbr_surface.hlsli"
#include "../common/ze_camera_projection.hlsli"
#include "../common/ze_thin_surface.hlsli"
cbuffer FrameConstants : register(b0)
{
    row_major float4x4 viewProj;
    float4 cameraPos;
    float4 lightDir;
    float4 lightColor;

    float4 skyHorizonColor;
    float4 skyZenithColor;
    float skyIntensity;
    float exposure;
    float gammaValue;
    float padding0;

    float4 fogColor;
    float fogDensity;
    float fogStart;
    float fogEnd;
    float fogEnabled;

    row_major float4x4 lightViewProj;
    float shadowConstantBias;
    float shadowSlopeBias;
    float shadowStrength;
    float shadowEnabled;

    float shadowPcfRadius;
    float shadowCascadesCount;
    float shadowDebugMode;
    float padding1;

    float postProcessEnabled;
    float nearPlane;
    float farPlane;
    float shadowNormalBias;
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 world;
    float4 objectColor;
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
    float alphaMode;
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
    row_major float4x4 foliagePreviousViewProj;
    float4 foliageMotionDepth;
};

cbuffer SkinningMatrices : register(b2)
{
    row_major float4x4 boneMatrices[256];
};

#include "../common/octahedral.hlsli"

struct VSInput
{
    float3 pos : POSITION;
    float2 octNormal : NORMAL;
    float2 octTangent : TANGENT;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float4 color : COLOR0;
    uint4 joints : JOINTS;
    float4 weights : WEIGHTS;
    #include "../common/ze_instance_input.hlsli"
};

Texture2D diffuseMap : register(t0);
SamplerState alphaSampler : register(s0);

struct PSInput
{
    float2 uv : TEXCOORD0;
    float4 pos : SV_POSITION;
    float3 worldPos : POSITION;
    float3 worldNormal : NORMAL;
    float4 motionCurrent : TEXCOORD8;
    float4 motionPrevious : TEXCOORD9;
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
#ifdef ZE_GPU_INSTANCING
    ZeInstanceVertex(input.instanceRow0,input.instanceRow1,input.instanceRow2,inPos,inNormal,input.uv);
    if(grassWindParams.w>=.5)input.uv.x+=input.instanceRow1.w;
#endif

    if (isSkinned > 0.5f)
    {
        float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
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
                totalWeight += w;
            }
        }
        if (totalWeight > 0.001f)
        {
            inPos = skinnedPos.xyz;
            inNormal = skinnedNormal;
        }
    }

    PSInput output;
    float4 worldPos = mul(float4(inPos, 1.0f), world);
    float4 previousWindParameters=grassWindParams;
    previousWindParameters.x=foliageWindField.w;
    float previousWindSlope;
    float3 previousWindPosition=ZeTreeVertex(worldPos.xyz,plantWeight,plantRoot,plantHeight,
        grassWindDirectionSpeed,previousWindParameters,foliageWindField,foliageWindGeometry,foliageWindClimate,previousWindSlope);
    previousWindPosition=ZeGrassVertex(previousWindPosition,inPos.y,input.uv,cameraPos.xyz,
        max(alphaCutoff*180.0,20.0),grassWindDirectionSpeed,previousWindParameters,foliageWindField,previousWindSlope);
    float plantSlope;
    worldPos.xyz = ZeTreeVertex(worldPos.xyz,
                                plantWeight,
                                plantRoot,
                                plantHeight,
                                grassWindDirectionSpeed,
                                grassWindParams,
                                foliageWindField,
                                foliageWindGeometry,
                                foliageWindClimate,
                                plantSlope);
    float bendSlope;
    worldPos.xyz = ZeGrassVertex(worldPos.xyz, inPos.y, input.uv, cameraPos.xyz,
        max(alphaCutoff*180.0,20.0), grassWindDirectionSpeed, grassWindParams, foliageWindField, bendSlope);
    inNormal.y -= dot(inNormal.xz, grassWindDirectionSpeed.xz) * bendSlope;
    output.pos = ZeProjectCamera(worldPos, viewProj, float2(lightDir.w, lightColor.w));
    output.uv = input.uv;
    output.motionCurrent=mul(worldPos,viewProj);
    output.motionPrevious=mul(float4(previousWindPosition,1),foliagePreviousViewProj);
    output.worldPos = worldPos.xyz;
    output.worldNormal = ZeTransformNormal(inNormal, (float3x3)world);
    if (foliageWindGeometry.z > 0) {
        output.worldNormal.y -= dot(output.worldNormal.xz, grassWindDirectionSpeed.xz) * plantSlope;
        output.worldNormal = normalize(output.worldNormal);
    }
    return output;
}

struct MotionDepthOutput {float4 normal : SV_Target0;float4 velocity : SV_Target1;};
float PlantLinearDepth(float4 clip){
    float d=clip.z/max(clip.w,1e-6);
    return foliageMotionDepth.z>.5?max(0,(d-foliageMotionDepth.y)/foliageMotionDepth.x):max(0,clip.w);
}
MotionDepthOutput PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace)
{
    if (alphaMode > 0.5f && alphaMode < 1.5f) {
        float alpha = objectColor.a;
        if (hasBaseColorTexture > 0.5f) alpha *= diffuseMap.Sample(alphaSampler,input.uv).a;
        clip(alpha-alphaCutoff);
    }
    float3 N = normalize(input.worldNormal);
    if(terrainSurfaceParams.w<-.5)
        N=ZeThinNormal(N,terrainSurfaceParams.z);
    else if(doubleSided>.5 && dot(N,cameraPos.xyz-input.worldPos)<0)N=-N;
    float3 viewSpaceNormal = N; // Transform to view space approximation
    
    float dist = distance(cameraPos.xyz, input.worldPos);
    float linearDepth = saturate((dist - nearPlane) / (farPlane - nearPlane));
    
    MotionDepthOutput output;
    output.normal=float4(viewSpaceNormal*.5+.5,linearDepth);
    output.velocity=0;
    if((foliageWindGeometry.z>0||grassWindParams.w>=.5)&&input.motionPrevious.w>1e-6&&input.motionCurrent.w>1e-6){
        float2 currentUv=input.motionCurrent.xy/input.motionCurrent.w*float2(.5,-.5)+.5;
        float2 previousUv=input.motionPrevious.xy/input.motionPrevious.w*float2(.5,-.5)+.5;
        output.velocity=float4(previousUv-currentUv,min(65000,PlantLinearDepth(input.motionPrevious)),min(65000,PlantLinearDepth(input.motionCurrent)));
    }
    return output;
}
