#version 330 core

// Per-vertex attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

// Uniforms
uniform mat4 u_MVP;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

// Per-instance (hex tile) data passed as uniforms for now;
// switch to instanced arrays when tile count grows large.
uniform vec3 u_TileColor;   // base colour tint
uniform float u_Height;     // Y extrusion (0 = flat desert floor)

out vec3 v_WorldNormal;
out vec3 v_WorldPos;
out vec2 v_TexCoord;
out vec3 v_Color;
out float v_EdgeDist;  // distance from hex center (for edge darkening)

void main() {
    vec3 pos = a_Position;
    pos.y += u_Height;

    gl_Position  = u_MVP * vec4(pos, 1.0);
    v_WorldNormal = u_NormalMatrix * a_Normal;
    v_WorldPos    = (u_Model * vec4(pos, 1.0)).xyz;
    v_TexCoord    = a_TexCoord;
    v_Color       = u_TileColor;
    
    // Calculate distance from center for edge darkening effect
    v_EdgeDist = length(a_TexCoord - vec2(0.5));
}
