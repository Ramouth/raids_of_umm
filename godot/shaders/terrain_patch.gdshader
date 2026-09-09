shader_type canvas_item;
render_mode unshaded;

// Feather legacy square/hex asset backgrounds into the shared sand surface.
// Coordinates and movement stay entirely in the logical map adapter.
uniform float opacity : hint_range(0.0, 1.0) = 1.0;
uniform float edge_start : hint_range(0.0, 1.0) = 0.52;
uniform bool continuous_texture = false;
uniform vec4 ground_color : source_color = vec4(0.784, 0.600, 0.263, 1.0);
varying vec2 world_position;
varying vec4 vertex_tint;

void vertex() {
    world_position = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy;
    vertex_tint = COLOR;
}

void fragment() {
    float radius = length((UV - vec2(0.5)) * 2.0);
    if (continuous_texture) {
        // Mirror a cropped interior of the legacy texture so repeat edges meet.
        // Adjacent sprites sample the same world position, preserving continuity.
        vec2 p = world_position / vec2(410.0, 310.0);
        p += vec2(sin(p.y * 4.1 + p.x * 1.8), cos(p.x * 3.7 - p.y * 1.3)) * 0.22;
        vec2 sample_uv = abs(fract(p) * 2.0 - 1.0);
        COLOR = texture(TEXTURE, vec2(0.1) + sample_uv * 0.8) * vertex_tint;
        COLOR.rgb = mix(ground_color.rgb, COLOR.rgb, 0.42);
    }
    COLOR.a *= (1.0 - smoothstep(edge_start, 1.0, radius)) * opacity;
}
