#ifndef ZE_SHADOW_FILTER_HLSLI
#define ZE_SHADOW_FILTER_HLSLI
#include "ze_shadow_receiver.hlsli"
#ifndef ZE_SHADOW_MAP_TYPE
#define ZE_SHADOW_MAP_TYPE Texture2D
#endif

float ZeShadowFilterCompare(ZE_SHADOW_MAP_TYPE map,float2 uv,float2 receiverUv,float2 size,
    float depth,float2 gradient,float bias,float2 minUv,float2 maxUv)
{
    float2 pixel=clamp(uv,minUv,maxUv)*size-.5f;
    float2 origin=floor(pixel),blend=frac(pixel);float visibility=0;
    [unroll] for(int y=0;y<2;++y) [unroll] for(int x=0;x<2;++x) {
        float2 tapUv=clamp((origin+float2(x,y)+.5f)/size,minUv,maxUv);
        int2 tap=clamp(int2(floor(tapUv*size)),int2(0,0),int2(size)-1);
        float receiver=ZeShadowReceiverTapDepth(depth,gradient,tapUv-receiverUv);
        float weight=(x?blend.x:1-blend.x)*(y?blend.y:1-blend.y);
        visibility+=weight*(receiver-bias<=map.Load(int3(tap,0)).r?1.f:0.f);
    }
    return visibility;
}

// Fixed extents let the shader compiler unroll constant array accesses without
// retaining dynamic-radius indexing, masks and the larger kernel's temporaries.
float ZeShadowUnit3(ZE_SHADOW_MAP_TYPE map,float2 uv,float2 size,float depth,float2 gradient,float bias)
{
    float2 pixel=uv*size-.5f,origin=floor(pixel),blend=frac(pixel);
    static const float kernel[3]={.25f,.5f,.25f};
    float2 weights[4];weights[0]=kernel[0]*(1-blend);
    [unroll] for(int i=1;i<3;++i)weights[i]=kernel[i-1]*blend+kernel[i]*(1-blend);
    weights[3]=kernel[2]*blend;float visibility=0;
    [unroll] for(int y=0;y<4;++y) [unroll] for(int x=0;x<4;++x) {
        int2 tap=int2(origin)+int2(x-1,y-1);float2 tapUv=(float2(tap)+.5f)/size;
        float receiver=ZeShadowReceiverTapDepth(depth,gradient,tapUv-uv);
        visibility+=weights[x].x*weights[y].y*(receiver-bias<=map.Load(int3(tap,0)).r?1.f:0.f);
    }
    return visibility;
}
float ZeShadowUnit5(ZE_SHADOW_MAP_TYPE map,float2 uv,float2 size,float depth,float2 gradient,float bias)
{
    float2 pixel=uv*size-.5f,origin=floor(pixel),blend=frac(pixel);
    static const float kernel[5]={.0625f,.25f,.375f,.25f,.0625f};
    float2 weights[6];weights[0]=kernel[0]*(1-blend);
    [unroll] for(int i=1;i<5;++i)weights[i]=kernel[i-1]*blend+kernel[i]*(1-blend);
    weights[5]=kernel[4]*blend;float visibility=0;
    [unroll] for(int y=0;y<6;++y) [unroll] for(int x=0;x<6;++x) {
        int2 tap=int2(origin)+int2(x-2,y-2);float2 tapUv=(float2(tap)+.5f)/size;
        float receiver=ZeShadowReceiverTapDepth(depth,gradient,tapUv-uv);
        visibility+=weights[x].x*weights[y].y*(receiver-bias<=map.Load(int3(tap,0)).r?1.f:0.f);
    }
    return visibility;
}
float ZeDirectionalShadowFilter(ZE_SHADOW_MAP_TYPE map,float2 uv,float2 size,float depth,
    float2 gradient,float bias,float2 minUv,float2 maxUv,float mode,float softness)
{
    if(mode<.5f) {
        float2 tapUv=clamp(uv,minUv,maxUv);
        int2 tap=clamp(int2(floor(tapUv*size)),int2(0,0),int2(size)-1);
        return ZeShadowReceiverTapDepth(depth,gradient,(float2(tap)+.5f)/size-uv)-bias<=map.Load(int3(tap,0)).r?1.f:0.f;
    }
    float spacing=round(clamp(softness,.25f,4.f)*4.f)*.25f;
    if(mode<1.5f) {
        if(abs(spacing-1.f)<1e-6f&&all(uv>=minUv+1.5f/size)&&all(uv<=maxUv-1.5f/size))
            return ZeShadowUnit3(map,uv,size,depth,gradient,bias);
        static const float kernel[3]={.25f,.5f,.25f};float visibility=0;
        [unroll] for(int y=-1;y<=1;++y) [unroll] for(int x=-1;x<=1;++x)
            visibility+=kernel[x+1]*kernel[y+1]*ZeShadowFilterCompare(map,
                uv+float2(x,y)*spacing/size,uv,size,depth,gradient,bias,minUv,maxUv);
        return visibility;
    }
    if(abs(spacing-1.f)<1e-6f&&all(uv>=minUv+2.5f/size)&&all(uv<=maxUv-2.5f/size))
        return ZeShadowUnit5(map,uv,size,depth,gradient,bias);
    static const float kernel[5]={.0625f,.25f,.375f,.25f,.0625f};float visibility=0;
    [unroll] for(int y=-2;y<=2;++y) [unroll] for(int x=-2;x<=2;++x)
        visibility+=kernel[x+2]*kernel[y+2]*ZeShadowFilterCompare(map,
            uv+float2(x,y)*spacing/size,uv,size,depth,gradient,bias,minUv,maxUv);
    return visibility;
}
#endif
