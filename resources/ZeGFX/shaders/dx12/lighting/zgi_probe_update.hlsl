#include "../common/ze_light_types.hlsli"

struct ZGiSceneProxy
{
    float4 centerRadius;
    float4 albedoEmissive;
};

StructuredBuffer<ZGiSceneProxy> SceneProxies : register(t0);
StructuredBuffer<GpuLocalLight> LocalLights : register(t1);
RWStructuredBuffer<float4> ProbeIrradiance : register(u0);
RWStructuredBuffer<float4> ProbeDistanceMoments : register(u1);

cbuffer ZGiUpdateConstants : register(b0)
{
    uint3 probeCounts;
    uint totalProbeCount;

    float3 volumeOrigin;
    float maximumRayDistance;

    float3 probeSpacing;
    float irradianceHysteresis;

    uint startProbe;
    uint probesToUpdate;
    uint raysPerProbe;
    uint resetHistory;

    uint sceneProxyCount;
    uint localLightCount;
    uint dirtyReasons;
    uint frameIndex;

    float3 skyHorizon;
    float skyIntensity;

    float3 skyZenith;
    float directionalIntensity;

    float3 directionalDirection;
    float directionalPadding;

    float3 directionalColor;
    float colorPadding;
};

float3 SphericalFibonacciDirection(uint sampleIndex, uint sampleCount, uint rotationSeed)
{
    float index = (float)((sampleIndex + rotationSeed) % max(sampleCount, 1u));
    float count = (float)max(sampleCount, 1u);
    float cosTheta = 1.0f - 2.0f * (index + 0.5f) / count;
    float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
    float phi = 2.39996323f * index + (float)rotationSeed * 0.013f;
    return float3(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta);
}

bool IntersectSphere(
    float3 rayOrigin,
    float3 rayDirection,
    float4 centerRadius,
    out float hitDistance)
{
    float3 offset = rayOrigin - centerRadius.xyz;
    float b = dot(offset, rayDirection);
    float c = dot(offset, offset) - centerRadius.w * centerRadius.w;
    float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        hitDistance = maximumRayDistance;
        return false;
    }

    float root = sqrt(discriminant);
    float nearHit = -b - root;
    float farHit = -b + root;
    hitDistance = nearHit > 0.01f ? nearHit : farHit;
    return hitDistance > 0.01f && hitDistance <= maximumRayDistance;
}

