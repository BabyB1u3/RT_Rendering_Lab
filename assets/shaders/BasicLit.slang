// BasicLit.slang
//
// Tutorial 06: Blinn-Phong lighting with a single directional light.
// No shadows, no texture — solid color albedo with ambient + diffuse + specular.
//
// Uniforms are packed into the implicit GlobalParams CB (binding 0) by Slang.
// The C++ side must upload a matching struct via SetUniformBlock(0, ...).

[vk::binding(0, 0)]
cbuffer GlobalParams
{
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4x4 u_NormalMatrix; // transpose(inverse(model)), passed as mat4

    float3 u_CameraPosition;
    float3 u_LightDirection; // points FROM light TOWARD surface
    float3 u_LightColor;
    float  u_LightIntensity;

    float3 u_Albedo;
    float  u_SpecularPower;
    float  u_AmbientStrength;
}

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 svPosition    : SV_Position;
    float3 worldPosition : WORLD_POS;
    float3 worldNormal   : WORLD_NORMAL;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    float4 worldPosition = mul(u_Model, float4(input.position, 1.0));

    VertexOutput output;
    output.svPosition    = mul(u_ViewProjection, worldPosition);
    output.worldPosition = worldPosition.xyz;
    output.worldNormal   = mul(float3x3(u_NormalMatrix), input.normal);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 normal   = normalize(input.worldNormal);
    float3 lightDir = normalize(u_LightDirection);

    // Ambient
    float3 ambient = u_AmbientStrength * u_Albedo;

    // Diffuse
    float NdotL = max(dot(normal, -lightDir), 0.0);
    float3 diffuse = NdotL * u_Albedo * u_LightColor * u_LightIntensity;

    // Blinn-Phong specular
    float3 viewDir = normalize(u_CameraPosition - input.worldPosition);
    float3 halfDir = normalize(viewDir + (-lightDir));
    float spec = pow(max(dot(normal, halfDir), 0.0), u_SpecularPower);
    float3 specular = spec * u_LightColor * u_LightIntensity;

    return float4(ambient + diffuse + specular, 1.0);
}
