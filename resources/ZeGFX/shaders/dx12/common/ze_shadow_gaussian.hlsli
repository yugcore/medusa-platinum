#ifndef ZE_SHADOW_GAUSSIAN_HLSLI
#define ZE_SHADOW_GAUSSIAN_HLSLI

// Exact consolidation of a unit-spaced 5x5 binomial kernel whose comparisons
// are bilinearly interpolated. Adjacent samples reuse the same depth texels:
// 36 unique comparisons replace 100 loads without changing the kernel.
// The caller retains its ordinary path for non-unit spacing and page edges.
float ZeGaussianShadowUnitSpacing(Texture2D map, float2 uv, float2 size,
    float depth, float2 gradient, float bias)
{
    float2 pixel = uv * size - 0.5f;
    float2 origin = floor(pixel), blend = frac(pixel);
    static const float kernel[5] = {0.0625f,0.25f,0.375f,0.25f,0.0625f};
    float2 weights[6];
    weights[0] = kernel[0] * (1.0f-blend);
    [unroll] for(int i=1;i<5;++i)
        weights[i] = kernel[i-1]*blend + kernel[i]*(1.0f-blend);
    weights[5] = kernel[4]*blend;
    float visibility = 0;
    [unroll] for(int y=0;y<6;++y) [unroll] for(int x=0;x<6;++x) {
        int2 tap = int2(origin) + int2(x-2,y-2);
        float2 tapUv = (float2(tap)+0.5f)/size;
        float receiver = ZeShadowReceiverTapDepth(depth,gradient,tapUv-uv);
        visibility += weights[x].x*weights[y].y *
            (receiver-bias <= map.Load(int3(tap,0)).r ? 1.0f : 0.0f);
    }
    return visibility;
}
#endif
