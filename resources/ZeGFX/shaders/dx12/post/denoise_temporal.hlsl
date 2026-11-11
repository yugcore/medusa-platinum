// ============================================================================
// ZeGFX — Temporal Denoising Compute Shader (SM 6.0)
// ============================================================================

Texture2D<float> RawInput : register(t0);
Texture2D<float> DepthBuffer : register(t1);
Texture2D<float2> MotionVectorBuffer : register(t2);
Texture2D<float> HistoryBuffer : register(t3);
RWTexture2D<float> OutTemporalAccum : register(u0);

cbuffer DenoiseConstants : register(b0) {
    float blendFactor; // Typically 0.05
    float depthThreshold;
    int width;
    int height;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 uv = dispatchThreadID.xy;
    if (uv.x >= (uint)width || uv.y >= (uint)height) return;
    
    float rawColor = RawInput[uv].r;
    float2 motion = MotionVectorBuffer[uv].xy;
    int2 historyUV = int2(round(float2(uv) - motion * float2(width, height)));
    historyUV = clamp(historyUV, int2(0, 0), int2(width - 1, height - 1));
    
    // 3x3 Neighborhood bounding box calculation
    float minColor = 99999.0; 
    float maxColor = -99999.0;
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int2 sampleUV = clamp(int2(uv) + int2(dx, dy), int2(0, 0), int2(width - 1, height - 1));
            float c = RawInput[sampleUV].r;
            minColor = min(minColor, c);
            maxColor = max(maxColor, c);
        }
    }
    
    float historyColor = HistoryBuffer[historyUV].r;
    // Clamp history to current neighborhood
    historyColor = clamp(historyColor, minColor, maxColor);
    
    // Exponential moving average blend
    OutTemporalAccum[uv] = lerp(historyColor, rawColor, blendFactor);
}
