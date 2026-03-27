// UnlitTransformed.slang
//
// Tutorial 04: unlit textured mesh with MVP transform.
// Accepts position, normal, and UV per vertex. Applies a Model-View-Projection
// matrix and samples a single texture. Normal is passed through but unused -
// keeps the vertex layout compatible with MeshFactory primitives.

uniform float4x4 u_MVP;

[vk::binding(1, 0)]
uniform Sampler2D u_Texture;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 svPosition : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.svPosition = mul(u_MVP, float4(input.position, 1.0));
    output.texCoord = input.texCoord;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return u_Texture.Sample(input.texCoord);
}
