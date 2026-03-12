#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform mat4 u_LightViewProjection;

out vec3 v_WorldPosition;
out vec3 v_WorldNormal;
out vec2 v_TexCoord;
out vec4 v_LightSpacePosition;

void main()
{
    vec4 worldPosition = u_Model * vec4(a_Position, 1.0);

    v_WorldPosition = worldPosition.xyz;
    v_WorldNormal = u_NormalMatrix * a_Normal;
    v_TexCoord = a_TexCoord;
    v_LightSpacePosition = u_LightViewProjection * worldPosition;

    gl_Position = u_ViewProjection * worldPosition;
}

#type fragment
#version 460 core

in vec3 v_WorldPosition;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;
in vec4 v_LightSpacePosition;

uniform sampler2D u_ShadowMap;
uniform sampler2D u_AlbedoMap;

uniform bool u_UseAlbedoMap;
uniform vec3 u_CameraPosition;
uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;

out vec4 FragColor;

float ComputeShadow(vec4 lightSpacePosition, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = lightSpacePosition.xyz / lightSpacePosition.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.0005, 0.005 * (1.0 - dot(normalize(normal), normalize(-lightDir))));

    // 3x3 PCF for soft shadow edges
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main()
{
    vec3 normal = normalize(v_WorldNormal);
    vec3 lightDir = normalize(u_LightDirection);

    vec3 albedo = vec3(0.85);
    if (u_UseAlbedoMap)
        albedo = texture(u_AlbedoMap, v_TexCoord).rgb;

    float NdotL = max(dot(normal, -lightDir), 0.0);
    float shadow = ComputeShadow(v_LightSpacePosition, normal, lightDir);

    vec3 ambient = 0.15 * albedo;
    vec3 diffuse = (1.0 - shadow) * NdotL * albedo * u_LightColor * u_LightIntensity;

    FragColor = vec4(ambient + diffuse, 1.0);
}