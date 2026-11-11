Texture2D<float4> SceneColorHDR : register(t0);
Texture2D<float4> BloomTexture : register(t1);
RWTexture2D<float4> DisplayBackbuffer : register(u0);

cbuffer PostProcessConstants : register(b0) {
    float BloomIntensity;
    float CameraExposureValue;
    float2 ScreenResolution;
};

// Stephen Hill ACES Filmic Curve Fit
float3 ACESFilm(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[numthreads(16, 16, 1)]
void TonemapCS(uint3 DispatchThreadID : SV_DispatchThreadID) {
    if (DispatchThreadID.x >= (uint)ScreenResolution.x || DispatchThreadID.y >= (uint)ScreenResolution.y) {
        return;
    }

    float3 hdrColor = SceneColorHDR[DispatchThreadID.xy].rgb;
    float3 bloom = BloomTexture[DispatchThreadID.xy].rgb;
    
    // Mix bloom
    float3 finalColor = hdrColor + bloom * BloomIntensity;

    // Apply exposure
    finalColor *= CameraExposureValue;

    // Tone Map to LDR range [0, 1]
    float3 tonemapped = ACESFilm(finalColor);

    // Gamma correction
    float3 srgb = pow(tonemapped, 1.0f / 2.2f);

    DisplayBackbuffer[DispatchThreadID.xy] = float4(srgb, 1.0f);
}
