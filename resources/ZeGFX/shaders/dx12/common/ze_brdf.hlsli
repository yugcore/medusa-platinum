#ifndef ZE_BRDF_HLSLI
#define ZE_BRDF_HLSLI

#ifndef PI
#define PI 3.14159265359f
#endif

struct BrdfInput
{
    float3 N;                       // World normal
    float3 V;                       // View vector (direction to camera)
    float3 L;                       // Light vector (direction to light)
    float3 baseColor;               // Linear albedo
    float metallic;                 // Clamped metallness
    float perceptualRoughness;      // Clamped perceptual roughness
};

struct BrdfResult
{
    float3 diffuse;                 // Diffuse BRDF * N.L
    float3 specular;                // Specular BRDF * N.L
};

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float D_GGX(float NoH, float alphaRoughness) {
    // alpha is perceptual roughness squared. Bound the input rather than
    // flooring the denominator: that floor erases smooth GGX highlights.
    float alpha = clamp(alphaRoughness, 0.000001f, 1.0f);
    float a2 = alpha * alpha;
    float h = saturate(NoH);
    // Factor 1-h*h to retain the small positive term at the specular peak.
    // precise prevents compiler reassociation back to a cancellation-prone form.
    precise float f = (1.0f - h) * (1.0f + h) + h * h * a2;
    return a2 / (PI * f * f);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float alphaRoughness) {
    float a2 = alphaRoughness * alphaRoughness;
    float GGXV = NoL * sqrt(NoV * NoV * (1.0f - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0f - a2) + a2);
    return 0.5f / max(GGXV + GGXL, 0.00001f);
}

float3 DiffuseLambert(float3 diffuseColor) {
    return diffuseColor / PI;
}

BrdfResult EvaluateDirectBrdf(BrdfInput input) {
    BrdfResult result;
    result.diffuse = float3(0.0f, 0.0f, 0.0f);
    result.specular = float3(0.0f, 0.0f, 0.0f);

    float NoL = saturate(dot(input.N, input.L));
    float NoV = saturate(dot(input.N, input.V));
    // Reflection is defined only in the upper hemisphere. Reject before
    // normalizing L+V, which is zero for exactly opposite directions.
    if (NoL <= 0.0f || NoV <= 0.0f) return result;
    float3 H = normalize(input.L + input.V);
    float NoH = saturate(dot(input.N, H));
    float LoH = saturate(dot(input.L, H));

    // Clamp roughness to prevent NaN/division issues
    float roughness = max(input.perceptualRoughness, 0.001f);
    float alphaRoughness = roughness * roughness;

    // Specular F0 (dielectrics = 0.04, metal = baseColor)
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), input.baseColor, input.metallic);
    float3 F = FresnelSchlick(LoH, F0);

    float D = D_GGX(NoH, alphaRoughness);
    float V_term = V_SmithGGXCorrelated(NoV, NoL, alphaRoughness);

    // Specular Term
    float3 specBRDF = F * (D * V_term);
    float3 directSpec = specBRDF * NoL;

    // Specular firefly suppression & dielectric energy bounding:
    // Dielectrics have F0 = 0.04 and should not exhibit unboundedly intense metallic-like
    // specular spikes on single sub-pixel microfacets. Pure metals (metallic = 1.0)
    // retain full, brilliant specular highlights.
    float maxDielectricSpec = 3.0f;
    float maxSpecular = lerp(maxDielectricSpec, 60.0f, input.metallic);
    float specLuma = dot(directSpec, float3(0.2126f, 0.7152f, 0.0722f));
    if (specLuma > maxSpecular) {
        directSpec *= (maxSpecular + (specLuma - maxSpecular) / (1.0f + (specLuma - maxSpecular) / maxSpecular)) / specLuma;
    }
    result.specular = directSpec;

    // Diffuse Term (energy conservation)
    float3 kD = (float3(1.0f, 1.0f, 1.0f) - F) * (1.0f - input.metallic);
    result.diffuse = kD * DiffuseLambert(input.baseColor) * NoL;

    return result;
}

// Legacy compatibility wrapper
float3 EvaluateDefaultLitBRDF(float3 L, float3 V, float3 N, float3 baseColor, float roughness, float metallic, float3 F0) {
    BrdfInput input;
    input.N = N;
    input.V = V;
    input.L = L;
    input.baseColor = baseColor;
    input.metallic = metallic;
    input.perceptualRoughness = roughness;
    
    BrdfResult res = EvaluateDirectBrdf(input);
    
    // EvaluateDirectBrdf returns BRDF * NoL. To match the old signature which didn't multiply NoL, we divide by NoL or recompute.
    // Let's divide by max(NoL, 0.0001f) to match legacy code exactly.
    float NoL = saturate(dot(N, L));
    return (res.diffuse + res.specular) / max(NoL, 0.0001f);
}

#endif // ZE_BRDF_HLSLI
