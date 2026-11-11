// Reuse the engine's vertex layout, frame/object constants and shadow utilities.
// t0/t1 are opaque HDR colour/depth snapshots in the WorldWater pass.
#define PSMain DreamUnusedPbrPSMain
#include "../materials/pbr_material.hlsl"
#undef PSMain
#include "../common/dreammare_surface.hlsli"

float DreamLinearDepth(float z) {
    if (hasNormalMap>0.5) return lerp(nearPlane,farPlane,z);
    return nearPlane * farPlane / max(farPlane - z * (farPlane - nearPlane), 0.0001);
}
float2 DreamWaveGradient(float2 p) {
    return ZeWaterWaveGradient(p);
}
float DreamWaterShadow(float3 p) {
    uint page = 0;
    if (shadowPageEnabled > 0.5) {
        float distanceToCamera = abs(viewProj[3][3]-1) < 0.001 ? distance(p,cameraPos.xyz)
            : mul(float4(p,1),viewProj).w;
        [unroll] for (uint i=0;i<3;++i)
            if (i+1 < min((uint)shadowPageCount,4u) && distanceToCamera > shadowCascadeSplits[i]) page=i+1;
    }
    float4 lightP = shadowPageEnabled > 0.5 ? mul(float4(p,1),shadowPageViewProj[page]) : mul(float4(p,1),lightViewProj);
    float3 projected=lightP.xyz/max(lightP.w,0.0001);
    float2 localUv=projected.xy*float2(0.5,-0.5)+0.5;
    float4 scaleBias=shadowPageEnabled > 0.5 ? shadowPageScaleBias[page] : float4(1,1,0,0);
    float2 uv=localUv*scaleBias.xy+scaleBias.zw;
    // Compute derivatives before branching, and correct PCF to the receiver plane.
    float2 gradient=ZeShadowReceiverDepthGradient(uv,projected.z);
    if (abs(ddx((float)page))+abs(ddy((float)page))>0) gradient=0;
    if (shadowEnabled < 0.5 || any(localUv<0) || any(localUv>1) || projected.z<0 || projected.z>1) return 1;
    uint w,h;shadowMapTex.GetDimensions(w,h);float2 size=max(float2(w,h),1);
    float visibility=0;
    [unroll] for(int y=0;y<2;++y) [unroll] for(int x=0;x<2;++x) {
        float2 sampleUv=clamp(uv+(float2(x,y)-0.5)/size,scaleBias.zw+1/size,scaleBias.zw+scaleBias.xy-1/size);
        int2 texel=clamp(int2(sampleUv*size),int2(0,0),int2(size)-1);
        float2 texelUv=(float2(texel)+0.5)/size;
        float receiver=ZeShadowReceiverTapDepth(projected.z,gradient,texelUv-uv);
        visibility += receiver-shadowConstantBias <= shadowMapTex.Load(int3(texel,0)).r ? 0.25 : 0;
    }
    return lerp(1,visibility,shadowStrength);
}

