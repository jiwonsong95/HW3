#version 430 core

layout (location = 0) in vec3 aPosition;
uniform mat4 u_sphereMVP;  

void main() {
    // ���Ǿ� ���� ��ǥ�� MVP ����
    gl_Position = u_sphereMVP * vec4(aPosition, 1.0);
}