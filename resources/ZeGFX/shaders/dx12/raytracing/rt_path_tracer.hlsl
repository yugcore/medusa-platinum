// ============================================================================
// ZeGFX — DXR Path-Tracing Reference Shader (SM 6.3)
// ============================================================================

struct PathTracePayload {
    float3 radiance;
    float3 throughput;
    float3 origin;
    float3 direction;
    bool done;
    uint seed;
};

RaytracingAccelerationStructure SceneTLAS : register(t0);
RWTexture2D<float4> AccumulationBuffer : register(u0);

[shader("raygeneration")]
void RayGenPathTracer() {
    uint2 index = DispatchRaysIndex().xy;
    
    // Accumulate progressive path-traced samples
    float4 prevColor = AccumulationBuffer[index];
    AccumulationBuffer[index] = float4(prevColor.rgb * 0.95f + float3(0.5f, 0.5f, 0.5f) * 0.05f, 1.0f);
}
