#include "surface_effects.hlsl"
float4 PSProbeLighting(Fragment input):SV_TARGET {
    int2 pixel = int2(input.position.xy);
    float4 original = source.Load(int3(pixel, 0));
    float depth = depthTex.Load(int3(pixel, 0));
    if (depth <= 0 || depth >= 1)
        return original;
    float3 p = Reconstruct(input.uv, depth);
    float4 nr = normalTex.Load(int3(pixel, 0));
    float3 normal = normalize(nr.xyz), view = normalize(camera.xyz - p), r = reflect(-view, normal);
    float3 gi=0,reflection=0;float giWeight=0,reflectionWeight=0;
    [loop]for(uint i=0;i<uint(trace.x);++i){uint offset=i*8;float3 local=Local(offset,p);if(any(local<0)||any(local>1))continue;
        float3 edge = min(local, 1 - local) * volumeData[offset + 4].xyz;
        float blend = volumeData[offset + 4].w;
        float weight = blend > 0 ? saturate(min(edge.x, min(edge.y, edge.z)) / blend) : 1;
        if(volumeData[offset+3].w<1.5){gi+=SampleGi(offset,local,p,normal)*weight*volumeData[offset+6].z;giWeight+=weight;}
        else {float ready;float3 value=SampleProbe(offset,p,r,nr.w,ready);reflection+=value*weight*ready*volumeData[offset+6].z;reflectionWeight+=weight*ready;}
    }
    float3 base=baseTex.Load(int3(pixel,0)).rgb;float metal=materialTex.Load(int3(pixel,0)).x/255.0;float3 f0=lerp(.04.xxx,base,metal);
    float2 dfg=environmentBrdf.SampleLevel(linearClamp,float2(saturate(dot(normal,view)),nr.w),0);float3 response=saturate(f0*dfg.x+dfg.y);
    float3 color=original.rgb+gi/max(giWeight,1)*base*(1-metal)*(1-response)*trace.y;
    if(reflectionWeight>0){float3 baseline=lerp(horizon.rgb,zenith.rgb,saturate(normal.y*.5+.5))*horizon.w/3.14159265;
        if (environment.x > .5) {
            float c = cos(environment.w), s = sin(environment.w);
            float3 d = float3(c * r.x - s * r.z, r.y, s * r.x + c * r.z);
            baseline = environmentSpecular.SampleLevel(linearClamp, d, nr.w * environment.z).rgb *
                       environment.y;
        }
        float weight=saturate(reflectionWeight);color-=min(color,baseline*response)*weight;color+=reflection/max(reflectionWeight,1)*response*trace.z;
    }return float4(max(color,0),original.a);
}
