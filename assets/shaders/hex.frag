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
    vec4 texSample = texture(u_Texture, v_TexCoord);
    vec3 texColor  = texSample.rgb;
    vec3 color;

    if (u_Textured != 0) {
        // ── Pixel-art terrain tile ────────────────────────────────────────────
        // Alpha-blend texture over the solid base terrain colour.
        // Opaque textures (sand, rock — alpha=1.0 everywhere) behave exactly
        // as before.  Transparent overlay textures (grass, forest) let the
        // solid terrain colour show through the clear areas, giving a softer
        // "painted over" look without hard pixel edges.
        // Blend: transparent areas show base terrain colour; opaque areas show texture as-is.
        color = mix(v_Color, texColor, texSample.a);

        // Very faint hex-edge darkening — just enough to read tile boundaries
        // without overwriting the pixel art edge detail.
        float edgeFactor = smoothstep(0.44, 0.50, v_EdgeDist);
        color = mix(color, color * 0.70, edgeFactor * 0.30);

    } else {
        // ── Ordered-dither terrain (LucasArts VGA style) ──────────────────────
        // Two palette entries: shadow and highlight from base terrain colour.
        vec3 colDark  = v_Color * 0.65;
        vec3 colLight = min(v_Color * 1.35, vec3(1.0));

        // Two-scale Bayer dither: fine grain (4 px) + coarse (8 px).
        // Mixing scales breaks the uniform grid and gives a painted feel.
        const int b[16] = int[16](0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5);
        ivec2 sc      = ivec2(gl_FragCoord.xy);
        float bFine   = float(b[(sc.x & 3)        | ((sc.y & 3) << 2)])        / 16.0;
        float bCoarse = float(b[((sc.x >> 1) & 3) | (((sc.y >> 1) & 3) << 2)]) / 16.0;
        float bayer   = bFine * 0.65 + bCoarse * 0.35;

        // Hex-local radial gradient: lighter centre, darker rim.
        float hexLight = clamp(1.0 - v_EdgeDist * 2.2 + 0.15, 0.0, 1.0);

        // World-space sine variation: slow organic brightness shift between hexes
        // (~3-hex period) so adjacent tiles feel hand-painted, not stamped.
        float worldVar = sin(v_WorldPos.x * 1.8 + 0.7) * cos(v_WorldPos.z * 2.1 + 1.3)
                         * 0.5 + 0.5;

        // Blend: structural (hex gradient) + fine grain (bayer) + slow variation.
        float blend = hexLight * 0.55 + bayer * 0.30 + worldVar * 0.15;
        color = (blend >= 0.5) ? colLight : colDark;

        // Crisp tile-boundary darkening reads as grid lines.
        float edgeFactor = smoothstep(0.40, 0.50, v_EdgeDist);
        color = mix(color, color * 0.58, edgeFactor * 0.45);
    }

    // Distance fog
    float dist      = length(v_WorldPos - u_CameraPos);
    float fogFactor = clamp(1.0 - exp(-u_FogDensity * dist), 0.0, 0.6);
    color = mix(color, u_FogColor, fogFactor);

    FragColor = vec4(color, 1.0);
}
