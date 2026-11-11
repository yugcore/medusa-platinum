// ============================================================================
// ZeGFX — Spatial Bilateral Denoiser Compute Shader (SM 6.0)
// ============================================================================

Texture2D<float> InputBuffer : register(t0);
Texture2D<float> DepthBuffer : register(t1);
Texture2D<float4> NormalBuffer : register(t2);
RWTexture2D<float> OutBilateralFiltered : register(u0);

cbuffer SpatialConstants : register(b0) {
    int blurRadius;
    float depthSigma;
    float normalSigma;
    int width;
    int height;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 uv = dispatchThreadID.xy;
    if (uv.x >= (uint)width || uv.y >= (uint)height) return;
    
    float centerVal = InputBuffer[uv].r;
    float centerDepth = DepthBuffer[uv].r;
    float3 centerNormal = NormalBuffer[uv].xyz;
    
    float sumVal = 0.0;
    float sumWeight = 0.0;
    
    for (int dy = -blurRadius; dy <= blurRadius; ++dy) {
        for (int dx = -blurRadius; dx <= blurRadius; ++dx) {
            int2 sampleOffset = int2(dx, dy);
            int2 sampleCoord = int2(uv) + sampleOffset;
            
            if (sampleCoord.x < 0 || sampleCoord.x >= width || 
                sampleCoord.y < 0 || sampleCoord.y >= height) continue;
            
            uint2 sampleUV = uint2(sampleCoord);
            float val = InputBuffer[sampleUV].r;
            float depth = DepthBuffer[sampleUV].r;
            float3 normal = NormalBuffer[sampleUV].xyz;
            
            // Bilateral weighting factors
            float spatialWeight = exp(-float(dx*dx + dy*dy) / (2.0 * max(1, blurRadius * blurRadius)));
            float depthWeight = exp(-abs(centerDepth - depth) / max(0.0001f, depthSigma));
            float normalWeight = pow(saturate(dot(centerNormal, normal)), normalSigma);
            
            float weight = spatialWeight * depthWeight * normalWeight;
            sumVal += val * weight;
            sumWeight += weight;
        }
    }
    OutBilateralFiltered[uv] = sumWeight > 0.0f ? (sumVal / sumWeight) : centerVal;
}
