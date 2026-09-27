shader_type canvas_item;
render_mode unshaded;
uniform vec4 meadow : source_color = vec4(0.32, 0.43, 0.26, 1.0);
varying vec2 ground_position;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
void vertex() { ground_position = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy; }
void fragment() {
    // Continuous world-space meadow: no individual hex-shaped ground images.
    vec2 p = ground_position;
    float broad = noise(p / 270.0) * 0.65 + noise(p / 91.0) * 0.35;
    float blades = noise(p / vec2(3.0, 7.0));
    vec3 shade = mix(meadow.rgb * 0.76, meadow.rgb * 1.18, broad);
    shade += (blades - 0.5) * 0.045;
    COLOR = vec4(shade, 1.0);
}
