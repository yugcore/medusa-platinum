struct ClusterData {
    float3 Center;
    float  Radius;
    float3 ConeNormal;
    float  ConeAngle;
    uint   VertexOffset;
    uint   VertexCount;
    uint   IndexOffset;
    uint   TriangleCount;
};

StructuredBuffer<ClusterData> ClusterBuffer : register(t2);
AppendStructuredBuffer<uint> VisibleClusterArgs : register(u0);

cbuffer CullParams : register(b0) {
    float4 FrustumPlanes[6];
    uint   TotalClusterCount;
};

bool IsFrustumCulled(float3 Center, float Radius) {
    for (int i = 0; i < 6; ++i) {
        if (dot(float4(Center, 1.0f), FrustumPlanes[i]) < -Radius) {
            return true;
        }
    }
    return false;
}

[numthreads(64, 1, 1)]
void ClusterCullCS(uint3 DispatchThreadID : SV_DispatchThreadID) {
    uint ClusterIndex = DispatchThreadID.x;
    if (ClusterIndex >= TotalClusterCount) return;

    ClusterData cluster = ClusterBuffer[ClusterIndex];
    
    // 1. Frustum Culling
    if (IsFrustumCulled(cluster.Center, cluster.Radius)) return;

    // Output visible cluster ID for drawing
    VisibleClusterArgs.Append(ClusterIndex);
}
