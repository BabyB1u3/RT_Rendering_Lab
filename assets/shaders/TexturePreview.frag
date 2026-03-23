#version 460 core

layout(location = 0) in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform bool u_IsDepthTexture;

layout(location = 0) out vec4 FragColor;

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
