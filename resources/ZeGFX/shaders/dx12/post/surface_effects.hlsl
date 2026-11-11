// Screen-space reflections and depth/normal validated temporal surface history.
cbuffer SurfaceConstants : register(b0) {
    row_major float4x4 inverseVP;
    row_major float4x4 viewProjection;
    row_major float4x4 currentToPrevious;
    float4 camera;
    float4 size; // width, height, reciprocal width/height
    float4 jitter; // current xy, previous xy
    float4 trace; // distance, thickness, stride in pixels, binary refinements
    float4 history; // weight, valid, max steps, normal rejection
    float4 environment; // enabled, intensity, mip count, rotation
    float4 horizon;
    float4 zenith;
    float4 volumeData[128]; // Used only by the volume lighting variant.
    float4 probeParams;
};
Texture2D<float4> source : register(t0);
Texture2D<float> depthTex : register(t1);
Texture2D<float4> normalTex : register(t2);
Texture2D<float4> previous : register(t3);
Texture2D<float> previousDepth : register(t4);
Texture2D<float4> previousNormal : register(t5);
Texture2D<float4> baseTex : register(t6);
Texture2D<uint4> materialTex : register(t7);
TextureCube<float4> environmentSpecular : register(t8);
Texture2D<float2> environmentBrdf : register(t9);
Texture2D<float4> reflectionTex : register(t10);
Texture2D<float4> rayReflectionTex : register(t11);
Texture2D<float> ambientVisibility : register(t12);
Texture2D<float4> giGrid : register(t13);
Texture2D<float4> localRadiance : register(t14);
Texture2D<float4> objectMotion : register(t15);
SamplerState linearClamp : register(s0);
SamplerState pointClamp : register(s1);
#include "../common/visual_probe_sampling.hlsli"
struct Fragment { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
Fragment VSMain(uint vertex : SV_VertexID) {
    Fragment output;
    output.uv = float2((vertex << 1) & 2, vertex & 2);
    output.position = float4(output.uv * float2(2,-2) + float2(-1,1),0,1);
    return output;
}
float3 Reconstruct(float2 uv, float z) {
    float4 h = mul(float4(uv * float2(2,-2) + float2(-1,1) - jitter.xy,z,1),inverseVP);
    return h.xyz / h.w;
}
bool Project(float3 p, out float2 uv, out float z) {
    float4 h = mul(float4(p,1),viewProjection);
    uv = (h.xy / max(h.w,1e-6) + jitter.xy) * float2(.5,-.5) + .5;
    z = h.z / max(h.w,1e-6);
    return h.w > 0 && z > 0 && z < 1 && all(uv > 0) && all(uv < 1);
}
float4 PSTemporal(Fragment input) : SV_TARGET {
    int2 pixel = int2(input.position.xy);
    float4 current = source.Load(int3(pixel,0));
    float depth = depthTex.Load(int3(pixel,0));
    if (history.y < .5 || depth >= 1 || depth <= 0) return current;
    float4 oldClip = mul(float4(input.uv * float2(2,-2) + float2(-1,1) - jitter.xy,depth,1),currentToPrevious);
    if (oldClip.w <= 0) return current;
    float3 oldNdc = oldClip.xyz / oldClip.w;
    float2 oldUv = (oldNdc.xy + jitter.zw) * float2(.5,-.5) + .5;
    if(camera.w>.5){float4 motion=objectMotion.Load(int3(pixel,0));if(motion.w>.5){oldUv=input.uv+motion.xy;oldNdc.z=motion.z;}}
    if (any(oldUv < size.zw * .5) || any(oldUv > 1 - size.zw * .5)) return current;
    float oldZ = previousDepth.SampleLevel(pointClamp,oldUv,0);
    float3 n = normalize(normalTex.Load(int3(pixel,0)).xyz);
    float3 oldN = normalize(previousNormal.SampleLevel(pointClamp,oldUv,0).xyz);
    // Device-depth derivatives bound rejection at grazing surfaces without allowing sky history.
    float tolerance = max(2e-5, 2 * (abs(ddx(depth)) + abs(ddy(depth))));
    if (oldZ >= 1 || abs(oldZ - oldNdc.z) > tolerance || dot(n,oldN) < history.w) return current;
    float4 lo = current, hi = current;
    [unroll] for (int y=-1;y<=1;++y) [unroll] for (int x=-1;x<=1;++x) {
        float4 v=source.Load(int3(clamp(pixel+int2(x,y),int2(0,0),int2(size.xy)-1),0));
        lo=min(lo,v);hi=max(hi,v);
    }
    float4 old=clamp(previous.SampleLevel(linearClamp,oldUv,0),lo,hi);
    return lerp(current,old,saturate(history.x));
}
float4 PSSsr(Fragment input) : SV_TARGET {
    int2 pixel=int2(input.position.xy);
    float z=depthTex.Load(int3(pixel,0));
    float4 nr=normalTex.Load(int3(pixel,0));
    if (z>=1 || z<=0 || nr.w>.85) return 0;
    float3 p=Reconstruct(input.uv,z), n=normalize(nr.xyz);
    float3 v=normalize(camera.xyz-p), r=normalize(reflect(-v,n));
    float3 origin=p+n*max(.005,trace.y*.2);
    float maxDistance=max(trace.x,.1);
    float distance=max(.02,trace.y), previousDistance=distance;
    uint steps=(uint)history.z;
    [loop] for(uint s=0;s<steps && distance<maxDistance;++s) {
        float3 q=origin+r*distance;
        float2 uv;float qDepth;
        if (!Project(q,uv,qDepth)) break;
        float sceneDepth=depthTex.SampleLevel(pointClamp,uv,0);
        if (sceneDepth < 1 && sceneDepth > 0) {
            float3 scenePosition=Reconstruct(uv,sceneDepth);
            float qDistance=length(q-camera.xyz), sceneDistance=length(scenePosition-camera.xyz);
            float delta=qDistance-sceneDistance;
            if (delta>=0 && delta<max(trace.y,.005)) {
                float lo=previousDistance,hi=distance;
                [loop] for(int b=0;b<(int)trace.w;++b) {
                    float mid=(lo+hi)*.5;float2 testUv;float testZ;
                    if(!Project(origin+r*mid,testUv,testZ))break;
                    float surfaceZ=depthTex.SampleLevel(pointClamp,testUv,0);
                    float surfaceDistance=length(Reconstruct(testUv,surfaceZ)-camera.xyz);
                    if(length(origin+r*mid-camera.xyz)>surfaceDistance)hi=mid;else lo=mid;
                }
                float2 hitUv;float hitZ;Project(origin+r*hi,hitUv,hitZ);
                float3 hitN=normalize(normalTex.SampleLevel(pointClamp,hitUv,0).xyz);
                if(dot(hitN,r)>=-.01 || length((hitUv-input.uv)*size.xy)<2) return 0;
                float edge=saturate(min(min(hitUv.x,hitUv.y),min(1-hitUv.x,1-hitUv.y))*12);
                float confidence=edge*(1-smoothstep(.55,.85,nr.w))*(1-hi/maxDistance);
                return float4(max(source.SampleLevel(linearClamp,hitUv,0).rgb,0),confidence);
            }
        }
        previousDistance=distance;
        // Projected step size is controlled by the existing pixel-stride setting.
        float2 nextUv;float nextZ;
        Project(q+r*.1,nextUv,nextZ);
        float pixelsPerUnit=length((nextUv-uv)*size.xy)*10;
        distance+=clamp(max(trace.z,1)/max(pixelsPerUnit,1e-3),.01,maxDistance/8);
    }
    return 0;
}
float4 PSSpatial(Fragment input) : SV_TARGET {
    int2 pixel=int2(input.position.xy);float depth=depthTex.Load(int3(pixel,0));
    if(depth>=1||depth<=0)return source.Load(int3(pixel,0));
    float3 p=Reconstruct(input.uv,depth),n=normalize(normalTex.Load(int3(pixel,0)).xyz);
    float4 sum=0;float total=0;int radius=(int)history.z;
    [loop]for(int y=-radius;y<=radius;++y)[loop]for(int x=-radius;x<=radius;++x) {
        int2 tap=clamp(pixel+int2(x,y),int2(0,0),int2(size.xy)-1);
        float z=depthTex.Load(int3(tap,0));if(z>=1||z<=0)continue;
        float3 sampleN=normalize(normalTex.Load(int3(tap,0)).xyz);
        float3 q=Reconstruct((tap+.5)*size.zw,z);
        float weight=exp(-float(x*x+y*y)/max(float(radius*radius),1))*
            pow(saturate(dot(n,sampleN)),32)*exp(-abs(dot(q-p,n))*80);
        sum+=source.Load(int3(tap,0))*weight;total+=weight;
    }
    return sum/max(total,.0001);
}
float4 PSCopy(Fragment input) : SV_TARGET {return source.SampleLevel(pointClamp,input.uv,0);}
float4 PSComposite(Fragment input) : SV_TARGET {
    int2 pixel=int2(input.position.xy);
    float4 original=source.Load(int3(pixel,0));
    float4 reflected=history.y>.5?reflectionTex.Load(int3(pixel,0)):0;
    if(history.w>.5) {
        float4 ray=rayReflectionTex.Load(int3(pixel,0));
        float coverage=ray.a+reflected.a*(1-ray.a);
        reflected=float4((ray.rgb*ray.a+reflected.rgb*reflected.a*(1-ray.a))/max(coverage,.00001),coverage);
    }
    float depth=depthTex.Load(int3(pixel,0));
    if(depth>=1||depth<=0)return original;
    float4 nr=normalTex.Load(int3(pixel,0));
    float3 p=Reconstruct(input.uv,depthTex.Load(int3(pixel,0)));
    float3 v=normalize(camera.xyz-p), n=normalize(nr.xyz), r=reflect(-v,n);
    float metallic=materialTex.Load(int3(pixel,0)).x/255.0;
    float3 f0=lerp(.04.xxx,baseTex.Load(int3(pixel,0)).rgb,metallic);
    float2 dfg=environmentBrdf.SampleLevel(linearClamp,float2(saturate(dot(n,v)),nr.w),0);
    float3 response=f0*dfg.x+dfg.y;
    float3 baseline=lerp(horizon.rgb,zenith.rgb,saturate(n.y*.5+.5))*horizon.w/3.14159265;
    if(environment.x>.5) {
        float ec=cos(environment.w),es=sin(environment.w);
        r=float3(ec*r.x-es*r.z,r.y,es*r.x+ec*r.z);
        baseline=environmentSpecular.SampleLevel(linearClamp,r,nr.w*environment.z).rgb*environment.y;
    }
    float3 probeBaseline=0;float probeWeight=0;
    [loop]for(uint i=0;i<uint(probeParams.x);++i){uint offset=i*8;if(volumeData[offset+3].w<1.5)continue;
        float3 local=Local(offset,p);if(any(local<0)||any(local>1))continue;
        float3 edge=min(local,1-local)*volumeData[offset+4].xyz;float blend=volumeData[offset+4].w;
        float weight=blend>0?saturate(min(edge.x,min(edge.y,edge.z))/blend):1;float ready;
        float3 radiance=SampleProbe(offset,p,reflect(-v,n),nr.w,ready);probeBaseline+=radiance*weight*ready*volumeData[offset+6].z;probeWeight+=weight*ready;
    }
    baseline=baseline*(1-saturate(probeWeight))+probeBaseline/max(probeWeight,1)*probeParams.z;
    float ao=baseTex.Load(int3(pixel,0)).a*ambientVisibility.Load(int3(pixel,0));
    // Replace the environment's specular estimate at a confident hit. A miss
    // preserves the existing sky/IBL, rather than adding a second environment.
    float3 oldSpecular=min(original.rgb,baseline*ao*response);
    float3 newSpecular=reflected.rgb*response*reflected.a+oldSpecular*zenith.w*(1-reflected.a);
    return float4(max(original.rgb-oldSpecular+newSpecular,0),original.a);
}
