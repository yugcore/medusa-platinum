#ifndef ZE_WATER_SURFACE_HLSLI
#define ZE_WATER_SURFACE_HLSLI
// Shared ocean/river ripple normals, continuous in world space and streamed time.
float ZeWaterHash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float ZeWaterNoise(float2 p) {
    float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
    return lerp(lerp(ZeWaterHash(i),ZeWaterHash(i+float2(1,0)),f.x),
                lerp(ZeWaterHash(i+float2(0,1)),ZeWaterHash(i+1),f.x),f.y);
}
float2 ZeWaterWaveGradient(float2 p) {
    float bend=ZeWaterNoise(p*.18)*4;
    return float2(cos(dot(p,float2(.85,.22))+bend)*.018+cos(dot(p,float2(-.31,1.12))-bend)*.009,
                  cos(dot(p,float2(.85,.22))+bend)*.006+cos(dot(p,float2(-.31,1.12))-bend)*.026);
}
float2 ZeWaterAdvectedGradient(float2 p,float2 velocity,float time) {
    float phase0=frac(time*.08),phase1=frac(time*.08+.5);
    return lerp(ZeWaterWaveGradient(p-velocity*(phase0*12.5)),
                ZeWaterWaveGradient(p-velocity*(phase1*12.5)),abs(phase0*2-1));
}
#endif
