struct VertexInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D gOffscreenTexture : register(t0, space0);
SamplerState gLinearSampler : register(s0, space0);

[shader("vertex")]
VertexOutput main_vertex(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 main_fragment(VertexOutput input) : SV_Target
{
    return gOffscreenTexture.Sample(gLinearSampler, input.uv);
}
