#version 330 core

in vec3 v_WorldNormal;
in vec3 v_WorldPos;
in vec2 v_TexCoord;

uniform sampler2D u_Albedo;
uniform vec3      u_SunDir;
uniform vec3      u_SunColor;
uniform vec3      u_AmbientColor;
uniform vec3      u_CameraPos;

// Simple specular for the low-poly shiny stone/metal aesthetic
uniform float u_Shininess;  // 0 = matte, 1 = shiny

out vec4 FragColor;

void main() {
    vec3 N     = normalize(v_WorldNormal);
    vec3 L     = normalize(u_SunDir);
    vec3 V     = normalize(u_CameraPos - v_WorldPos);
    vec3 H     = normalize(L + V);

    float NdL  = max(dot(N, L), 0.0);
    float NdH  = max(dot(N, H), 0.0);

    vec3 albedo   = texture(u_Albedo, v_TexCoord).rgb;
    vec3 diffuse  = albedo * u_SunColor * NdL;
    vec3 ambient  = albedo * u_AmbientColor;
    vec3 specular = u_SunColor * pow(NdH, 16.0) * u_Shininess * NdL;

    FragColor = vec4(diffuse + ambient + specular, 1.0);
}
