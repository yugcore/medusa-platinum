// Matching integer hash and world-space fields in stylized_grass.cpp.
uint GrassHash(int2 p) {
    uint h = uint(p.x) * 0x8da6b343u ^ uint(p.y) * 0xd8163841u ^ 0xcb1ab31fu;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return h;
}
float GrassNoise(float2 p) {
    int2 i = int2(floor(p)); float2 f = frac(p); f = f*f*(3-2*f);
    return lerp(lerp(float(GrassHash(i) & 0xffffffu) / 16777215.,
                     float(GrassHash(i+int2(1,0)) & 0xffffffu) / 16777215., f.x),
                lerp(float(GrassHash(i+int2(0,1)) & 0xffffffu) / 16777215.,
                     float(GrassHash(i+1) & 0xffffffu) / 16777215., f.x), f.y);
}
float4 GrassFields(float2 p) {
    float2 warp = float2(GrassNoise(p*.033+17),GrassNoise(p*.033-31))-.5;
    float2 q = p + warp*14;
    float lush = GrassNoise(q*.025);
    float height = smoothstep(.35,.78,GrassNoise(q*.068+71))*smoothstep(.22,.55,lush);
    float bare = smoothstep(.64,.88,GrassNoise(q*.046-19));
    float density = lerp(.88,1.,lush)*(1-bare*.32);
    return float4(density,height,lush,bare);
}