float3 EvaluateLocalLights(float3 worldPosition, float3 normal)
{
    float3 lighting = 0.0f;
    for (uint lightIndex = 0; lightIndex < localLightCount; ++lightIndex) {
        GpuLocalLight light = LocalLights[lightIndex];
        float3 toLight = light.positionRange.xyz - worldPosition;
        float distanceToLight = length(toLight);
        float range = max(light.positionRange.w, 0.001f);
        if (distanceToLight >= range) {
            continue;
        }

        float3 lightDirection = toLight / max(distanceToLight, 0.001f);
        float attenuation = saturate(1.0f - pow(distanceToLight / range, 4.0f));
        attenuation = attenuation * attenuation / (distanceToLight * distanceToLight + 1.0f);
        if (light.directionType.w == 2.0f) {
            float cosAngle = dot(-lightDirection, normalize(light.directionType.xyz));
            float cone = saturate(
                (cosAngle - light.coneRectFlags.y) /
                max(light.coneRectFlags.x - light.coneRectFlags.y, 0.0001f));
            attenuation *= cone * cone;
        }

        lighting +=
            light.colorIntensity.xyz *
            light.colorIntensity.w *
            attenuation *
            saturate(dot(normal, lightDirection));
    }
    return lighting;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint probeIndex = dispatchThreadId.x;
    if (probeIndex >= totalProbeCount) {
        return;
    }

    uint relativeIndex =
        (probeIndex + totalProbeCount - min(startProbe, totalProbeCount - 1u)) %
        max(totalProbeCount, 1u);
    bool scheduled = relativeIndex < probesToUpdate;
    if (!scheduled) {
        if (resetHistory != 0) {
            ProbeIrradiance[probeIndex] = 0.0f;
            ProbeDistanceMoments[probeIndex] = 0.0f;
        }
        return;
    }

    uint sliceSize = max(probeCounts.x * probeCounts.y, 1u);
    uint3 probeCoord;
    probeCoord.z = probeIndex / sliceSize;
    uint remainder = probeIndex % sliceSize;
    probeCoord.y = remainder / max(probeCounts.x, 1u);
    probeCoord.x = remainder % max(probeCounts.x, 1u);
    float3 probePosition = volumeOrigin + float3(probeCoord) * probeSpacing;

    float3 accumulatedRadiance = 0.0f;
    float accumulatedDistance = 0.0f;
    float accumulatedDistanceSquared = 0.0f;
    const uint traceCount = max(raysPerProbe, 1u);

    for (uint rayIndex = 0; rayIndex < traceCount; ++rayIndex) {
        float3 rayDirection = SphericalFibonacciDirection(
            rayIndex,
            traceCount,
            frameIndex + probeIndex * 17u);
        float nearestDistance = maximumRayDistance;
        uint nearestProxy = 0xFFFFFFFFu;

        for (uint proxyIndex = 0; proxyIndex < sceneProxyCount; ++proxyIndex) {
            float hitDistance;
            if (IntersectSphere(
                    probePosition,
                    rayDirection,
                    SceneProxies[proxyIndex].centerRadius,
                    hitDistance) &&
                hitDistance < nearestDistance) {
                nearestDistance = hitDistance;
                nearestProxy = proxyIndex;
            }
        }

        float3 radiance;
        if (nearestProxy != 0xFFFFFFFFu) {
            ZGiSceneProxy proxy = SceneProxies[nearestProxy];
            float3 hitPosition = probePosition + rayDirection * nearestDistance;
            float3 normal = normalize(hitPosition - proxy.centerRadius.xyz);
            float3 sunDirection = normalize(-directionalDirection);
            float3 sun =
                directionalColor *
                directionalIntensity *
                0.0001f *
                saturate(dot(normal, sunDirection));
            float skyFactor = saturate(normal.y * 0.5f + 0.5f);
            float3 sky = lerp(skyHorizon, skyZenith, skyFactor) * skyIntensity;
            float3 local = EvaluateLocalLights(hitPosition, normal);
            radiance =
                proxy.albedoEmissive.rgb * (sky + sun + local) +
                proxy.albedoEmissive.rgb * proxy.albedoEmissive.w;
        } else {
            float skyFactor = saturate(rayDirection.y * 0.5f + 0.5f);
            radiance = lerp(skyHorizon, skyZenith, skyFactor) * skyIntensity;
        }

        accumulatedRadiance += radiance;
        accumulatedDistance += nearestDistance;
        accumulatedDistanceSquared += nearestDistance * nearestDistance;
    }

    float inverseRayCount = 1.0f / (float)traceCount;
    float3 tracedIrradiance = accumulatedRadiance * inverseRayCount;
    float2 tracedMoments = float2(
        accumulatedDistance * inverseRayCount,
        accumulatedDistanceSquared * inverseRayCount);

    float4 oldIrradiance = resetHistory != 0 ? 0.0f : ProbeIrradiance[probeIndex];
    float4 oldMoments = resetHistory != 0 ? 0.0f : ProbeDistanceMoments[probeIndex];
    float historyWeight =
        resetHistory != 0 || oldIrradiance.a < 0.5f
        ? 0.0f
        : saturate(irradianceHysteresis);

    ProbeIrradiance[probeIndex] = float4(
        lerp(tracedIrradiance, oldIrradiance.rgb, historyWeight),
        1.0f);
    ProbeDistanceMoments[probeIndex] = float4(
        lerp(tracedMoments, oldMoments.xy, historyWeight),
        1.0f,
        1.0f);
}
