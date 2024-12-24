#version 430
in vec2 TexCoords;

uniform sampler2D gBufferTexture; // The G-buffer texture to debug
uniform int debugMode; // 0: Position, 1: Normal, 2: Albedo

out vec4 FragColor;

void main() {
    vec4 texValue = texture(gBufferTexture, TexCoords);

    if (debugMode == 0) { // Position
        FragColor = vec4(texValue.rgb * 0.001, 1.0); // Scale positions for visibility
    } else if (debugMode == 1) { // Normal
        FragColor = vec4(texValue.rgb * 0.5 + 0.5, 1.0); // Map [-1, 1] to [0, 1]
    } else if (debugMode == 2) { // Albedo
        FragColor = vec4(texValue.rgb, 1.0); // Albedo as-is
    } else {
        FragColor = vec4(0.0); // Fallback (shouldn't happen)
    }
}
