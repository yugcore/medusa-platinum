Texture2D sceneColor : register(t0);
RWByteAddressBuffer histogramBuffer : register(u0);

// Log luminance conversion constants
#define MIN_LOG_LUMINANCE -8.0f
#define MAX_LOG_LUMINANCE 4.0f
#define LOG_LUMINANCE_RANGE (MAX_LOG_LUMINANCE - MIN_LOG_LUMINANCE)

groupshared uint sharedHistogram[256];

[numthreads(16, 16, 1)]
void CSMain(uint3 groupThreadID : SV_GroupThreadID, uint3 globalThreadID : SV_DispatchThreadID) {
    uint localIndex = groupThreadID.y * 16 + groupThreadID.x;
    sharedHistogram[localIndex] = 0;
    
    GroupMemoryBarrierWithGroupSync();
    
    // Sample scene color and compute luminance
    float3 color = sceneColor.Load(int3(globalThreadID.xy, 0)).rgb;
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    
    if (luminance > 0.0001f) {
        float logLuminance = log2(luminance);
        float normalizedLog = saturate((logLuminance - MIN_LOG_LUMINANCE) / LOG_LUMINANCE_RANGE);
        uint bin = uint(normalizedLog * 255.0f);
        InterlockedAdd(sharedHistogram[bin], 1);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Accumulate to global buffer
    histogramBuffer.InterlockedAdd(localIndex * 4, sharedHistogram[localIndex]);
}
