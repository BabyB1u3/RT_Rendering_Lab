// FlatColor.slang
//
// Tutorial 02: vertex color pass-through shader.
// Accepts position and color per vertex, outputs interpolated color.
// No MVP transform — vertices are expected in NDC coordinates.

struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 svPosition : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.svPosition = float4(input.position, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
