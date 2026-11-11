struct VSInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    uint4 joints : JOINTS;
    float4 weights : WEIGHTS;
    #include "../common/ze_instance_input.hlsli"
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer FrameConstants : register(b0) {
    row_major float4x4 viewProj;
    float4 cameraPos;
    float4 lightDir;
    float4 lightColor;

    float4 skyHorizonColor;
    float4 skyZenithColor;
    float skyIntensity;
    float exposure;
    float gammaValue;
    float lightingDebugMode;

    float4 fogColor;
    float fogDensity;
    float fogStart;
    float fogEnd;
    float fogEnabled;

    row_major float4x4 lightViewProj;
};

cbuffer ObjectConstants : register(b1) {
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
};

cbuffer SkinningMatrices : register(b2) {
    row_major float4x4 boneMatrices[256];
};

Texture2D diffuseMap : register(t0);
SamplerState pointSampler : register(s0);

#include "../common/ze_grass_wind_v2.hlsli"
#include "../common/ze_plant_wind.hlsli"
#include "../common/ze_instancing.hlsli"

PSInput VSMain(VSInput input) {
    float plantWeight=saturate((input.position.y-foliageWindGeometry.x)/max(.001,foliageWindGeometry.y));
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

#ifdef ZE_GPU_INSTANCING
    float3 unusedNormal=float3(0,1,0);
    ZeInstanceVertex(input.instanceRow0,input.instanceRow1,input.instanceRow2,input.position,unusedNormal,input.uv);
    if(grassWindParams.w>=.5)input.uv.x+=input.instanceRow1.w;
#endif
    if (isSkinned > 0.5f) {
        float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float totalWeight = 0.0f;
        for (int i = 0; i < 4; ++i) {
            float w = input.weights[i];
            if (w > 0.0f) {
                uint jointIdx = input.joints[i];
                row_major float4x4 boneMat = boneMatrices[jointIdx];
                skinnedPos += mul(float4(input.position, 1.0f), boneMat) * w;
                totalWeight += w;
            }
        }
        if (totalWeight > 0.001f) {
            input.position = skinnedPos.xyz;
        }
    }

    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), world);
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
    worldPos.xyz = ZeGrassVertex(worldPos.xyz, input.position.y, input.uv, cameraPos.xyz,
        max(alphaCutoff*180.0,20.0), grassWindDirectionSpeed, grassWindParams, foliageWindField, bendSlope);
    if(grassWindParams.w>=.5 && grassWindDirectionSpeed.y>0) {
        float radius=grassWindDirectionSpeed.y;
        float fade=max(lmPadding1,.001);
        float visibility=1-smoothstep(radius-fade,radius,length(worldPos.xz-cameraPos.xz));
        float rootY=mul(float4(input.position.x,input.uv.x,input.position.z,1),world).y;
        worldPos.y=lerp(rootY,worldPos.y,visibility);
    }
    output.position = mul(worldPos, lightViewProj);
    output.uv = input.uv;
    return output;
}

void PSMain(PSInput input) {
    if (alphaMode > 0.5f && alphaMode < 1.5f) {
        float alpha = objectColor.a;
        if (hasBaseColorTexture > 0.5f) alpha *= diffuseMap.Sample(pointSampler,input.uv).a;
        clip(alpha - alphaCutoff);
    }
}
