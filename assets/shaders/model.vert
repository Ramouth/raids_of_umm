#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_MVP;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

out vec3 v_WorldNormal;
out vec3 v_WorldPos;
out vec2 v_TexCoord;

void main() {
    gl_Position   = u_MVP * vec4(a_Position, 1.0);
    v_WorldNormal = u_NormalMatrix * a_Normal;
    v_WorldPos    = (u_Model * vec4(a_Position, 1.0)).xyz;
    v_TexCoord    = a_TexCoord;
}
