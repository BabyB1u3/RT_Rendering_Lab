// ForwardLit.slang
//
// Forward shading pass: Blinn-Phong lighting with PCF shadow sampling.
// Migrated from ForwardLit.vert + ForwardLit.frag (GLSL 460).

import shadow;

// Binding convention (matches [vk::binding(N, 0)] → consistent across OpenGL/Metal/Vulkan):
//   binding 0 : implicit GlobalParams CB  (all non-opaque uniforms, packed by Slang)
//   binding 1 : u_ShadowMap
//   binding 2 : u_AlbedoMap
[vk::binding(1, 0)] uniform Sampler2D u_ShadowMap;
[vk::binding(2, 0)] uniform Sampler2D u_AlbedoMap;

// --- Non-opaque uniforms (packed into a UBO by Slang) ---
uniform float4x4 u_ViewProjection;
uniform float4x4 u_Model;
uniform float4x4 u_NormalMatrix;   // transpose(inverse(model)), computed on CPU, passed as mat4
uniform float4x4 u_LightViewProjection;

uniform float3 u_CameraPosition;
uniform float3 u_LightDirection;
uniform float3 u_LightColor;
uniform float  u_LightIntensity;

uniform float3 u_Albedo;
uniform float  u_SpecularPower;
uniform float  u_AmbientStrength;
uniform bool   u_UseAlbedoMap;
uniform float2 u_ShadowMapTexelSize;

// --- Vertex stage ---

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 svPosition         : SV_Position;
    float3 worldPosition      : WORLD_POS;
    float3 worldNormal        : WORLD_NORMAL;
    float2 texCoord           : TEXCOORD0;
    float4 lightSpacePosition : LIGHT_POS;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    float4 worldPosition = mul(u_Model, float4(input.position, 1.0));

    VertexOutput output;
    output.worldPosition      = worldPosition.xyz;
    output.worldNormal        = mul(float3x3(u_NormalMatrix), input.normal);
    output.texCoord           = input.texCoord;
    output.lightSpacePosition = mul(u_LightViewProjection, worldPosition);
    output.svPosition         = mul(u_ViewProjection, worldPosition);
    return output;
}

// --- Fragment stage ---

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 normal   = normalize(input.worldNormal);
    float3 lightDir = normalize(u_LightDirection);

    float3 albedo = u_Albedo;
    if (u_UseAlbedoMap)
        albedo *= u_AlbedoMap.Sample(input.texCoord).rgb;

    float NdotL  = max(dot(normal, -lightDir), 0.0);
    float shadow = ComputeShadow(
        u_ShadowMap,
        u_ShadowMapTexelSize,
        input.lightSpacePosition,
        normal,
        lightDir);

    float3 ambient = u_AmbientStrength * albedo;
    float3 diffuse = (1.0 - shadow) * NdotL * albedo * u_LightColor * u_LightIntensity;

    // Blinn-Phong specular
    float3 viewDir = normalize(u_CameraPosition - input.worldPosition);
    float3 halfDir = normalize(viewDir + (-lightDir));
    float spec     = pow(max(dot(normal, halfDir), 0.0), u_SpecularPower);
    float3 specular = (1.0 - shadow) * spec * u_LightColor * u_LightIntensity;

    return float4(ambient + diffuse + specular, 1.0);
}
