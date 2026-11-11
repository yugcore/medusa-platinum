#ifndef ZE_VIRTUAL_TEXTURE_HLSLI
#define ZE_VIRTUAL_TEXTURE_HLSLI
// Page table: R/G physical tile indices, B interior pixels, A validity.
// Pool uses padded sRGB color pages; page table is always linear UNORM.
bool ZeSampleVirtualColor(Texture2D pool,Texture2D pageTable,SamplerState samplerState,
                          float2 worldXZ,float4 parameters,out float3 color) {
    color=0;if(parameters.z<=0||parameters.w<=0)return false;
    float2 page=(worldXZ-parameters.xy)/parameters.z;
    int2 logical=int2(floor(page));uint width,height;pageTable.GetDimensions(width,height);
    if(any(logical<0)||logical.x>=int(width)||logical.y>=int(height))return false;
    float4 entry=pageTable.Load(int3(logical,0));if(entry.a<.5)return false;
    float2 slot=round(entry.rg*255);float pixels=round(entry.b*255),stride=pixels+2;
    uint poolWidth,poolHeight;pool.GetDimensions(poolWidth,poolHeight);
    float2 uv=(slot*stride+1+frac(page)*pixels)/float2(poolWidth,poolHeight);
    color=pool.SampleLevel(samplerState,uv,0).rgb;return true;
}
#endif
