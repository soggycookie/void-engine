
struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv;
};

struct VSInput
{
    float3 inPosition;
    float2 inUV;
};

struct UniformBuffer
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;
Sampler2D texture;

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.uv = input.inUV;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    float2 uv = vertIn.uv;
    float4 tex = texture.Sample(uv);
    return tex;
}

