shader_type canvas_item;
render_mode unshaded;

// The snowy north, as one continuous layer for a whole map (map_view.gd,
// _build_snow), so no hex outline or sprite seam can show.
//   density.r  snow fraction of the land around a point: 1 deep in a snowfield,
//              ~0.5 where a snow hex meets a meadow hex, 0 a hex out. Smoothed.
//   density.g  "dust": how close the nearest snow is, 1 at a snow hex, falling
//              to 0 about two hexes out. Drives the frost on the grass.
//   density.b  "trodden": 1 at a camp standing in the snow, 0 a hex out.
//              Thins the snow there to frost and grass.
// Deep snow: only low-frequency world-space noise shapes its outline (broad
// lobes and bays, never crumbs), with a clean pixel-stepped edge and a darker
// rim. Inside it, a little life: the lobes are lit on their windward (upper)
// side and faintly shaded on their lee, short broken drift ridges run across
// it, and near its edges a few grass tips and stones poke through.
// Beyond it, frost on the grass: irregular patches cut from layered noise, so
// they come in every shape (tiny flecks, lumps, joined runs), thick and merging
// beside the snow, fewer and smaller further out, gone two hexes away; thicker
// too where the land is half snow (a meadow nook inside a snowfield).
// Everything sits on a `pixel`-sized world grid, the meadow's own pixel scale
// at zoom 1: hard pixels, a lit top row and one pixel of shadow, no blur.
uniform sampler2D density : filter_linear, repeat_disable;
uniform vec2 origin;          // world position of density's top-left texel
uniform vec2 extent;          // world size covered by density
uniform float pixel = 2.0;    // world units per snow pixel
uniform float trodden = 0.75; // how far a camp thins the snow (map_view.gd SNOW_TRODDEN)
uniform vec4 light : source_color = vec4(0.875, 0.898, 0.914, 1.0);
uniform vec4 soft : source_color = vec4(0.776, 0.808, 0.839, 1.0);
uniform vec4 rim : source_color = vec4(0.659, 0.706, 0.753, 1.0);
uniform vec4 clump : source_color = vec4(0.722, 0.773, 0.792, 1.0);   // frost: a shade under the snow
uniform vec4 clump_lit : source_color = vec4(0.835, 0.863, 0.882, 1.0);
uniform vec4 shadow : source_color = vec4(0.12, 0.17, 0.14, 0.45);
uniform vec4 blade : source_color = vec4(0.24, 0.33, 0.19, 1.0);
uniform vec4 blade_lit : source_color = vec4(0.38, 0.47, 0.27, 1.0);
uniform vec4 stone : source_color = vec4(0.53, 0.52, 0.49, 1.0);
uniform vec4 stone_dark : source_color = vec4(0.37, 0.37, 0.36, 1.0);
varying vec2 world_position;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

void vertex() {
    world_position = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy;
}

vec3 field3(vec2 p) {
    return texture(density, (p - origin) / extent).rgb;
}

// How deep the snow is at a world point; > 0.5 is covered.
float cover(vec2 p) {
    vec2 q = p * vec2(0.6, 1.0);   // the wind draws drifts out east-west
    float n = noise(q / 110.0) * 0.7 + noise(q / 48.0) * 0.3;
    vec3 f = field3(p);
    return f.r - f.b * trodden + (n - 0.5) * 1.1;
}

// How much frost lies on the grass at a world point (0..1).
float frost_amount(vec2 p) {
    vec3 f = field3(p);
    return clamp(pow(f.g, 1.6) * 0.62 + smoothstep(0.2, 0.5, f.r) * 0.35 + f.b * 0.3, 0.0, 1.0);
}

// True when world pixel `p` (on grass) is frosted. Three octaves of noise at
// pixel scale give patches of every shape; the threshold drops as the frost
// thickens, so near the snow patches grow and join, far out only the odd fleck
// survives. A slow noise gathers frost in hollows and leaves some ground bare.
bool frosted(vec2 p) {
    float amount = frost_amount(p);
    if (amount < 0.03) return false;
    vec2 u = p / pixel;
    float n = noise(u * vec2(0.14, 0.19) + 3.1) * 0.55 + noise(u / 3.4 + 7.7) * 0.35 + noise(u / 1.4 + 1.3) * 0.1;
    float hollow = noise(p / 75.0 + 21.0) - 0.5;
    return n > 0.95 - amount * 0.55 - hollow * 0.3;
}

// Bare ground at `p`: neither drift nor frost.
bool edge(vec2 p) {
    return cover(p) < 0.5 && !frosted(p);
}

