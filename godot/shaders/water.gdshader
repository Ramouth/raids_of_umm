shader_type canvas_item;
render_mode unshaded;

varying vec2 world_position;

void vertex() {
    world_position = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy;
}

void fragment() {
    // Quantized ripples keep the water legible alongside the existing pixel art.
    vec2 p = floor(world_position / 2.0) * 2.0;
    float wave = sin(p.y * 0.45 + sin(p.x * 0.13) * 2.0 + TIME * 0.55);
    float fleck = sin(p.x * 0.73 + p.y * 0.32);
    float ripple = step(0.88, wave) * step(0.3, fleck);
    COLOR = vec4(mix(vec3(0.23, 0.50, 0.50), vec3(0.50, 0.72, 0.65), ripple * 0.65), 1.0);
}
