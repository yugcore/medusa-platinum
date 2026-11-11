#ifndef FROXEL_HLSLI
#define FROXEL_HLSLI

// Convert World Position to Froxel index
uint3 GetFroxelCoord(float3 worldPos, float4x4 ViewMatrix, float NearPlane, float FarPlane, float3 GridDimensions) {
    float3 viewPos = mul(ViewMatrix, float4(worldPos, 1.0f)).xyz;
    float zSlice = log(max(viewPos.z, NearPlane) / NearPlane) / log(FarPlane / NearPlane);
    
    // Project view coords placeholder mapping
    float2 ndc = viewPos.xy / max(viewPos.z, 0.001f);
    float2 screenUV = ndc * 0.5f + 0.5f;

    return uint3(clamp(screenUV * GridDimensions.xy, float2(0, 0), GridDimensions.xy - 1.0f), zSlice * GridDimensions.z);
}

#endif
