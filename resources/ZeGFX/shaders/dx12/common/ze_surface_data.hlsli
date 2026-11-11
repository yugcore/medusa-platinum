#ifndef ZE_SURFACE_DATA_HLSLI
#define ZE_SURFACE_DATA_HLSLI

// SurfaceData defines a backend-neutral conceptual surface record matching ZeGFX Phase 02 Material Contract
struct SurfaceData
{
    float3 baseColor;           // Linear RGB base color
    float3 normalWS;            // Normalized world space normal
    float metallic;             // Clamped metallness in [0, 1]
    float perceptualRoughness;  // Clamped perceptual roughness in [0, 1]
    float ambientOcclusion;     // Ambient occlusion factor in [0, 1], defaults to 1.0
    float3 emissive;            // Linear HDR emissive contribution
    uint shadingModel;          // Shading model ID (e.g., 0 = Unlit, 1 = DefaultLit)
    uint flags;                 // Miscellaneous material flags
};

#endif // ZE_SURFACE_DATA_HLSLI
