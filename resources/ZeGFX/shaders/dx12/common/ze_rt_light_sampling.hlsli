#ifndef ZE_RT_LIGHT_SAMPLING_HLSLI
#define ZE_RT_LIGHT_SAMPLING_HLSLI

struct GpuDirectionalLight {
    float3 direction;
    float angularSize;
    float3 color;
    float intensity;
};

#endif // ZE_RT_LIGHT_SAMPLING_HLSLI
