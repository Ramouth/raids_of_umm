#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform bool      u_UseTexture;
uniform vec4      u_Color;

out vec4 FragColor;

void main() {
    if (u_UseTexture) {
        FragColor = texture(u_Texture, v_TexCoord) * v_Color;
    } else {
        FragColor = u_Color;
    }
}
