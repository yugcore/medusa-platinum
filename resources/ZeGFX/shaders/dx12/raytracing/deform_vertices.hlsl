// Reuse the raster vertex program for skinning, instance transforms and plant
// wind. This keeps acceleration-structure positions consistent with raster.
#define ZE_GPU_INSTANCING 1
#define ZE_FOLIAGE_GRASS 1
#include "../materials/pbr_material.hlsl"
cbuffer DeformConfig:register(b5){uint vertexCount;uint vertexStride;uint vertexOffset;uint padding;float4 row0,row1,row2,color0,color1;}
ByteAddressBuffer inputVertices:register(t32);
RWByteAddressBuffer outputVertices:register(u0);
float2 SignedPair(uint word){return max(float2(int(word<<16)>>16,int(word)>>16)/32767.0,-1);}
uint PackOct(float3 normal,bool tangent,bool negative){
    normal/=abs(normal.x)+abs(normal.y)+abs(normal.z);float2 p=normal.xy;
    if(normal.z<0)p=(1-abs(p.yx))*float2(p.x>=0?1:-1,p.y>=0?1:-1);
    int2 packed=int2(round(clamp(p,-1,1)*32767));if(tangent)packed.x=(packed.x&~1)|(negative?1:0);
    return (uint(packed.x)&65535)|(uint(packed.y)<<16);
}
[numthreads(64,1,1)]
void CSDeform(uint3 id:SV_DispatchThreadID){
    if(id.x>=vertexCount)return;uint address=vertexOffset+id.x*vertexStride;VSInput v=(VSInput)0;
    v.pos=asfloat(inputVertices.Load3(address));v.octNormal=SignedPair(inputVertices.Load(address+12));v.octTangent=SignedPair(inputVertices.Load(address+16));
    uint uv=inputVertices.Load(address+20),uv1=inputVertices.Load(address+24),color=inputVertices.Load(address+28);
    v.uv=float2(f16tof32(uv&65535),f16tof32(uv>>16));v.uv1=float2(f16tof32(uv1&65535),f16tof32(uv1>>16));
    v.color=float4(color&255,(color>>8)&255,(color>>16)&255,color>>24)/255.0;
    uint2 joints=inputVertices.Load2(address+32),weights=inputVertices.Load2(address+40);
    v.joints = uint4(joints.x & 65535, joints.x >> 16, joints.y & 65535, joints.y >> 16);
    v.weights =
        float4(weights.x & 65535, weights.x >> 16, weights.y & 65535, weights.y >> 16) / 65535.0;
    v.instanceRow0=row0;v.instanceRow1=row1;v.instanceRow2=row2;v.instanceColor0=color0;v.instanceColor1=color1;
    PSInput deformed=VSMain(v);uint destination=id.x*48;
    [unroll]for(uint word=0;word<12;++word)outputVertices.Store(destination+word*4,inputVertices.Load(address+word*4));
    outputVertices.Store3(destination,asuint(deformed.worldPos-cameraPos.xyz));
    outputVertices.Store(destination+12,PackOct(normalize(deformed.worldNormal),false,false));
    outputVertices.Store(
        destination + 16,
        PackOct(normalize(deformed.worldTangent),
                true,
                dot(cross(deformed.worldNormal, deformed.worldTangent), deformed.worldBitan) < 0));
}
