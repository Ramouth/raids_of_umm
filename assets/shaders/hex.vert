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
uniform vec3  u_TileColor;   // base colour tint
uniform float u_Height;      // Y extrusion (0 = flat desert floor)
uniform int   u_Rotation;    // 0-5: texture rotation in 60-degree steps (CW)

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
    // Rotate UV around the hex centre (0.5, 0.5) in 60-degree steps.
    vec2 uv = a_TexCoord - 0.5;
    if (u_Rotation > 0) {
        float angle = float(u_Rotation) * (3.14159265358979 / 3.0);
        float c = cos(angle), s = sin(angle);
        uv = vec2(c * uv.x - s * uv.y, s * uv.x + c * uv.y);
    }
    v_TexCoord    = uv + 0.5;
    v_Color       = u_TileColor;
    
    // Calculate distance from center for edge darkening effect
    v_EdgeDist = length(a_TexCoord - vec2(0.5));
}
