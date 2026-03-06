#version 330 core

in vec3 v_WorldNormal;
in vec3 v_WorldPos;
in vec2 v_TexCoord;
in vec3 v_Color;
in float v_EdgeDist;

uniform sampler2D u_Texture;
uniform int       u_Textured;     // 1 = pixel-art texture, 0 = solid colour tile
uniform vec3      u_SunDir;
uniform vec3      u_SunColor;
uniform vec3      u_AmbientColor;
uniform float     u_FogDensity;
uniform vec3      u_FogColor;
uniform vec3      u_CameraPos;

out vec4 FragColor;

void main() {
    vec3 texColor = texture(u_Texture, v_TexCoord).rgb;
    vec3 color;

    if (u_Textured != 0) {
        // ── Pixel-art terrain tile ────────────────────────────────────────────
        // Use the authored texture colors directly — the artist-lit pixel art
        // already encodes its own shading. Diffuse lighting would shift the
        // carefully chosen palette and wash out the art.
        color = texColor * v_Color;

        // Very faint hex-edge darkening — just enough to read tile boundaries
        // without overwriting the pixel art edge detail.
        float edgeFactor = smoothstep(0.44, 0.50, v_EdgeDist);
        color = mix(color, color * 0.70, edgeFactor * 0.30);

    } else {
        // ── Solid-colour (no texture) tile — full lighting model ──────────────
        vec3 N    = normalize(v_WorldNormal);
        float NdL = max(dot(N, normalize(u_SunDir)), 0.0);

        vec3 albedo      = texColor * v_Color;
        vec3 diffuse     = albedo * u_SunColor * NdL;

        vec3 skyColor    = u_AmbientColor * 1.2;
        vec3 groundColor = u_AmbientColor * 0.4;
        float hemisphere = N.y * 0.5 + 0.5;
        vec3 ambient     = albedo * mix(groundColor, skyColor, hemisphere);

        color = diffuse + ambient;

        float edgeFactor = smoothstep(0.35, 0.5, v_EdgeDist);
        color = mix(color, color * 0.6, edgeFactor * 0.5);
    }

    // Distance fog
    float dist      = length(v_WorldPos - u_CameraPos);
    float fogFactor = clamp(1.0 - exp(-u_FogDensity * dist), 0.0, 0.6);
    color = mix(color, u_FogColor, fogFactor);

    FragColor = vec4(color, 1.0);
}
