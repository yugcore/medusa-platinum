struct DenseGeometryCluster
{
    float4 centerRadius;
    uint firstIndex;
    uint indexCount;
    uint pageIndex;
    uint binIndex;
    uint binCommandBase;
    uint flags;
    uint2 padding;
};

struct DenseGeometryPage
{
    uint firstCluster;
    uint clusterCount;
    uint firstIndex;
    uint indexCount;
};

struct DrawIndexedArguments
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

cbuffer DenseGeometryCullConstants : register(b0)
{
    float4 frustumPlanes[6];
    float4 cameraMaxDistance;
    uint clusterCount;
    uint pageCount;
    uint binCount;
    uint clusterBudget;
};

StructuredBuffer<DenseGeometryCluster> clusters : register(t0);
StructuredBuffer<DenseGeometryPage> pages : register(t1);
RWStructuredBuffer<DrawIndexedArguments> indirectCommands : register(u0);
RWStructuredBuffer<uint> binCounts : register(u1);

bool sphereInFrustum(float3 center, float radius)
{
    [unroll]
    for (uint planeIndex = 0; planeIndex < 6; ++planeIndex)
    {
        const float4 plane = frustumPlanes[planeIndex];
        if (dot(plane.xyz, center) + plane.w < -radius)
        {
            return false;
        }
    }
    return true;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint clusterIndex = dispatchThreadId.x;
    if (clusterIndex >= clusterCount || clusterIndex >= clusterBudget)
    {
        return;
    }

    const DenseGeometryCluster cluster = clusters[clusterIndex];
    if (cluster.indexCount == 0 ||
        cluster.pageIndex >= pageCount ||
        cluster.binIndex >= binCount)
    {
        return;
    }

    const DenseGeometryPage page = pages[cluster.pageIndex];
    if (page.indexCount == 0)
    {
        return;
    }

    const float3 center = cluster.centerRadius.xyz;
    const float radius = max(cluster.centerRadius.w, 0.001f);
    const float maxDistance = cameraMaxDistance.w;
    if (maxDistance > 0.0f &&
        distance(center, cameraMaxDistance.xyz) - radius > maxDistance)
    {
        return;
    }
    if (!sphereInFrustum(center, radius))
    {
        return;
    }

    uint binSlot = 0;
    InterlockedAdd(binCounts[cluster.binIndex], 1, binSlot);

    DrawIndexedArguments command;
    command.indexCountPerInstance = cluster.indexCount;
    command.instanceCount = 1;
    command.startIndexLocation = cluster.firstIndex;
    command.baseVertexLocation = 0;
    command.startInstanceLocation = 0;
    indirectCommands[cluster.binCommandBase + binSlot] = command;
}
