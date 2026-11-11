// ============================================================================
// ZeGFX — Temporal Anti-Aliasing (TAA) Resolve Shader (DirectX 12)
// Fullscreen pass featuring subpixel jitter resolution, depth reprojection,
// variance clipping in YCoCg space, out-of-bounds rejection, history invalidation,
// and subtle contrast-adaptive sharpening.
// ============================================================================

cbuffer TaaConstants : register(b0)
{
    float viewportWidth;
    float viewportHeight;
    float historyWeight;
    uint historyValid;
    float2 jitterOffset;
    float sharpening;
    float stationaryScene;
    row_major float4x4 currentToPreviousClip;
    float4 currentDepthParameters;
    float4 previousDepthParameters;
};

Texture2D sceneColorTex : register(t0);
Texture2D historyTex : register(t1);
Texture2D depthTex : register(t2);
Texture2D<float4> foliageVelocityTex : register(t3);

SamplerState linearClampSampler : register(s0);
SamplerState pointClampSampler : register(s1);

struct VSInput
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float2 ndc;
    ndc.x = (input.pos.x / viewportWidth) * 2.0f - 1.0f;
    ndc.y = 1.0f - (input.pos.y / viewportHeight) * 2.0f;
    output.pos = float4(ndc, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float3 RGBtoYCoCg(float3 c)
{
    return float3(
        c.r * 0.25f + c.g * 0.5f + c.b * 0.25f,
        c.r * 0.5f  - c.b * 0.5f,
       -c.r * 0.25f + c.g * 0.5f - c.b * 0.25f
    );
}

float3 YCoCgtoRGB(float3 c)
{
    return float3(
        c.x + c.y - c.z,
        c.x + c.z,
        c.x - c.y - c.z
    );
}

// History alpha stores linear camera depth, with zero reserved for sky.
// Raw device depth near 1 loses too much precision in an FP16 history target.
float LinearCameraDepth(float depth, float4 parameters)
{
    if (parameters.z > .5f)
        return max((depth - parameters.y) / parameters.x, 0.0f);
    return max(parameters.y / (depth - parameters.x), 0.0f);
}

bool SameHistorySurface(float storedDepth, float expectedDepth, float expectedMaxDepth, bool includesSky,bool stableCoverage)
{
    if (storedDepth == 0.0f) return expectedDepth == 0.0f || includesSky;
    if (expectedDepth == 0.0f) return false;
    // Unchanged geometry needs extra coverage tolerance: previous cubic taps
    // can contain a nearby blade/root sampled just outside this frame's
    // jittered footprint. Keep moving-camera/object rejection tighter.
    float tolerance = .02f + .01f * expectedDepth;
    if (stableCoverage)
        // Cubic history taps each carry a previous 3x3 depth footprint. Its
        // coverage extends beyond the current 3x3 footprint; allow that local
        // depth spread for an unchanged draw layout without relaxing motion.
        tolerance += max(.04f * expectedDepth, 2.0f * (expectedMaxDepth - expectedDepth));
    return storedDepth >= expectedDepth - tolerance && storedDepth <= expectedMaxDepth + tolerance;
}

// Validate the history footprint using point depth taps. Colour already
// contains accumulated subpixel coverage, so preserve it when the footprints
// overlap rather than renormalizing away a different edge layer every phase.
float3 SampleHistoryCubic(float2 uv, float expectedDepth, float expectedMaxDepth, bool includesSky,bool stableCoverage, out bool surfaceValid)
{
    float2 p = uv * float2(viewportWidth, viewportHeight) - .5f;
    int2 base = int2(floor(p));
    float2 f = frac(p);
    float2 w0 = f * (-.5f + f * (1.0f - .5f*f));
    float2 w1 = 1.0f + f*f * (-2.5f + 1.5f*f);
    float2 w2 = f * (.5f + f * (2.0f - 1.5f*f));
    float2 w3 = f*f * (-.5f + .5f*f);
    float2 weights[4] = {w0,w1,w2,w3};
    float3 result = 0;
    float total = 0;
    bool footprintValid = false;
    [unroll] for(int y=0;y<4;++y)
        [unroll] for(int x=0;x<4;++x)
        {
            int2 pixel = base + int2(x-1,y-1);
            if (any(pixel < 0) || any(pixel >= int2(viewportWidth,viewportHeight))) continue;
            float4 historyTap = historyTex.Load(int3(pixel,0));
            footprintValid = footprintValid || SameHistorySurface(historyTap.a,expectedDepth,expectedMaxDepth,includesSky,stableCoverage);
            float weight = weights[x].x * weights[y].y;
            result += historyTap.rgb * weight;
            total += weight;
        }
    // A newly revealed sky pixel must actually have sky at its history
    // coordinate; neighbouring sky cannot validate an old foreground trail.
    float centerDepth = historyTex.Load(int3(clamp(int2(uv*float2(viewportWidth,viewportHeight)),int2(0,0),int2(viewportWidth-1,viewportHeight-1)),0)).a;
    // The outermost pixel has no offscreen depth samples: jitter can switch
    // its entire clamped footprint between a blade and the ground. Preserve
    // finite coverage there only for an unchanged camera/draw layout. Sky
    // still requires sky history, and all motion keeps full depth rejection.
    float2 border = 1.0f / float2(viewportWidth,viewportHeight);
    bool clippedFootprint = any(uv < border) || any(uv > 1.0f-border);
    bool stableBorder = stableCoverage && clippedFootprint && centerDepth > 0;
    surfaceValid = total > .01f && (expectedDepth == 0 ? centerDepth == 0 : (footprintValid || stableBorder));
    return surfaceValid ? max(result / total,0.0f) : 0.0f;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 texelSize = float2(1.0f / max(viewportWidth, 1.0f), 1.0f / max(viewportHeight, 1.0f));
    // The history/output is unjittered. Fetch the current frame where its
    // jittered projection placed this output pixel.
    float2 currentUv = input.uv + float2(jitterOffset.x * 0.5f, -jitterOffset.y * 0.5f);

    // Sky and geometry share the same jittered raster sample positions.
    // Use the nearest surface in the pixel footprint for edge reprojection.
    // Its depth remains stable when a Halton phase misses a thin gap or blade.
    int2 centerPixel = int2(currentUv * float2(viewportWidth,viewportHeight));
    float depth = 1.0f;
    float maxSurfaceDepth = 0.0f;
    bool includesSky = false;
    float2 depthUv = currentUv;
    [unroll] for(int dy=-1;dy<=1;++dy)
        [unroll] for(int dx=-1;dx<=1;++dx)
        {
            int2 pixel = clamp(centerPixel + int2(dx,dy),int2(0,0),int2(viewportWidth-1,viewportHeight-1));
            float candidate = depthTex.Load(int3(pixel,0)).r;
            if(candidate >= .9999999f) includesSky=true;
            else maxSurfaceDepth=max(maxSurfaceDepth,candidate);
            if(candidate < depth) {depth=candidate;depthUv=(float2(pixel)+.5f)*texelSize;}
        }
    bool isSky = depth >= .9999999f;
    float storedDepth = isSky ? 0.0f : min(LinearCameraDepth(depth,currentDepthParameters),65000.0f);

    // Sample current pixel HDR color
    float3 currentSample = max(sceneColorTex.Sample(linearClampSampler, currentUv).rgb, 0.0f);

    // Sharpen only the new sample. Sharpening accumulated history every frame
    // creates a feedback loop that makes static silhouettes breathe.
    if (sharpening > 0.01f)
    {
        float3 top = max(sceneColorTex.Sample(linearClampSampler, currentUv + float2(0, -texelSize.y)).rgb, 0.0f);
        float3 btm = max(sceneColorTex.Sample(linearClampSampler, currentUv + float2(0,  texelSize.y)).rgb, 0.0f);
        float3 lft = max(sceneColorTex.Sample(linearClampSampler, currentUv + float2(-texelSize.x, 0)).rgb, 0.0f);
        float3 rgt = max(sceneColorTex.Sample(linearClampSampler, currentUv + float2( texelSize.x, 0)).rgb, 0.0f);

        float3 crossMin = min(currentSample, min(min(top, btm), min(lft, rgt)));
        float3 crossMax = max(currentSample, max(max(top, btm), max(lft, rgt)));

        float3 sharpened = currentSample + (currentSample - (top + btm + lft + rgt) * 0.25f) * sharpening;
        currentSample = clamp(sharpened, crossMin, crossMax);
    }


    // 3x3 neighborhood statistics in YCoCg space
    float3 currentYCoCg = RGBtoYCoCg(currentSample);
    float3 neighborMin = currentYCoCg;
    float3 neighborMax = currentYCoCg;
    float3 m1 = float3(0, 0, 0);
    float3 m2 = float3(0, 0, 0);

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUv = currentUv + float2(x, y) * texelSize;
            float3 s = max(sceneColorTex.Sample(pointClampSampler, sampleUv).rgb, 0.0f);
            float3 sYCoCg = RGBtoYCoCg(s);
            neighborMin = min(neighborMin, sYCoCg);
            neighborMax = max(neighborMax, sYCoCg);
            m1 += sYCoCg;
            m2 += sYCoCg * sYCoCg;
        }
    }

    // Variance clipping box in YCoCg
    float3 mu = m1 / 9.0f;
    float3 sigma = sqrt(max(m2 / 9.0f - mu * mu, 0.0f));
    // Moving reprojections use a variance box to reject stale colour.
    float3 boxMin = max(neighborMin, mu - 1.25f * sigma);
    float3 boxMax = min(neighborMax, mu + 1.25f * sigma);

    // Reproject clip space directly. The CPU composes this transform in double
    // precision so distant-world coordinates never cancel in shader floats.
    float2 prevUv;
    bool reprojectionValid = true;
    // Reproject the selected surface's motion onto this output pixel. Both
    // history and output coordinates are unjittered; depthUv is a raster texel.
    float2 depthOutputUv = depthUv - float2(jitterOffset.x*.5f,-jitterOffset.y*.5f);
    float4 clipPos = float4(depthOutputUv.x*2-1,1-depthOutputUv.y*2,depth,1);
    float4 prevClip = mul(clipPos,currentToPreviousClip);
    reprojectionValid = prevClip.w > 1e-6f && prevClip.z >= 0;
    float3 prevNdc = prevClip.xyz / max(abs(prevClip.w),1e-6f);
    float2 previousDepthUv = float2(prevNdc.x*.5f+.5f,.5f-prevNdc.y*.5f);
    prevUv = input.uv + previousDepthUv - depthOutputUv;
    float expectedDepth = isSky ? 0.0f : min(LinearCameraDepth(prevNdc.z,previousDepthParameters),65000.0f);
    // A pixel footprint can straddle several surfaces. Validate against its
    // reprojected depth interval, not one jitter-dependent nearest sample.
    float4 farClip = clipPos; farClip.z = maxSurfaceDepth;
    float4 previousFarClip = mul(farClip,currentToPreviousClip);
    float expectedMaxDepth = isSky ? 0.0f : min(LinearCameraDepth(previousFarClip.z / max(abs(previousFarClip.w),1e-6f),previousDepthParameters),65000.0f);
    expectedMaxDepth = max(expectedMaxDepth,expectedDepth);

    float4 foliageMotion=foliageVelocityTex.SampleLevel(pointClampSampler,depthUv,0);
    bool hasFoliageMotion=!isSky&&foliageMotion.w>0&&all(isfinite(foliageMotion))&&
        abs(foliageMotion.w-storedDepth)<=.02+.02*storedDepth&&
        (stationaryScene<.5f||length(foliageMotion.xy/texelSize)>.002f);
    if(hasFoliageMotion){
        prevUv=input.uv+foliageMotion.xy;
        expectedDepth=foliageMotion.z;
        expectedMaxDepth=max(expectedDepth,expectedDepth+LinearCameraDepth(maxSurfaceDepth,currentDepthParameters)-storedDepth);
        reprojectionValid=true;
    }
    bool stableCoverage=stationaryScene>.5f||hasFoliageMotion;
    // Check bounds and history validity
    // Accept coordinates inside the image, including the partial footprint at
    // its edges. Reprojection roundoff must not discard the first/last pixel;
    // the cubic sampler clips and normalizes its available taps explicitly.
    bool inBounds = all(prevUv >= 0.0f) && all(prevUv <= 1.0f);
    bool valid = (historyValid != 0u) && inBounds && reprojectionValid;

    float3 resolved;
    if (valid)
    {
        bool surfaceValid;
        float3 historySample = SampleHistoryCubic(prevUv,expectedDepth,expectedMaxDepth,includesSky,stableCoverage,surfaceValid);
        float3 historyYCoCg = RGBtoYCoCg(historySample);

        // Clamp history to current neighborhood bounding box to prevent ghosting
        float3 clampedHistoryYCoCg = clamp(historyYCoCg, boxMin, boxMax);
        float motionPixels = length((prevUv-input.uv)/texelSize);
        float3 rejectedHistory = max(YCoCgtoRGB(clampedHistoryYCoCg), 0.0f);
        // Retain coverage only when the CPU confirms an unchanged camera and
        // draw layout. Camera or object changes use full colour rejection as
        // well as depth validation, including slow motion on distant surfaces.
        // Correctly reprojected foliage retains subpixel coverage within its local colour range.
        float3 plantHistory=max(YCoCgtoRGB(clamp(historyYCoCg,neighborMin,neighborMax)),0);
        float3 clampedHistory=hasFoliageMotion?plantHistory:(stationaryScene>.5f?historySample:rejectedHistory);
        float weight = lerp(clamp(historyWeight, 0.50f, 0.95f),.65f,saturate(motionPixels/8));
        resolved = surfaceValid ? lerp(currentSample, clampedHistory, weight) : currentSample;
    }
    else
    {
        resolved = currentSample;
    }

    return float4(max(resolved, 0.0f), storedDepth);
}
