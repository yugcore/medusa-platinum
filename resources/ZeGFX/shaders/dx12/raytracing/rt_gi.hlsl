// ============================================================================
// ZeGFX — DXR Ray-Traced GI Shader (SM 6.3)
// ============================================================================

struct GiPayload {
    float3 color;
    float distance;
    int bounces;
};

RaytracingAccelerationStructure SceneTLAS : register(t0);
RWTexture2D<float4> OutIndirectIrradiance : register(u0);

[shader("raygeneration")]
void RayGenGI() {
    uint2 index = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;
    
    // Low-frequency diffuse irradiance estimation
    OutIndirectIrradiance[index] = float4(0.2f, 0.22f, 0.25f, 1.0f);
}
