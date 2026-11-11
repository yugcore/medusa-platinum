// Analytic slice integration of the cosine-weighted visible hemisphere.
// Mathematical reference: Jimenez et al., GTAO (2016), also documented by
// Intel XeGTAO: https://github.com/GameTechDev/XeGTAO .
#include "ssao.hlsl"
float Arc(float h,float n){return (cos(n)+2*h*sin(n)-cos(2*h-n))*.25;}
float PSGtao(PSInput input) : SV_TARGET {
    float z=depthTex.SampleLevel(pointSampler,input.uv,0);
    if(z>=.999999 || aoIntensity<=0 || aoRadius<=0)return 1;
    float3 p=ReconstructWorldPosition(input.uv,z);
    float3 n=normalize(normalTex.SampleLevel(pointSampler,input.uv,0).xyz);
    if(aoPadding.z<.5)n=normalize(n*2-1);
    float pixelRadius=clamp(aoRadius*projectionScale/max(length(p),.05),2,96);
    float visibility=0;float3 v=normalize(-p);
    [unroll]for(int direction=0;direction<6;++direction) {
        float angle=(direction+.5)*3.14159265/6;
        float2 screenAxis=float2(cos(angle),sin(angle));
        float3 tangent=ReconstructWorldPosition(input.uv+screenAxis*inverseViewportSize,z)-p;
        tangent=normalize(tangent-v*dot(tangent,v));
        float3 axis=normalize(cross(tangent,v)),projected=n-axis*dot(n,axis);
        float projectedLength=max(length(projected),.00001);
        float normalAngle=sign(dot(tangent,projected))*acos(saturate(dot(projected,v)/projectedLength));
        float2 horizons=float2(cos(normalAngle+1.570796327),cos(normalAngle-1.570796327));
        [unroll]for(int side=0;side<2;++side) {
            [unroll]for(int step=1;step<=6;++step) {
                float2 uv=(floor((input.uv+screenAxis*(side==0?1:-1)*(step*step/36.0)*pixelRadius*inverseViewportSize)*viewportSize)+.5)*inverseViewportSize;
                if(any(uv<=0)||any(uv>=1))continue;
                float depth=depthTex.SampleLevel(pointSampler,uv,0);if(depth>=.999999)continue;
                float3 delta=ReconstructWorldPosition(uv,depth)-p;float distance=length(delta);
                if(distance<=.0001||distance>=aoRadius)continue;
                float horizon=dot(delta/distance,v);
                float falloff=saturate((distance-aoRadius*.5)/(aoRadius*.5));
                horizons[side]=max(horizons[side],lerp(horizon,horizons[side],falloff)-aoBias/max(distance,.01));
            }
        }
        float h0=-acos(clamp(horizons.y,-1,1)),h1=acos(clamp(horizons.x,-1,1));
        h0=normalAngle+clamp(h0-normalAngle,-1.570796327,1.570796327);
        h1=normalAngle+clamp(h1-normalAngle,-1.570796327,1.570796327);
        visibility+=projectedLength*(Arc(h0,normalAngle)+Arc(h1,normalAngle));
    }
    return max(pow(saturate(1-(1-saturate(visibility/6))*aoIntensity),max(aoPower,.25)),saturate(aoMinVisibility));
}
