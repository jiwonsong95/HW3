#version 430

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_tex_coord;

uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_ModelViewMatrix;
uniform mat3 u_ModelViewMatrixInvTrans;
uniform mat4 u_ModelMatrix;

out vec3 FragPos;  
out vec3 Normal;   
out vec2 TexCoords;

void main()
{
    gl_Position = u_ModelViewProjectionMatrix * vec4(a_position, 1.0);

    vec4 posViewSpace = u_ModelViewMatrix * vec4(a_position, 1.0);
    FragPos = posViewSpace.xyz; 
    Normal  = normalize(u_ModelViewMatrixInvTrans * a_normal);
    TexCoords = a_tex_coord;
}
