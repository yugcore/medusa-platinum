#include "../common/ze_raytracing_common.hlsli"

[shader("closesthit")]
void HitMain(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    payload.distance = RayTCurrent();
    payload.instanceId = InstanceID();
    payload.geometryId = GeometryIndex();
    payload.color = float3(1.f, 1.f, 1.f);
}
