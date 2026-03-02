#version 330 core

// 2D UI overlay — NDC coordinates passed directly (no matrix needed for now)
layout(location = 0) in vec2 a_Position;  // screen [0,1] range
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;

uniform vec2 u_ScreenSize; // (width, height) in pixels

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    // Convert from pixel coords to NDC
    vec2 ndc = (a_Position / u_ScreenSize) * 2.0 - 1.0;
    ndc.y    = -ndc.y; // flip Y: pixel 0 is top, NDC +1 is top
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_TexCoord  = a_TexCoord;
    v_Color     = a_Color;
}
