// World-space, derivative-filtered detail: continuous across streamed tile borders.
#ifdef DREAMMARE_GRASS_GROUND
#include "stylized_grass_field.hlsli"
#endif
float DreamHash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float DreamNoise(float2 p) {
    float2 i=floor(p),f=frac(p); f=f*f*(3-2*f);
    return lerp(lerp(DreamHash(i),DreamHash(i+float2(1,0)),f.x),
                lerp(DreamHash(i+float2(0,1)),DreamHash(i+1),f.x),f.y);
}
void DreamSurface(float3 p,inout float3 color,inout float3 N,inout float rough) {
    // Generator palette bytes are sRGB; lighting and tone mapping consume linear colour.
    color=pow(saturate(color),2.2);
#ifdef DREAMMARE_GROUND
    float broad=DreamNoise(p.xz*0.035);
    float medium=DreamNoise(p.xz*0.45);
    float nearDetail=1-saturate(length(fwidth(p.xz))*1.4);
    float fine=DreamNoise(p.xz*3.5);
    color*=lerp(0.73,1.07,broad)*lerp(0.88,1.07,medium);
    color*=lerp(1,lerp(0.83,1.08,fine),nearDetail);
    float rock=smoothstep(0.92,0.70,N.y);
    float strata=sin(p.y*0.26+DreamNoise(p.xz*0.08)*3)*0.5+0.5;
    color*=lerp(1,lerp(0.83,1.08,strata),rock);
    rough=0.92;
#ifdef DREAMMARE_GRASS_GROUND
    float4 grass = GrassFields(p.xz);
    // Restrict meadow treatment to green, gentle terrain; shores/rock retain their identity.
    float green = smoothstep(.005,.025,color.g-max(color.r,color.b));
    float meadow = green*smoothstep(.83,.97,N.y);
    float3 turf = pow(lerp(float3(63,105,40),float3(80,120,46),grass.z)/255.,2.2);
    float3 soil = pow(float3(102,91,57)/255.,2.2);
    float grain = lerp(.9,1.07,medium);
    color = lerp(color,lerp(turf,soil,grass.w*.55)*grain,meadow*.8);
#endif
#endif
#ifdef DREAMMARE_WATER
    // Keep geometry and collision stable; normals and soft ripple highlights add water detail.
    float fade=1-saturate(length(fwidth(p.xz))*0.45);
    float ripple=sin(p.x*0.62+p.z*0.27)*sin(p.z*0.44-p.x*0.13);
    N=normalize(N+float3(cos(p.x*0.62+p.z*0.27)*0.065,0,cos(p.z*0.44-p.x*0.13)*0.045)*fade);
    color*=lerp(0.93,1.07,ripple*0.5+0.5);
    rough=0.32;
#endif
}
