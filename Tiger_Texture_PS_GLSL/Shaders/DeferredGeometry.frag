#version 430

in vec3 FragPos;
in vec3 Normal; 
in vec2 TexCoords;

layout (location = 0) out vec4 outPosition; 
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

uniform sampler2D u_base_texture;

void main()
{
    outPosition = vec4(FragPos, 1.0);

    outNormal   = vec4(normalize(Normal), 0.0);

    vec4 texColor = texture(u_base_texture, TexCoords);
    outAlbedo = texColor; 
}