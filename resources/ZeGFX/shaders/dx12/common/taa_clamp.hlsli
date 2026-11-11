// Clamp history color to prevent smearing/ghosting
float3 ClampHistory(float3 historyColor, float3 currentM1, float3 currentM2) {
    // Computes average and standard deviation in a 3x3 local neighborhood
    float3 mu = currentM1 / 9.0f;
    float3 sigma = sqrt(abs(currentM2 / 9.0f - mu * mu));
    
    // Variance box scale
    float gamma = 1.5f;
    float3 boxMin = mu - gamma * sigma;
    float3 boxMax = mu + gamma * sigma;

    return clamp(historyColor, boxMin, boxMax);
}
