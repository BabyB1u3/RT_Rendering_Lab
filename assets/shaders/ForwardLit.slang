// ForwardLit.slang
//
// Forward shading pass: Blinn-Phong lighting with PCF shadow sampling.
// Migrated from ForwardLit.vert + ForwardLit.frag (GLSL 460).

// --- Samplers (remain as plain uniforms in GLSL output) ---
uniform Sampler2D u_ShadowMap;
uniform Sampler2D u_AlbedoMap;

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

float ComputeShadow(float4 lightSpacePosition, float3 normal, float3 lightDir)
{
    float3 projCoords = lightSpacePosition.xyz / lightSpacePosition.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.0001, 0.002 * (1.0 - dot(normalize(normal), normalize(-lightDir))));

    // 3x3 PCF for soft shadow edges
    float shadow = 0.0;
    int2 texSize;
    u_ShadowMap.GetDimensions(texSize.x, texSize.y);
    float2 texelSize = 1.0 / float2(texSize);

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = u_ShadowMap.Sample(projCoords.xy + float2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 normal   = normalize(input.worldNormal);
    float3 lightDir = normalize(u_LightDirection);

    float3 albedo = u_Albedo;
    if (u_UseAlbedoMap)
        albedo *= u_AlbedoMap.Sample(input.texCoord).rgb;

    float NdotL  = max(dot(normal, -lightDir), 0.0);
    float shadow = ComputeShadow(input.lightSpacePosition, normal, lightDir);

    float3 ambient = u_AmbientStrength * albedo;
    float3 diffuse = (1.0 - shadow) * NdotL * albedo * u_LightColor * u_LightIntensity;

    // Blinn-Phong specular
    float3 viewDir = normalize(u_CameraPosition - input.worldPosition);
    float3 halfDir = normalize(viewDir + (-lightDir));
    float spec     = pow(max(dot(normal, halfDir), 0.0), u_SpecularPower);
    float3 specular = (1.0 - shadow) * spec * u_LightColor * u_LightIntensity;

    return float4(ambient + diffuse + specular, 1.0);
}
