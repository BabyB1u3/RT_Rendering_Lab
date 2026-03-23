#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform mat4 u_LightViewProjection;

layout(location = 0) out vec3 v_WorldPosition;
layout(location = 1) out vec3 v_WorldNormal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out vec4 v_LightSpacePosition;

void main()
{
    vec4 worldPosition = u_Model * vec4(a_Position, 1.0);

    v_WorldPosition = worldPosition.xyz;
    v_WorldNormal = u_NormalMatrix * a_Normal;
    v_TexCoord = a_TexCoord;
    v_LightSpacePosition = u_LightViewProjection * worldPosition;

    gl_Position = u_ViewProjection * worldPosition;
}
