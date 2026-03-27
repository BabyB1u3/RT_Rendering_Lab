// UnlitTextured.slang
//
// Tutorial 03: unlit textured quad shader.
// Accepts position and UV per vertex, samples a single texture.
// No MVP transform — vertices are expected in NDC coordinates.

[vk::binding(1, 0)] uniform Sampler2D u_Texture;

struct VertexInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 svPosition : SV_Position;
    float2 texCoord   : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.svPosition = float4(input.position, 1.0);
    output.texCoord   = input.texCoord;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return u_Texture.Sample(input.texCoord);
}
