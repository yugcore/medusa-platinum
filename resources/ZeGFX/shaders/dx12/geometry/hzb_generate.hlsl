// ============================================================================
// ZeGFX — Hierarchical Depth (Hi-Z) Downsampling Compute Shader
// ============================================================================

Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> TargetHZBMip : register(u0);

cbuffer HZBParams : register(b0) {
    uint2 SourceDimensions;
    uint2 TargetDimensions;
};

[numthreads(16, 16, 1)]
void CS_BuildHZB(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if (dispatchThreadId.x >= TargetDimensions.x || dispatchThreadId.y >= TargetDimensions.y) return;

    uint2 srcCoords = dispatchThreadId.xy * 2;

    float d0 = SourceDepth.Load(int3(srcCoords, 0));
    float d1 = SourceDepth.Load(int3(min(srcCoords + uint2(1, 0), SourceDimensions - 1), 0));
    float d2 = SourceDepth.Load(int3(min(srcCoords + uint2(0, 1), SourceDimensions - 1), 0));
    float d3 = SourceDepth.Load(int3(min(srcCoords + uint2(1, 1), SourceDimensions - 1), 0));

    // Reverse-Z: minimum depth value represents the closest conservative occluder
    float minDepth = min(min(d0, d1), min(d2, d3));

    TargetHZBMip[dispatchThreadId.xy] = minDepth;
}
