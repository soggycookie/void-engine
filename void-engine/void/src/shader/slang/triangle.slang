static float2 positions[3] =
    float2[](float2(0.0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5));

static float3 colors[3] = float3[](float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
                                   float3(0.0, 0.0, 1.0));

struct VSOutput
{
    float4 sv_position : SV_Position;
    float3 color;
};

struct VSInput
{
    float2 inPosition;
    float3 inColor;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.sv_position = float4(input.inPosition, 0.0, 1.0);
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target
{
    float3 color = inVert.color;
    return float4(color, 1.0);
}

