#include "../common/ze_brdf.hlsli"
#include "../common/ze_thin_surface.hlsli"
#include "../common/ze_ray_counters.hlsli"
#include "../common/ze_light_types.hlsli"
#include "../common/octahedral.hlsli"
struct RayMaterial {
    uint4 geometry; // vertex/index SRVs, vertex/index byte offsets
    uint4 layout; // stride, 16-bit indices, base texture, emission texture
    float4 baseColor;
    float4 emissionCutoff;
    float4 properties; // metallic, roughness, alpha mode, casts shadows
    uint4 surfaceTextures;
    row_major float4x4 previousWorld;
    uint4 previousGeometry;
    float4 thinSurface;
};
#ifdef ZE_FORWARD_RAYS
cbuffer RayConstants : register(b4) {
#else
cbuffer RayConstants : register(b0) {
#endif
    row_major float4x4 inverseVP;
    row_major float4x4 viewProjection;
    float4 camera;
    float4 worldCamera;
    float4 size;
    float4 jitter;
    float4 sunDirection;
    float4 sunColor;
    float4 horizon;
    float4 zenith;
    float4 options; // distance, normal bias, softness / roughness cutoff, samples
    uint4 metadata; // shadow mode, frame, local light count
    float4 probe; // start, count, atlas width/resolution, probe index
    row_major float4x4 previousVP;
    float4 rayEnvironment;
};
#ifndef ZE_FORWARD_RAYS
Texture2D<float4> sceneColor : register(t0);
Texture2D<float> sceneDepth : register(t1);
Texture2D<float4> sceneNormal : register(t2);
RaytracingAccelerationStructure scene : register(t3);
StructuredBuffer<RayMaterial> materials : register(t4);
StructuredBuffer<GpuLocalLight> lights : register(t5);
StructuredBuffer<float4> probePositions : register(t6);
RWTexture2D<float4> output : register(u0);
#else
RaytracingAccelerationStructure scene : register(t19);
StructuredBuffer<RayMaterial> materials : register(t20);
#define lights localLights
#define materialSampler anisoSampler
#endif
RWByteAddressBuffer rayCounters : register(u1);
Texture2D<float4> textures[] : register(t0,space1);
ByteAddressBuffer geometryBuffers[] : register(t0,space2);
#ifndef ZE_FORWARD_RAYS
SamplerState materialSampler : register(s0);
SamplerState pointSampler : register(s1);
#endif
void CountRay(bool hit) {ZeCountShadingRay(rayCounters,metadata.x,hit);}

uint Index(RayMaterial material,uint index) {
    uint address=material.geometry.w+index*(material.layout.y!=0?2:4);
    uint value=geometryBuffers[NonUniformResourceIndex(material.geometry.y)].Load(address&~3u);
    return material.layout.y!=0?(value>>((address&2)*8))&65535:value;
}
float2 UV(RayMaterial material,uint vertex) {
    uint value=geometryBuffers[NonUniformResourceIndex(material.geometry.x)].Load(material.geometry.z+vertex*material.layout.x+20);
    return float2(f16tof32(value&65535),f16tof32(value>>16));
}
float3 Position(RayMaterial material,uint vertex) {
    return asfloat(geometryBuffers[NonUniformResourceIndex(material.geometry.x)].Load3(material.geometry.z+vertex*material.layout.x));
}
float2 TriangleUV(RayMaterial material,uint primitiveId,float2 bary) {
    return UV(material,Index(material,primitiveId*3))*(1-bary.x-bary.y)+
        UV(material,Index(material,primitiveId*3+1))*bary.x+UV(material,Index(material,primitiveId*3+2))*bary.y;
}
float4 Base(RayMaterial material,float2 uv) {
    float4 color=material.baseColor;
    if(material.layout.z!=0xffffffff) color*=textures[NonUniformResourceIndex(material.layout.z)].SampleLevel(materialSampler,uv,0);
    return color;
}
bool Accept(uint instance,uint primitiveId,float2 bary,bool shadow) {
    RayMaterial material=materials[instance];
    if(shadow&&material.properties.w<.5) return false;
    if(material.properties.z<.5) return true;
    return Base(material,TriangleUV(material,primitiveId,bary)).a>=material.emissionCutoff.w;
}
bool Occluded(float3 origin,float3 direction,float maxDistance) {
    RayDesc ray;ray.Origin=origin;ray.Direction=direction;ray.TMin=.001;ray.TMax=maxDistance;
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(scene,RAY_FLAG_NONE,255,ray);
    while(q.Proceed()) if(q.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE &&
        Accept(q.CandidateInstanceID(),q.CandidatePrimitiveIndex(),q.CandidateTriangleBarycentrics(),true)) q.CommitNonOpaqueTriangleHit();
    bool hit=q.CommittedStatus()==COMMITTED_TRIANGLE_HIT;CountRay(hit);return hit;
}
float Random(inout uint state) {state=state*747796405u+2891336453u;uint w=((state>>((state>>28)+4))^state)*277803737u;return float((w>>22)^w)/4294967296.0;}
float3 Cone(float3 direction,float radius,inout uint state) {
    float angle=Random(state)*6.2831853,r=sqrt(Random(state))*radius;
    float3 tangent=normalize(cross(abs(direction.y)>.99?float3(0,0,1):float3(0,1,0),direction));
    return normalize(direction+tangent*(cos(angle)*r)+cross(direction,tangent)*(sin(angle)*r));
}
float3 Reconstruct(float2 uv,float depth) {
    float4 p=mul(float4(uv*float2(2,-2)+float2(-1,1)-jitter.xy,depth,1),inverseVP);
    return p.xyz/p.w;
}
float3 ShadeHit(RayMaterial material,float2 uv,float3 position,float3 n,float3 view) {
    float4 base=Base(material,uv);
    if(material.surfaceTextures.y!=0xffffffff) {
        float4 orm=textures[NonUniformResourceIndex(material.surfaceTextures.y)].SampleLevel(materialSampler,uv,0);
        material.properties.xy*=float2(orm.b,orm.g);
    }
    float3 emission=material.emissionCutoff.rgb;
    if(material.layout.w!=0xffffffff) emission*=textures[NonUniformResourceIndex(material.layout.w)].SampleLevel(materialSampler,uv,0).rgb;
    float3 ambient=lerp(horizon.rgb,zenith.rgb,saturate(n.y*.5+.5))*horizon.w*base.rgb*(1-material.properties.x)/3.14159265;
    if(material.surfaceTextures.z!=0xffffffff) ambient*=textures[NonUniformResourceIndex(material.surfaceTextures.z)].SampleLevel(materialSampler,uv,0).r;
    BrdfInput input;input.N=n;input.V=view;input.baseColor=base.rgb;input.metallic=material.properties.x;input.perceptualRoughness=material.properties.y;
    input.L=normalize(-sunDirection.xyz);
    float2 thin=material.thinSurface.w<-.5?ZeThinQuantize(material.thinSurface.xy):0;
    BrdfResult brdf=EvaluateThinSurfaceBrdf(input,thin);
    float3 sunlight=(brdf.diffuse+brdf.specular)*sunColor.rgb*sunColor.a;
    float3 color=ambient+emission;
    if(any(sunlight>0))color+=sunlight*(Occluded(position+n*max(options.y,.002),input.L,options.x)?0:1);
    for(uint i=0;i<metadata.z;++i) {
        GpuLocalLight light=lights[i];float3 d=(light.positionRange.xyz-worldCamera.xyz)-position;
        float distance=length(d);if(distance>=light.positionRange.w||distance<.001)continue;
        input.L=d/distance;
        float cutoff=saturate(1-pow(distance/light.positionRange.w,4));float attenuation=cutoff*cutoff/max(distance*distance,.01);
        if (light.directionType.w > 1.5)
            attenuation *= pow(saturate((dot(-input.L, normalize(light.directionType.xyz)) -
                                         light.coneRectFlags.y) /
                                        max(light.coneRectFlags.x - light.coneRectFlags.y, .0001)),
                               2);
        if(attenuation<=0)continue;
        brdf=EvaluateThinSurfaceBrdf(input,thin);float3 contribution=(brdf.diffuse+brdf.specular)*light.colorIntensity.rgb*light.colorIntensity.a*attenuation;
        if(!any(contribution>0))continue;
        if(light.coneRectFlags.z>.5&&Occluded(position+n*max(options.y,.002),input.L,distance-.002))continue;
        color+=contribution;
    }
    return max(color,0);
}
float2 SnormPair(uint word) {return max(float2(int(word<<16)>>16,int(word)>>16)/32767.0,-1);}
float3 Sky(float3 d) {
    float3 gradient=lerp(horizon.rgb,zenith.rgb,saturate(d.y*.5+.5))*horizon.w;
    if(rayEnvironment.x<0)return gradient;
    float c=cos(rayEnvironment.y),s=sin(rayEnvironment.y);d=float3(c*d.x-s*d.z,d.y,s*d.x+c*d.z);
    float2 uv=float2(frac(atan2(d.z,d.x)/6.28318530718+.5),acos(clamp(d.y,-1,1))/3.14159265);
    float3 panorama=textures[NonUniformResourceIndex(uint(rayEnvironment.x))].SampleLevel(materialSampler,uv,0).rgb*rayEnvironment.z;
    return lerp(panorama,gradient,saturate(rayEnvironment.w)*(1-saturate(abs(d.y)*4)));
}
float3 Radiance(float3 origin,float3 direction,inout uint seed,out bool firstHit,out float firstDistance) {
    float3 color=0,throughput=1;firstHit=false;firstDistance=options.x;
    [loop] for(uint bounce=0;bounce<max(metadata.w,1);++bounce) {
        RayDesc ray;ray.Origin=origin;ray.Direction=direction;ray.TMin=.001;ray.TMax=options.x;
        RayQuery<RAY_FLAG_NONE> q;q.TraceRayInline(scene,RAY_FLAG_NONE,255,ray);
        while (q.Proceed())
            if (q.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE &&
                Accept(q.CandidateInstanceID(),
                       q.CandidatePrimitiveIndex(),
                       q.CandidateTriangleBarycentrics(),
                       false))
                q.CommitNonOpaqueTriangleHit();
        bool hit=q.CommittedStatus()==COMMITTED_TRIANGLE_HIT;CountRay(hit);
        if(bounce==0){firstHit=hit;if(hit)firstDistance=q.CommittedRayT();}
        if(!hit){color+=throughput*Sky(direction);break;}
        RayMaterial material=materials[q.CommittedInstanceID()];uint primitive=q.CommittedPrimitiveIndex();float2 bary=q.CommittedTriangleBarycentrics();
        float3 weights=float3(1-bary.x-bary.y,bary);
        uint3 vertices=uint3(Index(material,primitive*3),Index(material,primitive*3+1),Index(material,primitive*3+2));
        float3 localN=0,localT=0;float4 vertexColor=0;float handedness=1;
        [unroll]for(uint i=0;i<3;++i){
            uint address=material.geometry.z+vertices[i]*material.layout.x;
            uint normal=geometryBuffers[NonUniformResourceIndex(material.geometry.x)].Load(address+12);
            uint tangent=geometryBuffers[NonUniformResourceIndex(material.geometry.x)].Load(address+16);
            float4 t = DecodeOctahedralTangentSNORM(SnormPair(tangent));
            localN += DecodeOctahedralSNORM(SnormPair(normal)) * weights[i];
            localT += t.xyz * weights[i];
            handedness = t.w;
            uint c=geometryBuffers[NonUniformResourceIndex(material.geometry.x)].Load(address+28);
            vertexColor+=float4(c&255,(c>>8)&255,(c>>16)&255,c>>24)/255.0*weights[i];
        }
        material.baseColor*=vertexColor;
        float3 n=normalize(mul(localN,(float3x3)q.CommittedWorldToObject3x4()));
        float3 tangent=normalize(mul((float3x3)q.CommittedObjectToWorld3x4(),localT));tangent=normalize(tangent-n*dot(n,tangent));
        if(dot(n,direction)>0)n=-n;
        float2 uv=TriangleUV(material,primitive,bary);
        if (material.surfaceTextures.x != 0xffffffff) {
            float3 map = textures[NonUniformResourceIndex(material.surfaceTextures.x)]
                                 .SampleLevel(materialSampler, uv, 0)
                                 .xyz *
                             2 -
                         1;
            n = normalize(tangent * map.x + cross(n, tangent) * handedness * map.y + n * map.z);
        }
        if(material.thinSurface.w<-.5)n=ZeThinNormal(n,material.thinSurface.z);
        float3 position=origin+direction*q.CommittedRayT();
        color+=throughput*ShadeHit(material,uv,position,n,-direction);
        if(bounce+1>=metadata.w)break;
        float2 mr=material.properties.xy;
        if (material.surfaceTextures.y != 0xffffffff) {
            float4 tex = textures[NonUniformResourceIndex(material.surfaceTextures.y)].SampleLevel(
                materialSampler, uv, 0);
            mr *= float2(tex.b, tex.g);
        }
        float3 f0=lerp(.04.xxx,Base(material,uv).rgb,mr.x);throughput*=f0+(1-f0)*pow(1-saturate(dot(-direction,n)),5);
        if(max(throughput.r,max(throughput.g,throughput.b))<.001)break;
        direction=Cone(reflect(direction,n),mr.y*mr.y,seed);if(dot(direction,n)<=0)break;origin=position+n*max(options.y,.002);
    }
    return max(color,0);
}
#ifndef ZE_FORWARD_RAYS
[numthreads(64,1,1)]
void CSGi(uint3 id:SV_DispatchThreadID) {
    if(id.x>=uint(probe.y))return;uint index=uint(probe.x)+id.x;
    float3 origin=probePositions[index].xyz;uint seed=index+metadata.y*104729;
    float3 irradiance[6];float distances[6],squared[6],weights[6];
    [unroll]for(uint k=0;k<6;++k){irradiance[k]=0;distances[k]=squared[k]=weights[k]=0;}
    [loop]for(uint sample=0;sample<uint(options.w);++sample){
        float y=1-2*(sample+.5)/options.w,phi=sample*2.39996323+Random(seed)*.05;
        float3 direction=float3(cos(phi)*sqrt(1-y*y),y,sin(phi)*sqrt(1-y*y));bool hit;float distance;
        float3 radiance=Radiance(origin,direction,seed,hit,distance);
        if(!hit)radiance=0;
        distance*=.01;
        [unroll] for (uint k = 0; k < 6; ++k) {
            float weight = max((k % 2 == 0 ? 1 : -1) * direction[k / 2], 0);
            irradiance[k] += radiance * weight;
            distances[k] += distance * weight;
            squared[k] += distance * distance * weight;
            weights[k] += weight;
        }
    }
    uint2 pixel=uint2(index%512,(index/512)*8);
    [unroll]for(uint k=0;k<6;++k)output[pixel+uint2(0,k)]=float4(irradiance[k]*(4/options.w),distances[k]/max(weights[k],.0001));
    output[pixel+uint2(0,6)]=float4(squared[0]/weights[0],squared[1]/weights[1],squared[2]/weights[2],squared[3]/weights[3]);
    output[pixel+uint2(0,7)]=float4(squared[4]/weights[4],squared[5]/weights[5],1,0);
}
[numthreads(8,8,1)]
void CSProbe(uint3 id:SV_DispatchThreadID) {
    uint resolution=uint(probe.z);if(any(id.xy>=resolution))return;
    float3 direction=DecodeOctahedral((id.xy+.5)/resolution);bool hit;float distance;uint seed=id.x+id.y*resolution+metadata.y*104729;
    float3 color=Radiance(probePositions[uint(probe.x)].xyz,direction,seed,hit,distance);
    output[id.xy+uint2(0,resolution*uint(probe.w))]=float4(color,1);
}
[numthreads(8,8,1)]
void CSClear(uint3 id:SV_DispatchThreadID){if(all(id.xy<uint2(size.xy)))output[id.xy]=0;}
[numthreads(8,8,1)]
void CSMotion(uint3 id:SV_DispatchThreadID){
    if(any(id.xy>=uint2(size.xy)))return;output[id.xy]=0;float z=sceneDepth.Load(int3(id.xy,0));if(z<=0||z>=1)return;
    float2 uv=(id.xy+.5)*size.zw;float3 p=Reconstruct(uv,z),direction=normalize(p);
    RayDesc ray;ray.Origin=0;ray.Direction=direction;ray.TMin=.001;ray.TMax=length(p)+max(.03,length(p)*.002);
    RayQuery<RAY_FLAG_NONE> q;q.TraceRayInline(scene,RAY_FLAG_NONE,255,ray);
    while (q.Proceed())
        if (q.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE &&
            Accept(q.CandidateInstanceID(),
                   q.CandidatePrimitiveIndex(),
                   q.CandidateTriangleBarycentrics(),
                   false))
            q.CommitNonOpaqueTriangleHit();
    if(q.CommittedStatus()!=COMMITTED_TRIANGLE_HIT||abs(q.CommittedRayT()-length(p))>max(.03,length(p)*.002))return;
    RayMaterial material=materials[q.CommittedInstanceID()];if(material.previousGeometry.z==0)return;
    uint primitive=q.CommittedPrimitiveIndex();float2 bary=q.CommittedTriangleBarycentrics();float3 weights=float3(1-bary.x-bary.y,bary),previousPosition=0;
    [unroll] for (uint i = 0; i < 3; ++i) {
        uint vertex = Index(material, primitive * 3 + i);
        previousPosition +=
            asfloat(geometryBuffers[NonUniformResourceIndex(material.previousGeometry.x)].Load3(
                material.previousGeometry.y + vertex * material.layout.x)) *
            weights[i];
    }
    float4 old=mul(mul(float4(previousPosition,1),material.previousWorld),previousVP);if(old.w<=0)return;
    float2 oldUv=(old.xy/old.w+jitter.zw)*float2(.5,-.5)+.5;
    output[id.xy]=float4(oldUv-uv,old.z/old.w,1);
}
[numthreads(8,8,1)]
void CSMain(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=uint2(size.xy)))return;
    float2 uv=(id.xy+.5)*size.zw;float depth=sceneDepth.Load(int3(id.xy,0));
    if(depth>=1||depth<=0){output[id.xy]=metadata.x!=0?1:0;return;}
    float4 nr=sceneNormal.Load(int3(id.xy,0));float3 n=normalize(nr.xyz),p=Reconstruct(uv,depth);
    uint seed=id.x+id.y*(uint)size.x+metadata.y*104729;
    uint samples=(uint)options.w;
    if(probe.w>.5&&samples>1) {
        float4 motion=textures[uint(probe.z)].Load(int3(id.xy,0));float2 oldUv=uv+motion.xy;
        if(motion.w>.5&&all(oldUv>size.zw*.5)&&all(oldUv<1-size.zw*.5)) {
            int2 oldPixel=int2(oldUv*size.xy);float oldDepth=textures[uint(probe.x)].Load(int3(oldPixel,0)).r;
            float3 oldNormal=normalize(textures[uint(probe.y)].Load(int3(oldPixel,0)).xyz);
            if(oldDepth>0&&oldDepth<1&&abs(oldDepth-motion.z)<2e-5&&dot(n,oldNormal)>.95)samples=1;
        }
    }
    if(metadata.x!=0) {
        float visibility=0;
        for(uint i=0;i<samples;++i) visibility+=Occluded(p+n*options.y,Cone(normalize(-sunDirection.xyz),options.z,seed),options.x)?0:1;
        output[id.xy]=(visibility/max(samples,1)).xxxx;return;
    }
    if(nr.w>options.z){output[id.xy]=0;return;}
    float3 v=normalize(camera.xyz-p),sum=0;float hits=0;
    for(uint sample=0;sample<samples;++sample) {
        float2 xi=float2(Random(seed),Random(seed));float a=max(.001,nr.w*nr.w);
        float phi=6.2831853*xi.x,cosTheta=sqrt((1-xi.y)/(1+(a*a-1)*xi.y));
        float3 tangent=normalize(cross(abs(n.y)>.99?float3(0,0,1):float3(0,1,0),n));
        float3 halfVector=normalize(tangent*(cos(phi)*sqrt(1-cosTheta*cosTheta))+cross(n,tangent)*(sin(phi)*sqrt(1-cosTheta*cosTheta))+n*cosTheta);
        float3 direction=reflect(-v,halfVector);if(dot(direction,n)<=0)continue;
        bool hit;float distance;float3 radiance=Radiance(p+n*max(options.y,.002),direction,seed,hit,distance);
        if(hit){sum+=radiance;hits+=1;}
    }
    // Misses preserve the existing environment contribution in the compositor.
    output[id.xy]=float4(sum/max(hits,1),hits/max(samples,1));
}
#endif
