struct VertexInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
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

ConstantBuffer<FrameParams> gFrame : register(b0, space0);
ConstantBuffer<MaterialParams> gMaterial : register(b0, space1);
ConstantBuffer<ObjectParams> gObject : register(b0, space2);
Texture2D gAlbedoTexture : register(t0, space1);
SamplerState gAlbedoSampler : register(s0, space1);

[shader("vertex")]
VertexOutput main_vertex(VertexInput input)
{
    VertexOutput output;
    output.position = mul(gFrame.viewProj, mul(gObject.model, float4(input.position.xyz, 1.0)));
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 main_fragment(VertexOutput input) : SV_Target
{
    return gAlbedoTexture.Sample(gAlbedoSampler, input.uv) * gMaterial.baseColor * gFrame.tint;
}
