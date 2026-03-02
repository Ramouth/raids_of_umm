#version 330 core

in vec3 v_WorldNormal;
in vec3 v_WorldPos;
in vec2 v_TexCoord;
in vec3 v_Color;

uniform sampler2D u_Texture;
uniform vec3      u_SunDir;     // normalised, world-space
uniform vec3      u_SunColor;   // e.g. (1.0, 0.92, 0.7) warm desert sun
uniform vec3      u_AmbientColor;

out vec4 FragColor;

void main() {
    vec3 N    = normalize(v_WorldNormal);
    float NdL = max(dot(N, normalize(u_SunDir)), 0.0);

    vec3 texColor = texture(u_Texture, v_TexCoord).rgb;
    vec3 albedo   = texColor * v_Color;

    vec3 diffuse  = albedo * u_SunColor * NdL;
    vec3 ambient  = albedo * u_AmbientColor;

    FragColor = vec4(diffuse + ambient, 1.0);
}
