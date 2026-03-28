// TexturePreview.slang
//
// Fullscreen quad shader for visualizing textures (color or depth).
// First shader migrated from GLSL to Slang.
//
// Uses Sampler2D (combined texture+sampler) for OpenGL-compatible GLSL output.

// Binding convention (matches [vk::binding(N, 0)] → consistent across OpenGL/Metal/Vulkan):
//   binding 0 : explicit GlobalParams CB
//   binding 1 : u_Texture
[vk::binding(0, 0)]
cbuffer GlobalParams
{
    bool u_IsDepthTexture;
}

[vk::binding(1, 0)] uniform Sampler2D u_Texture;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
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
    output.texCoord   = input.texCoord;
    output.svPosition = float4(input.position.xy, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    if (u_IsDepthTexture)
    {
        float depth = u_Texture.Sample(input.texCoord).r;
        return float4(depth, depth, depth, 1.0);
    }
    return u_Texture.Sample(input.texCoord);
}
