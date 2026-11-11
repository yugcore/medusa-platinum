#ifndef ZE_CAMERA_PROJECTION_HLSLI
#define ZE_CAMERA_PROJECTION_HLSLI
// Frame lightDir.w / lightColor.w carry the camera's NDC jitter. Apply it
// after projection so world-coordinate rounding cannot change the sample phase.
float4 ZeProjectCamera(float4 worldPosition, float4x4 unjitteredViewProjection, float2 jitter)
{
    float4 clip = mul(worldPosition, unjitteredViewProjection);
    clip.xy += jitter * clip.w;
    return clip;
}
#endif
