// ZeGFX procedural water. Adapted from the MIT tuxalin water shader; licence
// and original source are retained in third_party/water-shader and the runtime
// textures/water/LICENSE.txt. No Unity GrabPass or game-specific constants.
#define PSMain ZeWaterUnusedPbrPSMain
#define VSMain ZeWaterPackedVSMain
#include "../materials/pbr_material.hlsl"
#undef VSMain
#undef PSMain
#include "../common/ze_water_optics.hlsli"

#define waterShallow waterSurfaceParams[0]
#define waterDeep waterSurfaceParams[1]
#define waterAbsorption waterSurfaceParams[2]
#define waterWaves waterSurfaceParams[3]
#define waterDynamics waterSurfaceParams[4]
#define waterFoam waterSurfaceParams[5]
#define waterShore waterSurfaceParams[6]
#define waterOptions waterSurfaceParams[7]

PSInput VSMain(VSInput input) {
    PSInput output=ZeWaterPackedVSMain(input);
    if(waterDynamics.y>0) {
        float3 displacement=ZeWaterDisplacement(output.worldPos.xz,lmPadding1*waterDynamics.x,waterWaves.zw,waterWaves.y,waterOptions.w);
        // UV1 carries final bed depth when supplied by a procedural water mesh.
        // Keep shores fixed. Default displacement is zero, preserving the exact
        // generated surface/query contract; opt-in waves require a dense mesh.
        float shore=input.uv1.y>.5?smoothstep(0,max(.5,waterShore.x*2),input.uv1.x):1;
        output.worldPos+=displacement*waterDynamics.y*shore;
        output.pos=ZeProjectCamera(float4(output.worldPos,1),viewProj,float2(lightDir.w,lightColor.w));
        output.clipPos=output.pos;
    }
    return output;
}
float ZeWaterLinearDepth(float z) {
    return hasNormalMap>.5?lerp(nearPlane,farPlane,z):
        nearPlane*farPlane/max(farPlane-z*(farPlane-nearPlane),.0001);
}
float ZeWaterSunShadow(float3 p) {
    uint page=0;
    if(shadowPageEnabled>.5) {
        float cameraDepth=abs(viewProj[3][3]-1)<.001?distance(p,cameraPos.xyz):mul(float4(p,1),viewProj).w;
        [unroll]for(uint i=0;i<3;++i)if(i+1<min((uint)shadowPageCount,4u)&&cameraDepth>shadowCascadeSplits[i])page=i+1;
    }
    float4 clip=shadowPageEnabled>.5?mul(float4(p,1),shadowPageViewProj[page]):mul(float4(p,1),lightViewProj);
    float3 projected=clip.xyz/max(abs(clip.w),.0001);
    float2 local=projected.xy*float2(.5,-.5)+.5;
    float4 atlas=shadowPageEnabled>.5?shadowPageScaleBias[page]:float4(1,1,0,0);
    float2 uv=local*atlas.xy+atlas.zw;
    float2 receiverGradient=ZeShadowReceiverDepthGradient(uv,projected.z);
    if(abs(ddx((float)page))+abs(ddy((float)page))>0)receiverGradient=0;
    if(shadowEnabled<.5||any(local<0)||any(local>1)||projected.z<0||projected.z>1)return 1;
    uint width,height;shadowMapTex.GetDimensions(width,height);float2 size=max(float2(width,height),1);
    float visibility=0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x) {
        float2 sampleUv=clamp(uv+(float2(x,y)-.5)/size,atlas.zw+1/size,atlas.zw+atlas.xy-1/size);
        int2 texel=clamp(int2(sampleUv*size),int2(0,0),int2(size)-1);
        float receiver=ZeShadowReceiverTapDepth(projected.z,receiverGradient,(float2(texel)+.5)/size-uv);
        visibility+=(receiver-shadowConstantBias<=shadowMapTex.Load(int3(texel,0)).r)? .25:0;
    }
    return lerp(1,visibility,shadowStrength);
}
float3 ZeWaterTextureNormal(float2 worldPosition,float2 flow,float time) {
    float phase0=frac(time*.08),phase1=frac(time*.08+.5),blend=abs(phase0*2-1);
    float2 a=worldPosition-flow*(phase0*12.5),b=worldPosition-flow*(phase1*12.5);
    float3 detail=0;
    const float3 scales=float3(1.6,.8,.3),weights=float3(.50,.32,.18);
    [unroll]for(int i=0;i<3;++i) {
        float2 uv0=a*(waterWaves.y*waterOptions.z*scales[i]);
        float2 uv1=b*(waterWaves.y*waterOptions.z*scales[i]);
        if(i==1){uv0=float2(-uv0.y,uv0.x)+.37;uv1=float2(-uv1.y,uv1.x)+.37;}
        float3 n=lerp(metallicRoughnessTex.Sample(baseSampler,uv0).xyz,
                      metallicRoughnessTex.Sample(baseSampler,uv1).xyz,blend)*2-1;
        if(i==1)n.xy=float2(n.y,-n.x);
        detail+=n*weights[i];
    }
    return normalize(float3(detail.x,max(.25,detail.z),detail.y));
}
float4 PSMain(PSInput input,bool frontFace:SV_IsFrontFace):SV_TARGET {
    float time=lmPadding1*waterDynamics.x,river=step(.75,input.color.a),ocean=1-step(.25,input.color.a);
    float2 wind=waterWaves.zw;
    float2 flow=river>.5?input.uv:wind*lerp(.10,.24,ocean);
    float footprint=length(fwidth(input.worldPos.xz));
    float height;float2 gradient;
    ZeWaterWaves(input.worldPos.xz,time,waterWaves.zw,waterWaves.y,footprint,height,gradient);
    float detailFade=1-smoothstep(.8,5,footprint);
    float3 N=normalize(float3(-gradient.x*waterWaves.x*3,1,-gradient.y*waterWaves.x*3));
    if(((uint)waterOptions.x&1u)!=0) {
        float3 detail=ZeWaterTextureNormal(input.worldPos.xz,flow,time);
        N=normalize(float3(N.x+detail.x*waterWaves.x*detailFade,N.y,N.z+detail.z*waterWaves.x*detailFade));
    }
    float3 ray=normalize(input.worldPos-cameraPos.xyz);
    if(hasNormalMap>.5)ray=normalize(float3(viewProj[0][2],viewProj[1][2],viewProj[2][2]));
    float3 V=-ray;
    // Render both faces without allowing negative Fresnel/NaN normals underwater.
    if(dot(N,V)<0)N=-N;
    float fresnel=ZeWaterFresnel(abs(dot(N,V)))*saturate(waterAbsorption.w);
    float2 pixelUv=input.pos.xy/max(float2(viewportWidth,viewportHeight),1);
    float waterZ=ZeWaterLinearDepth(input.pos.z),cameraDistance=distance(cameraPos.xyz,input.worldPos);
    float rayDepth=2.8,depth=input.uv1.y>.5?input.uv1.x:2.8;
    float3 bed=float3(.12,.21,.16);
    if(hasBaseColorTexture>.5) {
        float sceneZ=ZeWaterLinearDepth(normalMapTex.SampleLevel(clampSampler,pixelUv,0).r);
        float rayScale=hasNormalMap>.5?1:cameraDistance/max(waterZ,.01);
        rayDepth=clamp((sceneZ-waterZ)*rayScale,0,80);
        depth=rayDepth*max(abs(ray.y),.01);
        float2 offset=N.xz*min(depth,3)*waterShore.z*detailFade*smoothstep(0,max(.01,waterShore.y),depth);
        float2 refractedUv=clamp(pixelUv+offset,.001,.999);
        float offsetZ=ZeWaterLinearDepth(normalMapTex.SampleLevel(clampSampler,refractedUv,0).r);
        if(offsetZ<waterZ+.025)refractedUv=pixelUv;
        bed=baseColorTex.SampleLevel(clampSampler,refractedUv,0).rgb;
    }
    float surfaceFade=smoothstep(0,max(.01,waterShore.y),depth);
    fresnel*=surfaceFade;
    float3 L=normalize(-lightDir.xyz);
    float shadow=ZeWaterSunShadow(input.worldPos);
    // A bounded, animated shallow-bed light pattern, an addition to the source.
    // It is not an underwater volume or a claim of physically traced caustics.
    float2 bedPoint=input.worldPos.xz+ray.xz*rayDepth;
    float a=sin(bedPoint.x*1.7+sin(bedPoint.y*1.1+time*.45));
    float b=sin(bedPoint.y*1.9+sin(bedPoint.x*.8-time*.33));
    float caustic=pow(saturate(1-abs(a+b)*.7),12);
    caustic*=waterShore.w*exp(-depth*.35)*smoothstep(0,.15,depth)*detailFade*shadow*saturate(L.y);
    bed*=1+caustic;
    float3 T=ZeWaterTransmission(depth,rayDepth,waterAbsorption.xyz,waterShallow.w,waterDeep.w);
    float3 deepColor=waterDeep.rgb;
    if(ocean>.5&&terrainVariationParams.w>.5)deepColor=terrainVariationParams.rgb;
    float3 tint=lerp(waterShallow.rgb,deepColor,1-exp(-depth*.24));
    float3 transmitted=bed*T+tint*(1-T)*max(skyIntensity,.25);
    float3 R=reflect(-V,N);
    float3 reflection=lerp(skyHorizonColor.rgb,skyZenithColor.rgb,smoothstep(0,.85,max(R.y,0)))*skyIntensity;
    float variance=max(dot(ddx(N),ddx(N)),dot(ddy(N),ddy(N)));
    float roughness=clamp(sqrt(waterDynamics.w*waterDynamics.w+variance*2),.04,.7);
    if(iblEnabled>.5) {
        float c=cos(environmentParams.x),s=sin(environmentParams.x);
        float3 envR=float3(c*R.x-s*R.z,R.y,s*R.x+c*R.z);
        reflection=envMapTex.SampleLevel(clampSampler,envR,min(envMapMaxMip,roughness*envMapMaxMip)).rgb*iblIntensity;
    }
    // Bounded screen-space reflection, with sky/prefiltered environment fallback.
    // Sample only the opaque copy; no read/write feedback or water self-reflection.
    int steps=clamp((int)waterOptions.y,0,24);
    if(hasBaseColorTexture>.5&&fresnel>.045&&R.y>.005&&steps>0) {
        [loop]for(int i=0;i<steps;++i) {
            float distanceAlong=.8+i*(1.2+i*.28);
            float4 clip=ZeProjectCamera(float4(input.worldPos+R*distanceAlong,1),viewProj,float2(lightDir.w,lightColor.w));
            if(clip.w<=0||clip.z<0)break;
            float2 uv=clip.xy/clip.w*float2(.5,-.5)+.5;
            if(any(uv<.01)||any(uv>.99))break;
            float rayZ=ZeWaterLinearDepth(clip.z/clip.w),sceneZ=ZeWaterLinearDepth(normalMapTex.SampleLevel(clampSampler,uv,0).r);
            float delta=rayZ-sceneZ,range=.8+distanceAlong*.025;
            if(delta>0&&delta<range&&sceneZ<farPlane*.995) {
                float edge=smoothstep(0,.10,min(min(uv.x,1-uv.x),min(uv.y,1-uv.y)));
                float confidence=edge*(1-smoothstep(range*.3,range,delta));
                reflection=lerp(reflection,baseColorTex.SampleLevel(clampSampler,uv,0).rgb,confidence*.55);
                break;
            }
        }
    }
    float specular=ZeWaterSunSpecular(N,V,L,roughness)*shadow*surfaceFade;
    float3 color=lerp(transmitted,reflection,fresnel)+specular*lightColor.rgb;
    // Two independent advected foam layers, washed shore edge and sparse crests.
    float2 foamUv=input.worldPos.xz*.32-flow*time*.06;
    float pattern=ZeWaterNoise(foamUv*3);
    if(((uint)waterOptions.x&2u)!=0)pattern=emissiveTex.Sample(baseSampler,foamUv).r;
    float shorePattern=ZeWaterNoise(foamUv*1.8+19);
    if(((uint)waterOptions.x&4u)!=0)shorePattern=occlusionTex.Sample(baseSampler,foamUv*.55+time*.007).r;
    float shore=1-smoothstep(.015,max(.02,waterShore.x),depth);
    float pulse=.55+.45*sin(time*1.1+depth*6+ZeWaterNoise(input.worldPos.xz*.17)*2);
    float foam=shore*smoothstep(.2,.8,lerp(pattern,shorePattern,.6))*lerp(.65,1,pulse);
    foam+=smoothstep(.78,.98,height)*waterDynamics.z*ocean*(1-shore)*pattern*.25;
    foam*=waterFoam.w*surfaceFade*(1-smoothstep(.8,5,footprint));
    float3 foamLight=waterFoam.rgb*(max(.25,skyIntensity)+lightColor.rgb*saturate(L.y)*shadow*.18);
    color=lerp(color,foamLight,saturate(foam));
    // The copied bed has already received fog. Fog only newly scattered and
    // reflected radiance, preserving shallow clarity without double fogging.
    if(fogEnabled>.5) {
        float fog=ZeOutdoorFog(cameraPos.xyz,input.worldPos,fogDensity,fogStart,fogEnd,environmentParams.y,environmentParams.z);
        float3 bedPart=bed*T*(1-fresnel)*(1-saturate(foam));
        color=bedPart+lerp(color-bedPart,fogColor.rgb*(1-T*(1-fresnel)*(1-saturate(foam))),saturate(fog));
    }
    if(postProcessEnabled<.5)color=pow(max(color*exposure,0),1/max(gammaValue,.1));
    return float4(max(color,0),1);
}
