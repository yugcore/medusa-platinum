#ifndef ZE_INSTANCING_HLSLI
#define ZE_INSTANCING_HLSLI
void ZeInstanceVertex(float4 row0,float4 row1,float4 row2,
                      inout float3 position,inout float3 normal,inout float2 uv)
{
    float4 p=float4(position,1);
    position=float3(dot(row0,p),dot(row1,p),dot(row2,p));
    float3 a=float3(row0.x,row1.x,row2.x),b=float3(row0.y,row1.y,row2.y),c=float3(row0.z,row1.z,row2.z);
    float determinant=dot(a,cross(b,c));
    normal=normalize((cross(b,c)*normal.x+cross(c,a)*normal.y+cross(a,b)*normal.z)
                     * (determinant<0 ? -1 : 1));
}
#endif
