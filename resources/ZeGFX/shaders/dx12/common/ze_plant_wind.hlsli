#ifndef ZE_PLANT_WIND_HLSLI
#define ZE_PLANT_WIND_HLSLI
float ZeWindPulse(float3 p,float4 directionSpeed,float4 parameters,float4 field)
{
    if(field.z<.5) return 1+parameters.y*(.5+.5*sin(dot(p.xz,float2(.045,.031))-parameters.x*parameters.z*6.283185307));
    float along=dot(p.xz,field.xy),across=dot(p.xz,float2(-field.y,field.x));
    float travel=along-parameters.x*max(.5,directionSpeed.w);
    float wavelength=clamp(directionSpeed.w/max(.01,parameters.z),12,120);
    float broad=.5+.5*sin(travel*6.283185307/wavelength+.45*sin(across*.025));
    float local=.5+.5*sin(travel*.17+across*.11-parameters.x*.37);
    float slow=.5+.5*sin(parameters.x*.23+along*.013+across*.019);
    return 1+parameters.y*(2*broad*(.65+.35*local)*(.55+.45*slow)-.65);
}
float3 ZeTreeVertex(float3 p,float weight,float3 root,float height,float4 directionSpeed,
                    float4 parameters,float4 field,float4 geometry,float4 climate,out float slope)
{
    slope=0;
    if(geometry.z<=0||directionSpeed.w<=0||weight<=0||height<=0)return p;
    float t=saturate((root.y-climate.x)/max(.001,climate.y-climate.x));
    float exposure=1+(climate.z-1)*t*t*(3-2*t);
    float phase=root.x*.071+root.z*.053;
    float sway=.55+.25*sin(parameters.x*.85+phase)+.20*sin(parameters.x*1.47+phase*1.73);
    float angle=min(.06,.025*geometry.z*saturate(geometry.w)*(directionSpeed.w/(directionSpeed.w+3))*exposure*ZeWindPulse(root,directionSpeed,parameters,field)*sway);
    float bend=height*weight*weight*angle;
    p.xz+=directionSpeed.xz*bend;p.y-=bend*weight*.08;
    slope=2*weight*angle;
    return p;
}
#endif
