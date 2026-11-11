#include "../common/ze_ray_counters.hlsli"
cbuffer Configuration:register(b0){uint total;uint category;}
RWByteAddressBuffer measured:register(u1);
[numthreads(64,1,1)]
void CSMain(uint3 id:SV_DispatchThreadID) {
    if(id.x>=total)return;
    if(id.x%3==0)ZeCountShadingRay(measured,category,true);
    else ZeCountShadingRay(measured,category,false);
}
