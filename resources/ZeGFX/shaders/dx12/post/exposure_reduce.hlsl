ByteAddressBuffer histogramBuffer : register(t0);
RWByteAddressBuffer exposureState : register(u0);

cbuffer ExposureConstants : register(b0) {
    float minEV;
    float maxEV;
    float speedBrighten;
    float speedDarken;
    float deltaTime;
    float exposureCompensation;
};

[numthreads(1, 1, 1)]
void CSMain() {
    // Read histogram and compute average
    float totalPixels = 0.0f;
    float weightedSum = 0.0f;
    
    for (uint i = 0; i < 256; ++i) {
        float count = float(histogramBuffer.Load(i * 4));
        // Percentile clamp: simple weighting
        float val = float(i) / 255.0f;
        weightedSum += val * count;
        totalPixels += count;
    }
    
    float avgNormalizedLog = (totalPixels > 0.0f) ? (weightedSum / totalPixels) : 0.5f;
    float avgLogLuminance = avgNormalizedLog * 12.0f - 8.0f; // Range [-8, 4]
    float avgLuminance = exp2(avgLogLuminance);
    
    // Adapt current EV
    float currentEV = asfloat(exposureState.Load(0));
    float targetEV = log2(max(avgLuminance, 0.0001f));
    targetEV = max(minEV, min(maxEV, targetEV));
    
    float speed = (targetEV > currentEV) ? speedBrighten : speedDarken;
    currentEV += (targetEV - currentEV) * (1.0f - exp(-speed * deltaTime));
    
    exposureState.Store(0, asuint(currentEV));
    
    // Write exposure multiplier to offset 4
    float exposureMult = exp2(-currentEV + exposureCompensation);
    exposureState.Store(4, asuint(exposureMult));
}
