// Shared by opaque composition and forward/transparent PBR materials.
float2 OctEncode(float3 d){d/=abs(d.x)+abs(d.y)+abs(d.z);float2 p=d.xy;if(d.z<0)p=(1-abs(p.yx))*float2(p.x>=0?1:-1,p.y>=0?1:-1);return p*.5+.5;}
float3 Local(uint offset,float3 p){return float3(dot(volumeData[offset],float4(p,1)),dot(volumeData[offset+1],float4(p,1)),dot(volumeData[offset+2],float4(p,1)));}
float4 Directional(uint probe,float3 n) {
    uint2 pixel=uint2(probe%512,(probe/512)*8);float3 weights=n*n;
    float4 x=giGrid.Load(int3(pixel+uint2(0,n.x>=0?0:1),0));
    float4 y=giGrid.Load(int3(pixel+uint2(0,n.y>=0?2:3),0));
    float4 z=giGrid.Load(int3(pixel+uint2(0,n.z>=0?4:5),0));
    return x*weights.x+y*weights.y+z*weights.z;
}
float3 SampleGi(uint offset,float3 uvw,float3 p,float3 normal) {
    uint3 counts=uint3(volumeData[offset+5].xyz);uint first=uint(volumeData[offset+5].w);
    float3 grid=saturate(uvw)*(counts-1);uint3 cell=min(uint3(grid),counts-2);float3 f=grid-cell;
    float3 sum=0;float total=0;
    [unroll]for(uint i=0;i<8;++i){
        uint3 corner=uint3(i&1,(i>>1)&1,(i>>2)&1),q=cell+corner;
        uint index=first+q.x+counts.x*(q.y+counts.y*q.z);uint2 pixel=uint2(index%512,(index/512)*8);
        if(giGrid.Load(int3(pixel+uint2(0,7),0)).z<.5)continue;
        float3 localDelta=(uvw-float3(q)/(counts-1))*volumeData[offset+4].xyz;
        // Volume axes form an orthogonal frame. Convert the probe-to-surface
        // direction to world space for the directional distance moments.
        float3 d=0;
        [unroll]for(uint axis=0;axis<3;++axis)d+=normalize(volumeData[offset+axis].xyz)*localDelta[axis];
        float distance=length(d)*.01;float3 direction=normalize(d+float3(.00001,0,0));float4 moments=Directional(index,direction);
        float4 m0=giGrid.Load(int3(pixel+uint2(0,6),0)),m1=giGrid.Load(int3(pixel+uint2(0,7),0));
        float3 second=float3(direction.x>=0?m0.x:m0.y,direction.y>=0?m0.z:m0.w,direction.z>=0?m1.x:m1.y);
        float variance=max(dot(second,direction*direction)-moments.a*moments.a,.00001);
        float delta=max(distance-moments.a-.002,0);float visibility=variance/(variance+delta*delta);
        float3 interpolation=lerp(1-f,f,float3(corner));float weight=interpolation.x*interpolation.y*interpolation.z*visibility*visibility;
        weight*=max(.05,dot(-direction,normal)*.5+.5);
        sum+=Directional(index,normal).rgb*weight;total+=weight;
    }
    return sum/max(total,.00001);
}
float3 SampleProbe(uint offset,float3 p,float3 direction,float roughness,out float ready){
    float3 local=Local(offset,p),localDirection=float3(dot(volumeData[offset].xyz,direction),dot(volumeData[offset+1].xyz,direction),dot(volumeData[offset+2].xyz,direction));
    float3 safe=sign(localDirection)*max(abs(localDirection),.000001);safe+=float3(safe.x==0?.000001:0,safe.y==0?.000001:0,safe.z==0?.000001:0);
    float3 distances=(step(0,localDirection)-local)/safe;float t=min(distances.x,min(distances.y,distances.z));
    float3 r=normalize(p+direction*max(t,0)-volumeData[offset+3].xyz);
    uint resolution=uint(volumeData[offset+6].y),index=uint(volumeData[offset+6].x);uint w,h;localRadiance.GetDimensions(w,h);
    float3 tangent=normalize(cross(abs(r.y)>.99?float3(0,0,1):float3(0,1,0),r));float3 bitangent=cross(r,tangent);
    float3 sum=0;ready=0;
    // Roughness filtering in direction space avoids seams at octahedral folds.
    [unroll]for(uint tap=0;tap<16;++tap){float angle=tap*2.39996323;float radius=sqrt((tap+.5)/16)*roughness*roughness*2;
        float3 d=normalize(r+(tangent*cos(angle)+bitangent*sin(angle))*radius);float2 uv=OctEncode(d);
        uv=(clamp(uv*resolution,.5,resolution-.5)+float2(0,index*resolution))/float2(w,h);
        float4 value=localRadiance.SampleLevel(linearClamp,uv,0);sum+=value.rgb;ready+=value.a/16;
    }return sum/16;
}
