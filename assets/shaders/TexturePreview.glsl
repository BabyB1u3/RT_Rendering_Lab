#type vertex
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);
}

#type fragment
#version 460 core

in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform bool u_IsDepthTexture;

out vec4 FragColor;

void main()
{
    if (u_IsDepthTexture)
    {
        float depth = texture(u_Texture, v_TexCoord).r;
        FragColor = vec4(vec3(depth), 1.0);
    }
    else
    {
        FragColor = texture(u_Texture, v_TexCoord);
    }
}