// Grass tips and stones poking through the snow near its edge, sparse, one
// small figure per 8-pixel cell at most. 0 none, 1 blade, 2 lit blade,
// 3 stone, 4 dark stone.
int poke(vec2 p) {
    vec2 ip = floor(p / pixel);
    vec2 cell = floor(ip / 8.0);
    float pick = hash(cell + 17.3);
    if (pick > 0.22) return 0;
    vec2 anchor = cell * 8.0 + floor(vec2(hash(cell + 2.9), hash(cell + 6.1)) * 4.0) + 2.0;
    // Only where the snow thins out toward its edge.
    float c = cover((anchor + 0.5) * pixel);
    if (c < 0.5 || c > 0.68) return 0;
    vec2 d = ip - anchor;
    if (pick < 0.06) {
        // A stone: a low hump, 4 wide and 2 high, its top just clear of the snow.
        if (d.y == 0.0 && d.x >= 1.0 && d.x <= 2.0) return 3;
        if (d.y == 1.0 && d.x >= 0.0 && d.x <= 3.0) return 4;
        return 0;
    }
    // A tuft of grass tips: blades with a lit tip, their feet buried in snow.
    // Two shapes, either way round, 5 pixels wide and 3 high.
    if (hash(cell + 4.4) > 0.5) d.x = 4.0 - d.x;
    if (d.x < 0.0 || d.x > 4.0 || d.y < -1.0 || d.y > 1.0) return 0;
    int row = int(d.y) + 1;
    int col = int(d.x);
    // Rows top to bottom as 5-bit masks (bit 4 = leftmost): lit tips, blades, base.
    int tips = hash(cell + 9.4) > 0.5 ? 9 : 4;     // .L..L or ..L..
    int body = hash(cell + 9.4) > 0.5 ? 26 : 21;   // LL.L. or L.L.L
    int base = 30;                                  // DDDD.
    int bit = 1 << (4 - col);
    if (row == 0) return (tips & bit) != 0 ? 2 : 0;
    if (row == 1) return (body & bit) != 0 ? ((tips & (bit << 1)) != 0 || (tips & (bit >> 1)) != 0 ? 1 : 2) : 0;
    return (base & bit) != 0 ? 1 : 0;
    return 0;
}

void fragment() {
    vec2 p = floor(world_position / pixel) * pixel + pixel * 0.5;
    float c = cover(p);
    if (c >= 0.5) {
        // The drift texture tiles seamlessly; a slow warp keeps repeats from lining up.
        vec2 warp = vec2(noise(p / 210.0), noise(p / 210.0 + 11.0)) * 40.0;
        vec3 snow = texture(TEXTURE, fract((p + warp) / 128.0)).rgb;
        // Broad, banded light and shade in world space (never repeating, no gradients).
        float broad = noise(p / 170.0 + 7.3);
        if (broad > 0.68) snow = mix(snow, light.rgb, 0.6);
        else if (broad < 0.3) snow = mix(snow, soft.rgb, 0.6);
        // The lobes have a body: a faint shade on their lee, a few pixels in
        // from the lower edge, so the drift seems to round over into it.
        if (cover(p + vec2(0.0, pixel * 4.0)) < 0.5 || cover(p + vec2(pixel * 2.0, pixel * 3.0)) < 0.5) snow = mix(snow, soft.rgb, 0.7);
        else if (cover(p + vec2(0.0, pixel * 8.0)) < 0.5) snow = mix(snow, soft.rgb, 0.35);
        // Short, broken drift ridges: a lit crest pixel over a shaded lee pixel,
        // wavy lines running east-west, only where a slow noise lets them show.
        float t = p.y / 34.0 + noise(p / 90.0) * 2.4 + noise(p / 31.0 + 4.0) * 0.5;
        float crest = fract(t);
        float step_t = pixel / 34.0;
        if (noise(p / vec2(44.0, 22.0) + 13.0) > 0.64 && c > 0.62) {
            if (crest < step_t) snow = mix(snow, light.rgb, 0.8);
            else if (crest < step_t * 2.0) snow = mix(snow, rim.rgb, 0.4);
        }
        // A clean rim where the drift ends: 1 pixel, 2 on the lee (lower) side.
        // Where frost runs up to the drift the two join: no rim there.
        if (edge(p + vec2(0.0, -pixel)) || edge(p + vec2(-pixel, 0.0))
                || edge(p + vec2(pixel, 0.0)) || edge(p + vec2(0.0, pixel))
                || edge(p + vec2(0.0, pixel * 2.0))) snow = rim.rgb;
        int thing = poke(p);
        if (thing == 1) snow = blade.rgb;
        else if (thing == 2) snow = blade_lit.rgb;
        else if (thing == 3) snow = stone.rgb;
        else if (thing == 4) snow = stone_dark.rgb;
        else if (poke(p - vec2(0.0, pixel)) >= 3 || poke(p - vec2(0.0, pixel)) == 1) snow = mix(snow, rim.rgb, 0.6);   // its shadow on the snow
        COLOR = vec4(snow, 1.0);
    } else if (frosted(p)) {
        COLOR = frosted(p - vec2(0.0, pixel)) ? clump : clump_lit;   // lit top row
    } else if (cover(p - vec2(0.0, pixel)) >= 0.5) {
        COLOR = shadow;   // one pixel of shadow under the drift's lower edge
    } else if (frosted(p - vec2(0.0, pixel))) {
        COLOR = shadow;   // and under each frost patch
    } else {
        discard;
    }
}
