#version 330 core

in vec3 v_WorldNormal;
in vec3 v_WorldPos;
in vec2 v_TexCoord;
in vec3 v_Color;
in float v_EdgeDist;

uniform sampler2D u_Texture;
uniform vec3      u_SunDir;     // normalised, world-space
uniform vec3      u_SunColor;   // e.g. (1.0, 0.92, 0.7) warm desert sun
uniform vec3      u_AmbientColor;
uniform float     u_FogDensity; // 0.0 = no fog
uniform vec3      u_FogColor;   // e.g. (0.76, 0.69, 0.50) dusty desert air
uniform vec3      u_CameraPos;  // for fog distance calculation

out vec4 FragColor;

void main() {
    vec3 N    = normalize(v_WorldNormal);
    float NdL = max(dot(N, normalize(u_SunDir)), 0.0);

    vec3 texColor = texture(u_Texture, v_TexCoord).rgb;
    vec3 albedo   = texColor * v_Color;

    // Lambertian diffuse
    vec3 diffuse  = albedo * u_SunColor * NdL;
    
    // Hemisphere ambient (sky vs ground bounce)
    vec3 skyColor = u_AmbientColor * 1.2;
    vec3 groundColor = u_AmbientColor * 0.4;
    float hemisphere = N.y * 0.5 + 0.5; // 0 at horizon, 1 at top
    vec3 ambient = albedo * mix(groundColor, skyColor, hemisphere);

    vec3 color = diffuse + ambient;
    
    // Edge darkening - darken hex edges for better tile definition
    float edgeFactor = smoothstep(0.35, 0.5, v_EdgeDist);
    color = mix(color, color * 0.6, edgeFactor * 0.5);
    
    // Distance fog
    float dist = length(v_WorldPos - u_CameraPos);
    float fogFactor = 1.0 - exp(-u_FogDensity * dist);
    fogFactor = clamp(fogFactor, 0.0, 0.7); // cap max fog
    color = mix(color, u_FogColor, fogFactor);

    FragColor = vec4(color, 1.0);
}
