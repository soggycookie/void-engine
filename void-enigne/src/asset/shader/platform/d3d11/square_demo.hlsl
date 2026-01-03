struct VS_IN
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_OUT VSMain(VS_IN input)
{
    VS_OUT o;
    o.pos = input.pos;
    o.uv = input.uv;
    return o;
}

float4 PSMain(VS_OUT input) : SV_TARGET
{
    return float4(input.uv, 1, 1);
}