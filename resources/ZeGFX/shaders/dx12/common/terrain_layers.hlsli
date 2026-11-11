// Generic three-layer PBR terrain. UV carries normalized layer weights;
// vertex alpha carries authored coverage. Unassigned coverage retains color.
float2 ZeTerrainHash(float2 p) {
    p+=terrainVariationParams.z*float2(.017,.031);
    float3 q=frac(float3(p.xyx)*float3(.1031,.1030,.0973));
    q+=dot(q,q.yzx+33.33);
    return frac((q.xx+q.yz)*q.zy);
}
float ZeTerrainNoise(float2 p) {
    float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
    return lerp(lerp(ZeTerrainHash(i).x,ZeTerrainHash(i+float2(1,0)).x,f.x),
                lerp(ZeTerrainHash(i+float2(0,1)).x,ZeTerrainHash(i+1).x,f.x),f.y);
}
float2 ZeTerrainAtlasUv(float2 uv,int layer) {
    float border=terrainSurfaceParams.z;
    float cells=terrainVariationParams.w>3.5?4:2;
    return float2(layer%int(cells),layer/int(cells))/cells + border + frac(uv)*(1/cells-2*border);
}
void ZeTerrainSurface(PSInput input,inout float4 albedo,inout float3 N,
    inout float rough,inout float metal,inout float ao) {
    int layerCount=terrainVariationParams.w>3.5?int(terrainVariationParams.w):3;
    float cells=layerCount>3?4:2;
    float coverage=layerCount>3?1:saturate(input.color.a);
    float weights[8]={input.uv.x,input.uv.y,max(0,1-input.uv.x-input.uv.y),0,0,0,0,0};
    if(layerCount>3){weights[2]=input.uv1.x;weights[3]=input.uv1.y;
        weights[4]=input.color.r;weights[5]=input.color.g;weights[6]=input.color.b;weights[7]=input.color.a;}
    float weightSum=0;[unroll]for(int i=0;i<8;++i){weights[i]=max(weights[i],0);weightSum+=weights[i];}
    [unroll]for(int j=0;j<8;++j)weights[j]/=max(weightSum,1e-5);
    float3 geometric=normalize(input.worldNormal);
    float3 projection=pow(abs(geometric),4);
    projection/=max(dot(projection,1),1e-5);
    float3 signs=float3(geometric.x>=0?1:-1,geometric.y>=0?1:-1,geometric.z>=0?1:-1);
    float3 p=input.worldPos/max(terrainSurfaceParams.x,.25);
    uint width,height,levels;baseColorTex.GetDimensions(0,width,height,levels);
    float meanLevel=max(0,float(levels)-2);
    float3 color=0,normal=0,orm=0;
    [unroll] for(int axis=0;axis<3;++axis) {
        if(projection[axis]<.0001)continue;
        float2 uv=axis==0?float2(-p.z*signs.x,p.y):
                  axis==1?float2(p.x,-p.z*signs.y):float2(p.x*signs.z,p.y);
        // Three translated copies blend on a continuous triangular lattice.
        // Identical transforms apply to all PBR maps; no tile boundary seams.
        float2 grid=float2(uv.x-uv.y*.57735027,uv.y*1.15470054)*.45;
        float2 cell=floor(grid),f=frac(grid);
        bool upper=f.x+f.y>1;
        float2 corners[3];
        corners[0]=cell+(upper?float2(1,1):float2(0,0));
        corners[1]=cell+float2(1,0);corners[2]=cell+float2(0,1);
        float3 blends=upper?float3(f.x+f.y-1,1-f.y,1-f.x):float3(1-f.x-f.y,f.x,f.y);
        blends=pow(max(blends,0),3);blends/=max(dot(blends,1),1e-5);
        int sampleCount=terrainSurfaceParams.w>0?3:1;
        // Different atlas cells have periodic padding. Cap filter footprints so
        // coarse mips cannot blend unrelated materials across cell boundaries.
        float scale=1/cells-2*terrainSurfaceParams.z;
        float footprint=max(length(ddx(uv)),length(ddy(uv)))*float(width)*scale;
        float farFade=smoothstep(32,192,footprint);
        float2 gx=ddx(uv)*scale,gy=ddy(uv)*scale;
        float limit=terrainSurfaceParams.z*.25;
        gx*=min(1,limit/max(length(gx),1e-6));
        gy*=min(1,limit/max(length(gy),1e-6));
        [loop] for(int layer=0;layer<layerCount;++layer) {
            float weight=projection[axis]*weights[layer];
            if(weight<.0001)continue;
            float2 center=(float2(layer%int(cells),layer/int(cells))+.5)/cells;
            float3 layerColor=0,layerOrm=0,n=0;
            [loop] for(int tap=0;tap<sampleCount;++tap) {
                float blend=sampleCount==1?1:blends[tap];
                float2 offset=ZeTerrainHash(corners[tap])*terrainSurfaceParams.w*7;
                float2 atlasUv=ZeTerrainAtlasUv(uv+offset,layer);
                layerColor+=blend*baseColorTex.SampleGrad(anisoSampler,atlasUv,gx,gy).rgb;
                layerOrm+=blend*metallicRoughnessTex.SampleGrad(anisoSampler,atlasUv,gx,gy).rgb;
                n+=blend*(normalMapTex.SampleGrad(anisoSampler,atlasUv,gx,gy).xyz*2-1);
            }
            color+=weight*lerp(layerColor,baseColorTex.SampleLevel(clampSampler,center,meanLevel).rgb,farFade);
            orm+=weight*lerp(layerOrm,metallicRoughnessTex.SampleLevel(clampSampler,center,meanLevel).rgb,farFade);
            n.xy*=terrainSurfaceParams.y*(1-farFade);n=normalize(n);
            float3 world=axis==0?float3(n.z*signs.x,n.y,-n.x*signs.x):
                         axis==1?float3(n.x,n.z*signs.y,-n.y*signs.y):
                                 float3(n.x*signs.z,n.y,n.z*signs.z);
            float3 axisNormal=axis==0?float3(signs.x,0,0):
                              axis==1?float3(0,signs.y,0):float3(0,0,signs.z);
            normal+=weight*(world-axisNormal);
        }
    }
    // Broad seed-stable patches survive the mip fade into distant terrain.
    float2 macro=input.worldPos.xz/max(terrainVariationParams.x,1);
    float patch=.7*ZeTerrainNoise(macro)+.3*ZeTerrainNoise(macro*2.17+19);
    color*=1+(patch*2-1)*terrainVariationParams.y;
    albedo.rgb=lerp(pow(saturate(albedo.rgb),2.2),color,coverage);albedo.a=1;
    N=normalize(geometric+normal*coverage);
    rough=lerp(rough,clamp(orm.g,.08,1),coverage);
    metal=lerp(metal,saturate(orm.b),coverage);
    ao=lerp(ao,saturate(orm.r),coverage);
}
