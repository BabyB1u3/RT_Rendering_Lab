// ShadowDepth.slang
//
// Depth-only pass for shadow map generation.
// Transforms vertices by light-space MVP; fragment shader is empty.

[vk::binding(0, 0)]
cbuffer GlobalParams
{
    float4x4 u_LightViewProjection;
    float4x4 u_Model;
}

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
float4 vertexMain(VertexInput input) : SV_Position
{
    return mul(u_LightViewProjection, mul(u_Model, float4(input.position, 1.0)));
}

[shader("fragment")]
void fragmentMain()
{
}
