// ZExposureReduction.hlsl
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16

struct ExposureConstants
{
    uint   SceneColorIndex;
    uint   EyeAdaptationUavIndex;
    uint   ViewWidth;
    uint   ViewHeight;
    float  MinExposure;
    float  MaxExposure;
    float  AdaptationSpeedUp;
    float  AdaptationSpeedDown;
    float  DeltaTime;
};

ConstantBuffer<ExposureConstants> cb : register(b0);

// Global Bindless Heap Access
Texture2D<float4> BindlessTextures[] : register(t0, space0);
RWStructuredBuffer<float4> BindlessBuffersUAV[] : register(u0, space0);

groupshared float gs_LuminanceSum[THREAD_GROUP_SIZE_X * THREAD_GROUP_SIZE_Y];

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void MainCS(uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    Texture2D<float4> sceneColor = BindlessTextures[cb.SceneColorIndex];
    
    float logLuminanceSum = 0.0f;
    uint sampleCount = 0;

    // Direct tiled sampling
    if (dispatchThreadId.x < cb.ViewWidth && dispatchThreadId.y < cb.ViewHeight)
    {
        float3 color = sceneColor.Load(int3(dispatchThreadId.xy, 0)).rgb;
        float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
        
        // Log-luminance bias to prevent divide-by-zero on black pixels
        logLuminanceSum = log(max(luminance, 0.0001f));
        sampleCount = 1;
    }

    // Write thread results to Local Data Share (LDS)
    gs_LuminanceSum[groupIndex] = logLuminanceSum;
    GroupMemoryBarrierWithGroupSync();

    // Perform LDS reduction using wave instructions
    for (uint stride = (THREAD_GROUP_SIZE_X * THREAD_GROUP_SIZE_Y) / 2; stride > 0; stride /= 2)
    {
        if (groupIndex < stride)
        {
            gs_LuminanceSum[groupIndex] += gs_LuminanceSum[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Thread 0 in the first group updates the persistent exposure state
    if (groupIndex == 0 && dispatchThreadId.x == 0 && dispatchThreadId.y == 0)
    {
        float totalLogLuminance = gs_LuminanceSum[0] / (cb.ViewWidth * cb.ViewHeight);
        float currentLuminance = exp(totalLogLuminance);

        RWStructuredBuffer<float4> exposureBuffer = BindlessBuffersUAV[cb.EyeAdaptationUavIndex];
        float lastExposure = exposureBuffer[0].x;

        // Calculate targeted exposure value
        float targetExposure = 1.0f / max(currentLuminance, 0.0001f);
        targetExposure = clamp(targetExposure, cb.MinExposure, cb.MaxExposure);

        // Interpolate adaptation over time
        float speed = (targetExposure > lastExposure) ? cb.AdaptationSpeedUp : cb.AdaptationSpeedDown;
        float finalExposure = lastExposure + (targetExposure - lastExposure) * (1.0f - exp(-cb.DeltaTime * speed));

        // Save exposure values: [Current Exposure, Target Exposure, Average Luminance, Frame Time]
        exposureBuffer[0] = float4(finalExposure, targetExposure, currentLuminance, cb.DeltaTime);
    }
}
