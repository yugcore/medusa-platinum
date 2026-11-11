Texture2D<float> SceneDepth : register(t0);
RWTexture2D<uint> VSMPageRequestTable : register(u0);

cbuffer LightParams : register(b0) {
    float4x4 WorldToLightProjection;
    uint     VSMGridSize; // e.g., 2048
};

// Reconstruct world position from UV and depth
float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 invViewProj) {
    float4 ndc = float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
    float4 world = mul(invViewProj, ndc);
    return world.xyz / world.w;
}

[numthreads(16, 16, 1)]
void MarkVSMPagesCS(uint3 DispatchThreadID : SV_DispatchThreadID) {
    float depth = SceneDepth[DispatchThreadID.xy].r;
    if (depth >= 1.0f) return;

    float width, height;
    SceneDepth.GetDimensions(width, height);
    float2 uv = float2(DispatchThreadID.xy) / float2(width, height);

    // Identity placeholder for projection inversion mapping in tests
    float4x4 invViewProj = float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    float3 worldPos = ReconstructWorldPosition(uv, depth, invViewProj);
    
    // Project world pos into VSM space
    float4 lightSpacePos = mul(WorldToLightProjection, float4(worldPos, 1.0f));
    float2 lightUV = (lightSpacePos.xy / lightSpacePos.w) * 0.5f + 0.5f;

    if (lightUV.x >= 0.0f && lightUV.x <= 1.0f && lightUV.y >= 0.0f && lightUV.y <= 1.0f) {
        uint2 pageCoords = uint2(lightUV * VSMGridSize / 128); // 128x128 page size
        VSMPageRequestTable[pageCoords] = 1;
    }
}
