#ifndef ZE_OUTDOOR_FOG_HLSLI
#define ZE_OUTDOOR_FOG_HLSLI
// Analytic Beer-Lambert height fog, renderer Y-up. Integrate the view segment
// after the near clear region; avoid division loss for horizontal rays.
float ZeOutdoorFog(float3 camera, float3 world, float density, float start,
                   float end, float falloff, float baseHeight)
{
    float distanceToSurface=length(world-camera);
    float d=max(0.0,min(distanceToSurface,max(end,start))-start);
    if(d<=0.0 || density<=0.0) return 0.0;
    float startFraction=min(start/max(distanceToSurface,0.001),1.0);
    float y0=lerp(camera.y,world.y,startFraction);
    float y1=lerp(camera.y,world.y,min(end/max(distanceToSurface,0.001),1.0));
    float delta=clamp((y1-y0)*falloff,-12.0,12.0);
    float integral=abs(delta)<0.001 ? 1.0-delta*0.5 : (1.0-exp(-delta))/delta;
    integral*=exp(clamp(-(y0-baseHeight)*falloff,-12.0,12.0));
    float opticalDepth=max(0.0,density*d*min(integral,8.0));
    return saturate(1.0-exp(-opticalDepth));
}
#endif
