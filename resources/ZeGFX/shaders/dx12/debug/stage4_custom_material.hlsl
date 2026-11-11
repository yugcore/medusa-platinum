// Example custom forward material. Reuses ZeGFX's packed mesh, skinning,
// lighting and conventional shadow contract; replaces only the final color.
#define PSMain ZeDefaultMaterialPS
#include "../materials/pbr_material.hlsl"
#undef PSMain

float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    float4 lit = ZeDefaultMaterialPS(input,isFrontFace);
    float stripe = step(0.5f,frac(input.uv.x*8.0f));
    return float4(lerp(lit.rgb,lit.rgb*float3(0.15f,1.0f,0.3f),stripe),lit.a);
}
