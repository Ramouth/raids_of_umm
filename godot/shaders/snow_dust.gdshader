shader_type canvas_item;

// Trees and ridges near the snowy north: snow lies on the upper edge of every
// crown, branch ledge and crest (opaque pixels with open air just above them),
// so the woods, the mountains and the snow read as one cold place. On ridges
// (`rock` on) the lit grey rock faces above `cap` (a fraction of the sprite's
// height) are snowed over too, for snow caps. Hard pixels only, no blur.
uniform vec4 snow : source_color = vec4(0.83, 0.87, 0.9, 1.0);
uniform vec4 snow_shade : source_color = vec4(0.66, 0.72, 0.78, 1.0);
uniform float depth = 7.0;   // texels of snow on a ledge (trees are drawn at ~0.4x)
uniform float cap = 1.0;     // below about this fraction of the height, ledges stay bare
uniform bool rock = false;
varying vec4 tint;

void vertex() {
    tint = COLOR;   // modulate (e.g. the darkened margin), kept apart from the texture
}

void fragment() {
    vec4 c = texture(TEXTURE, UV);
    // A ragged snow line, not a ruler: the cap wanders with the slope.
    float line = cap + (fract(sin(floor(UV.x * 40.0) * 91.7) * 4375.85) - 0.5) * 0.06 + sin(UV.x * 23.0) * 0.04;
    if (c.a > 0.5 && UV.y < line) {
        vec2 up = vec2(0.0, TEXTURE_PIXEL_SIZE.y * depth);
        float lum = dot(c.rgb, vec3(0.3, 0.59, 0.11));
        float grey = max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
        // Open air within `depth` texels above: a cap of snow; a little lower, its shadow.
        if (texture(TEXTURE, UV - up).a < 0.5 || texture(TEXTURE, UV - up * 0.5).a < 0.5) c.rgb = snow.rgb;
        else if (rock && grey < 0.12 && lum > 0.42 + (UV.y - line + 0.12) * 1.5) c.rgb = snow.rgb;
        else if (rock && grey < 0.12 && lum > 0.3 + (UV.y - line + 0.12) * 1.5) c.rgb = snow_shade.rgb;
        else if (texture(TEXTURE, UV - up * 1.6).a < 0.5) c.rgb = mix(c.rgb, snow_shade.rgb, 0.65);
    }
    COLOR = c * tint;
}
