// ZPostProcessMaterialCommon.hlsli - Shared Custom Pass Shader Header

struct ZPostProcessMaterialParams
{
    uint SceneColorIndex;
    uint DepthTextureIndex;
    uint CustomDepthTextureIndex;
    uint VelocityTextureIndex;
    
    uint GBufferAIndex;
    uint GBufferBIndex;
    uint GBufferCIndex;
    uint GBufferDIndex;

    float4 CustomParameters[8];
};

ConstantBuffer<ZPostProcessMaterialParams> MaterialParams : register(b0);
Texture2D<float4> BindlessTextures[] : register(t0, space0);
SamplerState      BilinearSampler    : register(s0);

float4 GetSceneColor(float2 uv)
{
    return BindlessTextures[MaterialParams.SceneColorIndex].SampleLevel(BilinearSampler, uv, 0);
}

float GetSceneDepth(float2 uv)
{
    return BindlessTextures[MaterialParams.DepthTextureIndex].SampleLevel(BilinearSampler, uv, 0).r;
}
