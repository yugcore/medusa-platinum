// Adapted from tuxalin/water-shader, Copyright (c) 2017 tuxalin.
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#ifndef ZE_WATER_OPTICS_HLSLI
#define ZE_WATER_OPTICS_HLSLI
static const float ZE_WATER_PI=3.14159265359;
float2 ZeWaterDirection(float2 wind) {return dot(wind,wind)>1e-8?normalize(wind):float2(1,0);}
float ZeWaterFresnel(float cosine) {return .02037+.97963*pow(1-saturate(cosine),5);}
// Four sine/Gerstner components, in metres. Analytic derivatives avoid extra
// height-map fetches. Screen footprint suppresses unresolved short waves.
void ZeWaterWaves(float2 p,float time,float2 wind,float scale,float footprint,
                  out float height,out float2 gradient) {
    float2 direction=ZeWaterDirection(wind),side=float2(-direction.y,direction.x);
    const float4 lengths=float4(18,9,4.5,2.3)*(.055/max(.001,scale));
    const float4 amplitudes=float4(.065,.045,.025,.012);
    height=0;gradient=0;
    [unroll] for(int i=0;i<4;++i) {
        float angle=float(i)*1.17-.45;
        float2 d=direction*cos(angle)+side*sin(angle);
        float k=2*ZE_WATER_PI/lengths[i];
        float phase=dot(p,d)*k-time*sqrt(9.81*k)*length(wind);
        float filter=1-smoothstep(.35,1.5,footprint*k);
        height+=sin(phase)*amplitudes[i]*filter;
        gradient+=cos(phase)*amplitudes[i]*k*d*filter;
    }
    height/=.147;
}
float3 ZeWaterDisplacement(float2 p,float time,float2 wind,float scale,float steepness) {
    float2 direction=ZeWaterDirection(wind),side=float2(-direction.y,direction.x);
    const float4 lengths=float4(18,9,4.5,2.3)*(.055/max(.001,scale));
    const float4 amplitudes=float4(.065,.045,.025,.012);
    float3 displacement=0;
    [unroll]for(int i=0;i<4;++i) {
        float angle=float(i)*1.17-.45;
        float2 d=direction*cos(angle)+side*sin(angle);
        float k=2*ZE_WATER_PI/lengths[i],phase=dot(p,d)*k-time*sqrt(9.81*k)*length(wind);
        displacement.y+=amplitudes[i]*sin(phase);
        // Two Gerstner components plus two sine components, as in the source.
        if(i<2)displacement.xz+=d*(saturate(steepness)*amplitudes[i]*cos(phase));
    }
    return displacement/.147;
}
// Physical extinction plus the reference shader's separate vertical-depth tint
// and eye-path clarity. Return the transmission separately for correct fogging.
float3 ZeWaterTransmission(float verticalDepth,float rayDepth,float3 absorption,
                          float clarity,float visibility) {
    return exp(-max(absorption,.001)*max(rayDepth,0)) *
        exp(-max(clarity,0)*max(rayDepth,0)/max(visibility,.1));
}
float ZeWaterSunSpecular(float3 N,float3 V,float3 L,float roughness) {
    float3 halfVector=V+L;
    if(dot(halfVector,halfVector)<1e-8)return 0;
    float3 H=normalize(halfVector);
    float NoV=max(.02,saturate(dot(N,V))),NoL=saturate(dot(N,L)),NoH=saturate(dot(N,H));
    float a=max(.04,roughness*roughness),a2=a*a;
    float denominator=NoH*NoH*(a2-1)+1;
    float D=a2/max(ZE_WATER_PI*denominator*denominator,1e-6);
    float k=(roughness+1)*(roughness+1)*.125;
    float Gv=NoV/(NoV*(1-k)+k),Gl=NoL/max(NoL*(1-k)+k,1e-5);
    return min(.5,D*Gv*Gl*ZeWaterFresnel(dot(H,V))/(4*NoV));
}
#endif