float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET {
    const float time=lmPadding1;
    const float river=step(0.75,input.color.a), ocean=1-step(0.25,input.color.a);
    float2 velocity=river > 0.5 ? input.uv : lerp(float2(0.06,0.035),float2(0.32,0.16),ocean);
    // Two overlapping 12.5-second advection phases avoid unbounded stretching
    // where velocity changes around bends. Coordinates/clock are shared by all tiles.
    float phase0=frac(time*0.08), phase1=frac(time*0.08+0.5);
    float weight=abs(phase0*2-1);
    float2 p0=input.worldPos.xz-velocity*(phase0*12.5);
    float2 p1=input.worldPos.xz-velocity*(phase1*12.5);
    float2 gradient=lerp(DreamWaveGradient(p0),DreamWaveGradient(p1),weight);
    float footprint=length(fwidth(input.worldPos.xz));
    float rippleFade=1-smoothstep(0.7,3,footprint);
    float3 N=normalize(float3(-gradient.x*rippleFade,1,-gradient.y*rippleFade));
    float3 V=normalize(cameraPos.xyz-input.worldPos);
    float NdotV=saturate(abs(dot(N,V)));
    float fresnel=0.0204+0.9796*pow(1-NdotV,5);
    float2 pixelUv=input.pos.xy/max(float2(viewportWidth,viewportHeight),1);
    float waterZ=DreamLinearDepth(input.pos.z);
    float cameraDistance=distance(cameraPos.xyz,input.worldPos);
    float depth=2.8;
    float3 bed=float3(0.12,0.21,0.16);
    if (hasBaseColorTexture>0.5) {
        float sceneZ=DreamLinearDepth(normalMapTex.SampleLevel(clampSampler,pixelUv,0).r);
        float rayScale=hasNormalMap>0.5 ? 1 : cameraDistance/max(waterZ,0.01);
        depth=clamp((sceneZ-waterZ)*rayScale,0,80);
        float2 offset=N.xz * min(depth,3) * 0.0014 * rippleFade;
        float2 refractedUv=saturate(pixelUv+offset);
        float refractedZ=DreamLinearDepth(normalMapTex.SampleLevel(clampSampler,refractedUv,0).r);
        // Never pull a foreground bank/actor into the water at a silhouette.
        if (refractedZ<waterZ+0.025) refractedUv=pixelUv;
        bed=baseColorTex.SampleLevel(clampSampler,refractedUv,0).rgb;
    }
    float3 transmittance=exp(-depth*float3(0.30,0.105,0.08));
    float3 deepColor=lerp(float3(0.018,0.14,0.16),float3(0.012,0.085,0.16),ocean);
    if(ocean>.5 && terrainVariationParams.w>.5) deepColor=terrainVariationParams.xyz;
    float3 transmitted=bed*transmittance+deepColor*(1-transmittance)*max(skyIntensity,0.25);
    float3 R=reflect(-V,N);
    float skyBlend=smoothstep(0,0.85,max(R.y,0));
    float3 reflection=lerp(skyHorizonColor.rgb,skyZenithColor.rgb,skyBlend)*skyIntensity;
    if (iblEnabled>0.5) {
        float ec=cos(environmentParams.x),es=sin(environmentParams.x);
        float3 envR=float3(ec*R.x-es*R.z,R.y,es*R.x+ec*R.z);
        reflection=envMapTex.SampleLevel(clampSampler,envR,min(envMapMaxMip,1.4)).rgb*iblIntensity;
    }
    // Bounded screen-space bank reflections; missing/off-screen hits fall back
    // to the sky instead of leaving black streaks. No persistent reflection cache.
    if (hasBaseColorTexture>0.5 && fresnel>0.055 && R.y>0.01) {
        [loop] for(int stepIndex=0;stepIndex<10;++stepIndex) {
            float rayDistance=1.0+stepIndex*stepIndex*1.9;
            float4 clip=ZeProjectCamera(float4(input.worldPos+R*rayDistance,1),viewProj,float2(lightDir.w,lightColor.w));
            if (clip.w<=0 || clip.z<0) break;
            float2 uv=clip.xy/clip.w*float2(0.5,-0.5)+0.5;
            if (any(uv<0.01) || any(uv>0.99)) break;
            float rayDepth=DreamLinearDepth(clip.z/clip.w);
            float sampleDepth=DreamLinearDepth(normalMapTex.SampleLevel(clampSampler,uv,0).r);
            float delta=rayDepth-sampleDepth;
            float hitRange=1.2+rayDistance*0.018;
            if (delta>0 && delta<hitRange && sampleDepth<farPlane*0.995) {
                float edge=smoothstep(0,0.1,min(min(uv.x,1-uv.x),min(uv.y,1-uv.y)));
                float confidence=smoothstep(0,0.2,delta)*(1-smoothstep(hitRange*0.25,hitRange,delta));
                reflection=lerp(reflection,baseColorTex.SampleLevel(clampSampler,uv,0).rgb,edge*confidence*0.2);
                break;
            }
        }
    }
    float3 L=normalize(-lightDir.xyz);
    float3 H=normalize(V+L);
    float shadow=DreamWaterShadow(input.worldPos);
    // Narrow, energy-bounded glints retain the water colour instead of washing
    // whole reaches white. Unresolved ripples broaden and weaken the highlight.
    float sun=pow(saturate(dot(N,H)),lerp(240,600,rippleFade))*lerp(0.025,0.09,rippleFade)*shadow;
    float3 color=lerp(transmitted,reflection,fresnel)+sun*lightColor.rgb;
    float foamNoise=lerp(DreamNoise(p0*0.65),DreamNoise(p1*0.65),weight);
    float foam=(1-smoothstep(0.06,0.40,depth))*smoothstep(0.32,0.68,foamNoise);
    foam*=1-smoothstep(0.5,2.5,footprint);
    color=lerp(color,float3(0.48,0.57,0.51)*max(skyIntensity,0.5),foam*0.25);
    // The copied bed already includes fog. Fog only the reflected/scattered part
    // beyond the configured start, avoiding a second fog application to the bed.
    if (fogEnabled>0.5) {
        float fog=ZeOutdoorFog(cameraPos.xyz,input.worldPos,fogDensity,fogStart,fogEnd,environmentParams.y,environmentParams.z);
        float3 bedContribution=bed*transmittance*(1-fresnel);
        color=bedContribution+lerp(color-bedContribution,fogColor.rgb*(1-transmittance*(1-fresnel)),saturate(fog));
    }
    if (postProcessEnabled<0.5) color=pow(max(color*exposure,0),1/max(gammaValue,0.1));
    return float4(max(color,0),1);
}
