// ============================================================================
// ZeGFX — Compute Software Rasterizer for Sub-Pixel Meshlet Clusters
// ============================================================================

struct SoftwareClusterTriangle {
    float3 v0;
    float3 v1;
    float3 v2;
    uint   instanceId;
    uint   primitiveId;
};

cbuffer SoftwareRasterParams : register(b0) {
    float4x4 ViewProjection;
    uint2    ViewportDimensions;
    uint     TriangleCount;
    uint     SoftwareThresholdPixels;
};

StructuredBuffer<SoftwareClusterTriangle> Triangles : register(t0);
RWTexture2D<uint2> VisibilityBufferUAV : register(u0); // .x = packed depth (uint), .y = instance/primitive payload

float EdgeFunction(float2 a, float2 b, float2 c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

[numthreads(64, 1, 1)]
void CS_SoftwareRasterize(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint triIdx = dispatchThreadId.x;
    if (triIdx >= TriangleCount) return;

    SoftwareClusterTriangle tri = Triangles[triIdx];

    // Project vertices to clip space
    float4 p0 = mul(ViewProjection, float4(tri.v0, 1.0f));
    float4 p1 = mul(ViewProjection, float4(tri.v1, 1.0f));
    float4 p2 = mul(ViewProjection, float4(tri.v2, 1.0f));

    if (p0.w <= 0.0f || p1.w <= 0.0f || p2.w <= 0.0f) return;

    // Convert to NDC and screen space
    float2 s0 = (p0.xy / p0.w * 0.5f + 0.5f) * float2(ViewportDimensions);
    float2 s1 = (p1.xy / p1.w * 0.5f + 0.5f) * float2(ViewportDimensions);
    float2 s2 = (p2.xy / p2.w * 0.5f + 0.5f) * float2(ViewportDimensions);

    // Compute bounding box
    float2 minP = floor(min(s0, min(s1, s2)));
    float2 maxP = ceil(max(s0, max(s1, s2)));

    float area = EdgeFunction(s0, s1, s2);
    if (area <= 0.0f) return; // Backface culling

    // Software rasterize sub-pixel triangle bounds
    int startX = max(0, (int)minP.x);
    int endX   = min((int)ViewportDimensions.x - 1, (int)maxP.x);
    int startY = max(0, (int)minP.y);
    int endY   = min((int)ViewportDimensions.y - 1, (int)maxP.y);

    uint packedPayload = (tri.instanceId << 16) | (tri.primitiveId & 0xFFFF);

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            float2 pixelSample = float2(x, y) + 0.5f;

            float w0 = EdgeFunction(s1, s2, pixelSample);
            float w1 = EdgeFunction(s2, s0, pixelSample);
            float w2 = EdgeFunction(s0, s1, pixelSample);

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float invArea = 1.0f / area;
                float b0 = w0 * invArea;
                float b1 = w1 * invArea;
                float b2 = w2 * invArea;

                float depth = b0 * (p0.z / p0.w) + b1 * (p1.z / p1.w) + b2 * (p2.z / p2.w);
                uint depthUint = asuint(depth);

                InterlockedMin(VisibilityBufferUAV[uint2(x, y)].x, depthUint);
                VisibilityBufferUAV[uint2(x, y)].y = packedPayload;
            }
        }
    }
}
