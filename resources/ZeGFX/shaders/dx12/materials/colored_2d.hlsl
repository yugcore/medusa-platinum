cbuffer FrameConstants : register(b0)
{
    float viewportWidth;
    float viewportHeight;
    float padding0;
    float padding1;
};

struct VSInput
{
    float2 pos : POSITION;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float2 ndc;
    ndc.x = (input.pos.x / viewportWidth) * 2.0f - 1.0f;
    ndc.y = 1.0f - (input.pos.y / viewportHeight) * 2.0f;
    output.pos = float4(ndc, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
