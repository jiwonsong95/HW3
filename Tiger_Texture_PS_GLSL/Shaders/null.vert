#version 430 core

layout (location = 0) in vec3 aPosition;
uniform mat4 u_sphereMVP;  

void main() {
    // 스피어 정점 좌표에 MVP 적용
    gl_Position = u_sphereMVP * vec4(aPosition, 1.0);
}