#include "surface_effects.hlsl"
float4 PSAutoExposure(Fragment input):SV_TARGET {
    uint bins[64];[unroll]for(uint i=0;i<64;++i)bins[i]=0;
    // Uniform coverage with trimmed log-luminance prevents small emitters from
    // driving the whole frame and preserves the HDR ordering before tone mapping.
    [loop]for(uint y=0;y<32;++y)[loop]for(uint x=0;x<32;++x) {
        uint2 pixel=min(uint2((float2(x,y)+.5)*size.xy/32),uint2(size.xy)-1);
        float3 rgb=max(source.Load(int3(pixel,0)).rgb,0);
        float ev=log2(max(dot(rgb,float3(.2126,.7152,.0722)),1e-6)/.18);
        uint bin=(uint)clamp((ev+20)*64/40,0,63);bins[bin]++;
    }
    float sum=0,count=0;uint cumulative=0;
    [unroll]for(uint bin=0;bin<64;++bin) {
        uint begin=cumulative;cumulative+=bins[bin];
        float used=max(0.0,min(float(cumulative),1004.0)-max(float(begin),20.0));
        sum+=used*((bin+.5)*40/64-20);count+=used;
    }
    float ev=clamp(sum/max(count,1),trace.x,trace.y);
    float target=exp2(-ev+trace.z),old=previous.Load(int3(0,0,0)).r;
    float speed=target>old?trace.w:history.x;
    float value=history.z>.5?lerp(old,target,1-exp(-speed*history.y)):target;
    return float4(clamp(value,1e-6,1e6),ev,exp2(ev)*.18,1);
}
float DepthOfFieldRadius(float2 uv,float z) {
    float distance=z>=.999999?1e6:max(.02,dot(Reconstruct(uv,z),camera.xyz));
    float focus=max(trace.x,trace.z+.001);
    float coc=trace.z*trace.z*(distance-focus)/(max(trace.y,.5)*distance*(focus-trace.z));
    return clamp(coc/.024*size.y*.5,-trace.w,trace.w);
}
float4 PSDepthOfField(Fragment input):SV_TARGET {
    int2 pixel=int2(input.position.xy);float depth=depthTex.Load(int3(pixel,0));
    float radius=DepthOfFieldRadius(input.uv,depth);
    float4 center=source.Load(int3(pixel,0));if(abs(radius)<.5)return center;
    float4 sum=center;float total=1;
    [unroll]for(uint tap=0;tap<48;++tap) {
        float angle=tap*2.39996323,r=sqrt((tap+.5)/48)*abs(radius);
        float2 uv=clamp(input.uv+float2(cos(angle),sin(angle))*r*size.zw,size.zw*.5,1-size.zw*.5);
        float z=depthTex.SampleLevel(pointClamp,uv,0),sampleRadius=DepthOfFieldRadius(uv,z);
        // In-focus foreground must not acquire distant background colors.
        float weight=radius<0?(sampleRadius<=radius+.75?1:0):(sampleRadius<-.5?0:1);
        sum+=source.SampleLevel(linearClamp,uv,0)*weight;total+=weight;
    }
    return sum/max(total,1);
}
