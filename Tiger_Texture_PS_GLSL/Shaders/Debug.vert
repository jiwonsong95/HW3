#version 430
layout(location = 0) in vec2 quadPos; // Fullscreen quad positions
layout(location = 1) in vec2 quadTexCoord; // Texture coordinates

out vec2 TexCoords;

void main() {
    TexCoords = quadTexCoord;
    gl_Position = vec4(quadPos.xy, 0.0, 1.0);
}
