struct VertexInput
{
    float2 position : POSITION;
    float4 color : COLOR0;
};

struct FrameParams
{
    float4x4 viewProj;
    float4 tint;
    float time;
};

struct MaterialParams
{
    float4 baseColor;
};

struct ObjectParams
{
    float4x4 model;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

ParameterBlock<FrameParams> gFrame : register(b0, space0);
ParameterBlock<MaterialParams> gMaterial : register(b0, space1);
ParameterBlock<ObjectParams> gObject : register(b0, space2);

[shader("vertex")]
VertexOutput main_vertex(VertexInput input)
{
    VertexOutput output;
    const float4 localPosition = float4(input.position, 0.0, 1.0);
    output.position = mul(gFrame.viewProj, mul(gObject.model, localPosition));
    output.color = input.color * gFrame.tint;
    return output;
}

[shader("fragment")]
float4 main_fragment(VertexOutput input) : SV_Target
{
    return input.color * gMaterial.baseColor;
}
