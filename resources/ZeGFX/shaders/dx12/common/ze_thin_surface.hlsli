#ifndef ZE_THIN_SURFACE_HLSLI
#define ZE_THIN_SURFACE_HLSLI
#include "ze_brdf.hlsli"

// The GBuffer stores wrap/transmission in two nibbles. Forward uses the same
// quantization, so changing render paths does not change the diffuse model.
float2 ZeThinQuantize(float2 value) { return round(saturate(value)*15.0)/15.0; }
uint ZeThinPack(float2 value) {
    uint2 encoded=(uint2)round(saturate(value)*15.0);
    return encoded.x | (encoded.y<<4);
}
float2 ZeThinUnpack(uint value) { return float2(value&15u,(value>>4)&15u)/15.0; }
float3 ZeThinNormal(float3 normal, float upBlend) {
    float3 candidate=normal+float3(0,saturate(upBlend),0);
    return dot(candidate,candidate)>1e-8 ? normalize(candidate) : normal;
}
BrdfResult EvaluateThinSurfaceBrdf(BrdfInput input, float2 thin) {
    BrdfResult result=EvaluateDirectBrdf(input);
    if (any(thin>0)) {
        float3 halfVector=input.L+input.V;
        float halfLengthSquared=dot(halfVector,halfVector);
        // At opposite light/view directions the reflection half vector is
        // undefined. Its limiting L.H is zero; transmission uses the incident
        // interface angle below and remains well-defined in this configuration.
        float LoH=halfLengthSquared>1e-8f
            ? saturate(dot(input.L,halfVector)*rsqrt(max(halfLengthSquared,1e-8f))) : 0.0f;
        float3 F0=lerp(.04.xxx,input.baseColor,input.metallic);
        float3 kD=(1-FresnelSchlick(LoH,F0))*(1-input.metallic);
        float nL=dot(input.N,input.L);
        // Normalized wrapped Lambert plus restrained view-dependent sunlight
        // through a thin sheet. Both remain subject to actual light visibility.
        float wrap=saturate((nL+thin.x)/(1+thin.x))/(1+thin.x);
        float through=saturate(-nL)*pow(saturate(dot(-input.L,input.V)),3)*thin.y;
        // Transmission crosses the sheet; using reflection's grazing half
        // angle incorrectly extinguishes exactly aligned backlighting.
        float3 transmitted=(1-FresnelSchlick(saturate(abs(nL)),F0))*(1-input.metallic);
        result.diffuse=DiffuseLambert(input.baseColor)*(kD*wrap+transmitted*through);
    }
    return result;
}
#endif
