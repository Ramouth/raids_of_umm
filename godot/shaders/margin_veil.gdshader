shader_type canvas_item;
render_mode unshaded;

// The veil over the land beyond the map's edge (map_view.gd, _margin_veil).
// `rings` holds, per hex (texel = axial cell - `first`), how far that hex lies
// from the playable land: 0 on the map, 1..3 in the margin, 4 past it. Each
// fragment blends the rings of the hexes around it with a Gaussian, so the
// fade follows the coastline of the map in one smooth sweep, with no hex-shaped
// bands; it starts a little inside the map's outermost hexes, so it has no
// edge of its own.
uniform sampler2D rings : filter_nearest, repeat_disable;
uniform ivec2 first;
uniform vec2 hex = vec2(112.0, 80.0);
uniform float sigma = 46.0;
uniform vec4 shade : source_color;
uniform vec4 fade = vec4(0.42, 0.68, 0.9, 1.0);   // veil strength at rings 1..4
uniform float start = 0.3;   // blended ring where the veil begins (inside the map's edge hexes)
varying vec2 world_position;

void vertex() {
    world_position = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy;
}

float ring_of(ivec2 cell) {
    ivec2 t = cell - first;
    ivec2 size = textureSize(rings, 0);
    if (t.x < 0 || t.y < 0 || t.x >= size.x || t.y >= size.y) return 4.0;
    return texelFetch(rings, t, 0).r * 4.0;
}

void fragment() {
    vec2 p = world_position;
    float q = p.x / (hex.x * 0.75);
    float r = p.y / hex.y - q * 0.5;
    ivec2 home = ivec2(int(round(q)), int(round(r)));
    float sum = 0.0;
    float total = 0.0;
    for (int dq = -2; dq <= 2; dq++) {
        for (int dr = -2; dr <= 2; dr++) {
            ivec2 cell = home + ivec2(dq, dr);
            vec2 centre = vec2(float(cell.x) * hex.x * 0.75, (float(cell.y) + float(cell.x) * 0.5) * hex.y);
            vec2 d = p - centre;
            float k = exp(-dot(d, d) / (2.0 * sigma * sigma));
            sum += k * ring_of(cell);
            total += k;
        }
    }
    float ring = sum / total;
    float alpha;
    if (ring < 1.0) alpha = fade.x * clamp((ring - start) / (1.0 - start), 0.0, 1.0);
    else if (ring < 2.0) alpha = mix(fade.x, fade.y, ring - 1.0);
    else if (ring < 3.0) alpha = mix(fade.y, fade.z, ring - 2.0);
    else alpha = mix(fade.z, fade.w, clamp(ring - 3.0, 0.0, 1.0));
    if (alpha <= 0.0) discard;
    COLOR = vec4(shade.rgb, alpha);
}
