#version 430

layout (location = 0) in vec2 quadPos;
layout (location = 1) in vec2 quadTexCoord;

out vec2 TexCoords;

void main()
{
    TexCoords = quadTexCoord;
    gl_Position = vec4(quadPos, 0.0, 1.0);
}