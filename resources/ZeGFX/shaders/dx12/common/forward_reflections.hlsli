Texture2D<float4> forwardOpaqueColor:register(t18);
Texture2D<float> forwardOpaqueDepth:register(t21);
cbuffer ForwardReflectionConstants:register(b6){row_major float4x4 forwardInverseVP,forwardVP;float4 forwardCamera,forwardSize,forwardJitter,forwardTrace,forwardOptions;}
float3 ForwardReconstruct(float2 uv,float depth){float4 p=mul(float4(uv*float2(2,-2)+float2(-1,1)-forwardJitter.xy,depth,1),forwardInverseVP);return p.xyz/p.w;}
bool ForwardProject(float3 p,out float2 uv){float4 clip=mul(float4(p,1),forwardVP);uv=(clip.xy/max(clip.w,1e-6)+forwardJitter.xy)*float2(.5,-.5)+.5;return clip.w>0&&clip.z>0&&clip.z<clip.w&&all(uv>0)&&all(uv<1);}
float4 ForwardScreenReflection(float3 worldPosition,float3 n,float3 v,float roughness){
    if(forwardOptions.x<.5||roughness>.85)return 0;float3 p=worldPosition-forwardCamera.xyz,r=reflect(-v,n),origin=p+n*max(.005,forwardTrace.y*.2);
    float distance=max(.02,forwardTrace.y),previousDistance=distance;float2 sourceUv;ForwardProject(p,sourceUv);
    [loop]for(uint step=0;step<uint(forwardOptions.y)&&distance<forwardTrace.x;++step){
        float3 q=origin+r*distance;float2 uv;if(!ForwardProject(q,uv))break;float z=forwardOpaqueDepth.SampleLevel(clampSampler,uv,0);
        if(z>0&&z<1){float delta=length(q)-length(ForwardReconstruct(uv,z));
            if(delta>=0&&delta<max(forwardTrace.y,.005)&&length((uv-sourceUv)*forwardSize.xy)>2){
                float lo=previousDistance,hi=distance;
                [loop]for(uint refine=0;refine<uint(forwardTrace.w);++refine){float mid=(lo+hi)*.5;float2 tap;if(!ForwardProject(origin+r*mid,tap))break;float depth=forwardOpaqueDepth.SampleLevel(clampSampler,tap,0);if(length(origin+r*mid)>length(ForwardReconstruct(tap,depth)))hi=mid;else lo=mid;}
                ForwardProject(origin+r*hi,uv);float edge=saturate(min(min(uv.x,uv.y),min(1-uv.x,1-uv.y))*12);
                return float4(forwardOpaqueColor.SampleLevel(clampSampler,uv,0).rgb,edge*(1-smoothstep(.55,.85,roughness))*(1-hi/max(forwardTrace.x,.1)));
            }
        }
        previousDistance=distance;float2 next;ForwardProject(q+r*.1,next);float pixels=length((next-uv)*forwardSize.xy)*10;distance+=clamp(max(forwardTrace.z,1)/max(pixels,.001),.01,max(forwardTrace.x/8,.01));
    }return 0;
}
