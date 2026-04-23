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
    float2 grid = floor(input.uv * 10.0);
    float checker = fmod(grid.x + grid.y, 2.0);
    float4 primary = float4(input.uv.x, input.uv.y, 1.0 - input.uv.x, 1.0);
    float4 accent = float4(1.0, 0.7, 0.2, 1.0);
    return lerp(primary, accent, checker * 0.35);
}
