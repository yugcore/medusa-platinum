#ifndef ZE_GRASS_WIND_HLSLI
#define ZE_GRASS_WIND_HLSLI
#include "ze_plant_wind.hlsli"
// Used by grass color, depth/normal and conventional shadow passes.
float3 ZeGrassVertex(float3 position, float localY, float2 uv, float3 camera,
                     float distanceEnd, float4 directionSpeed, float4 parameters, float4 field,
                     out float bendSlope)
{
    bendSlope = 0;
    if (parameters.w < .5) return position;
    float height = max(0, localY - uv.x);
    float weight = saturate(uv.y);
    float distanceFromCamera = length(position.xz - camera.xz);
    float strength = max(0, parameters.w - 1);
    float speed = max(0, directionSpeed.w);
    float phase = dot(position.xz, float2(.045, .031)) - parameters.x * parameters.z * 6.283185307;
    float pulse = ZeWindPulse(position,directionSpeed,parameters,field);
    float sway = .7 + .3 * sin(dot(position.xz, float2(.11, .09)) - parameters.x * 1.8);
    float bladeLength = height / max(weight, .001);
    float bend = bladeLength * weight * weight * min(.65, .42 * (speed / (speed + 3)) * strength * pulse * sway);
    position.xz += directionSpeed.xz * bend;
    position.y -= bend * weight * .12;
    bendSlope = 2 * bend / max(height, .001);
    // Same rooted LOD disappearance in every pass, including when wind is calm.
    float fade = 1 - smoothstep(distanceEnd * .68, distanceEnd, distanceFromCamera);
    position.y -= height * (1 - fade);
    return position;
}
#endif
