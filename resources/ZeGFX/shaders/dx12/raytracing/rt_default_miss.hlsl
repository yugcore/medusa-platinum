#include "../common/ze_raytracing_common.hlsli"

[shader("miss")]
void MissMain(inout RayPayload payload) {
    payload.color = float3(0.2f, 0.2f, 0.25f);
    payload.distance = -1.f;
    payload.instanceId = 0xFFFFFFFF;
    payload.geometryId = 0xFFFFFFFF;
}
