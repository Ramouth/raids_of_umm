#version 330 core

layout(location = 0) in vec2 a_Offset;    // billboard-local: x∈[-0.5,0.5], y∈[0,1]
layout(location = 1) in vec2 a_TexCoord;  // UV in [0,1]

uniform vec3  u_Center;   // world-space base (bottom-centre) of sprite
uniform float u_Size;     // size in world units
uniform mat4  u_View;
uniform mat4  u_Proj;

out vec2 v_TexCoord;

void main() {
    // Extract camera right and up from the view matrix (column-major GLM layout).
    // Row 0 of view = right vector, row 1 = up vector.
    vec3 camRight = vec3(u_View[0][0], u_View[1][0], u_View[2][0]);
    vec3 camUp    = vec3(u_View[0][1], u_View[1][1], u_View[2][1]);

    vec3 pos = u_Center
             + camRight * a_Offset.x * u_Size
             + camUp    * a_Offset.y * u_Size;

    gl_Position = u_Proj * u_View * vec4(pos, 1.0);

    // Flip V: PNG is stored top-to-bottom but OpenGL expects bottom-to-top.
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
}
