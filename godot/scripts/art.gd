@tool
class_name Art
extends RefCounted
## The art registry, used statically: Art.texture(id). Every image the client shows is looked up
## here by a logical id ("unit.woad_runner", "portrait.ushari", "screen.victory").
## godot/content_source/art.json maps ids to files under res://content/textures/.
## texture() never returns null: a missing or unknown image becomes a generated
## placeholder (a flat shape in its category colour with the id's initials),
## warned about once. Swapping art = editing art.json or dropping in a file.
## A class_name with static state rather than an autoload: autoload names do not
## resolve in scripts compiled before autoloads exist (tests run with --script
## preload screen scripts), while class names always do.

const REGISTRY := "res://content_source/art.json"
const ROOT := "res://content/textures/"
## Placeholder colour and default frame size per id prefix.
const CATEGORIES := {
    "unit": [Color("8a5a44"), Vector2i(128, 128)], "hero": [Color("6f7f9a"), Vector2i(64, 64)],
    "portrait": [Color("7a6a8c"), Vector2i(128, 128)], "object": [Color("8c7a4a"), Vector2i(128, 128)],
    "kind": [Color("a08a3c"), Vector2i(64, 64)], "feature": [Color("5f6f4a"), Vector2i(128, 128)],
    "tree": [Color("3f6a3a"), Vector2i(128, 128)], "terrain": [Color("7a6a50"), Vector2i(64, 64)],
    "screen": [Color("4a3a2a"), Vector2i(640, 400)], "slide": [Color("2e2a36"), Vector2i(640, 360)],
    "icon": [Color("6a5a3a"), Vector2i(32, 32)],
}
const OTHER := [Color("8a2f6a"), Vector2i(64, 64)]
const OUTLINE := Color("1a120c")
const INK := Color("f3e6c4")
## 3x5 glyphs, rows top to bottom, 3 bits each (4 = left column).
const GLYPHS := {
    "A": [2, 5, 7, 5, 5], "B": [6, 5, 6, 5, 6], "C": [3, 4, 4, 4, 3], "D": [6, 5, 5, 5, 6],
    "E": [7, 4, 6, 4, 7], "F": [7, 4, 6, 4, 4], "G": [3, 4, 5, 5, 3], "H": [5, 5, 7, 5, 5],
    "I": [7, 2, 2, 2, 7], "J": [1, 1, 1, 5, 2], "K": [5, 5, 6, 5, 5], "L": [4, 4, 4, 4, 7],
    "M": [5, 7, 7, 5, 5], "N": [6, 5, 5, 5, 5], "O": [2, 5, 5, 5, 2], "P": [6, 5, 6, 4, 4],
    "Q": [2, 5, 5, 6, 3], "R": [6, 5, 6, 5, 5], "S": [3, 4, 2, 1, 6], "T": [7, 2, 2, 2, 2],
    "U": [5, 5, 5, 5, 7], "V": [5, 5, 5, 5, 2], "W": [5, 5, 7, 7, 5], "X": [5, 5, 2, 5, 5],
    "Y": [5, 5, 2, 2, 2], "Z": [7, 1, 2, 4, 7], "0": [7, 5, 5, 5, 7], "1": [2, 6, 2, 2, 7],
    "2": [6, 1, 2, 4, 7], "3": [6, 1, 2, 1, 6], "4": [5, 5, 7, 1, 1], "5": [7, 4, 6, 1, 6],
    "6": [3, 4, 7, 5, 7], "7": [7, 1, 2, 2, 2], "8": [7, 5, 7, 5, 7], "9": [7, 5, 7, 1, 6],
    "?": [6, 1, 2, 0, 2],
}

static var _ids: Dictionary = {}
static var _overrides: Dictionary = {}
static var _cache: Dictionary = {}
static var _warned: Dictionary = {}

static func _static_init() -> void:
    reload()

## Re-reads art.json and forgets cached textures (overrides are kept).
static func reload() -> void:
    _ids = {}
    var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(REGISTRY)) if FileAccess.file_exists(REGISTRY) else null
    if parsed is Dictionary and parsed.get("ids") is Dictionary:
        _ids = parsed.ids
    else:
        push_warning("Art: %s is missing or unreadable; every image will be a placeholder." % REGISTRY)
    _cache.clear()

## Points an id at another file for this session (tests, quick previews).
static func override(id: String, file: String) -> void:
    _overrides[id] = file
    _cache.clear()
    _warned.erase(id)

static func clear_overrides() -> void:
    _overrides.clear()
    _cache.clear()
    _warned.clear()

## Every id listed in art.json.
static func ids() -> Array:
    return _ids.keys()

## True when art.json (or an override) names this id; unit.<id> is always known.
static func is_listed(id: String) -> bool:
    return not _entry(id).is_empty()

## The file shown for this id (after fallbacks), as a res:// path; "" when none exists.
static func path(id: String) -> String:
    var relative := _resolve(id, 0)
    return "" if relative.is_empty() else ROOT + relative

## True when this id (or its fallback) has a real image on disk.
static func has_art(id: String) -> bool:
    return not _resolve(id, 0).is_empty()

## The image for `id`, never null. `id` may also be a file under content/textures
## (data such as buildings.json names files). An id art.json does not list uses
## `fallback` (an id) instead; anything still without an image gets a placeholder.
static func texture(id: String, fallback := "") -> Texture2D:
    var key := id + "|" + fallback
    if _cache.has(key): return _cache[key]
    var result: Texture2D = null
    var relative := _resolve(id, 0)
    if relative.is_empty() and not fallback.is_empty() and not is_listed(id):
        relative = _resolve(fallback, 0)
    if not relative.is_empty():
        result = _load(relative, bool(_entry(id).get("mipmaps", false)))
        if result != null and _entry(id).has("tint"): result = _tinted(result, Color(str(_entry(id).tint)))
    if result == null:
        if not _warned.has(id):
            _warned[id] = true
            push_warning("Art: no image for '%s' (%s); showing a placeholder. Add it to godot/content_source/art.json or drop the file in." % [id, _describe(id)])
        result = placeholder(id)
    _cache[key] = result
    return result

## A flat, nearest-filtered stand-in in the category colour with the id's initials.
static func placeholder(id: String) -> ImageTexture:
    var key := "placeholder|" + id
    if _cache.has(key): return _cache[key]
    var category: Array = CATEGORIES.get(id.get_slice(".", 0), OTHER)
    var entry := _entry(id)
    var frame: Vector2i = category[1]
    if entry.get("size") is Array and entry.size.size() == 2:
        frame = Vector2i(int(entry.size[0]), int(entry.size[1]))
    var frames := maxi(1, int(entry.get("frames", 1)))
    var image := Image.create_empty(frame.x * frames, frame.y, false, Image.FORMAT_RGBA8)
    var tile := Image.create_empty(frame.x, frame.y, false, Image.FORMAT_RGBA8)
    _draw_shape(tile, id.get_slice(".", 0), category[0])
    _draw_initials(tile, _initials(id))
    for i in frames:
        image.blit_rect(tile, Rect2i(Vector2i.ZERO, frame), Vector2i(i * frame.x, 0))
    var result := ImageTexture.create_from_image(image)
    _cache[key] = result
    return result

static func _entry(id: String) -> Dictionary:
    var value: Variant = _ids.get(id)
    var entry: Dictionary = {}
    if value is String: entry = {"file": value}
    elif value is Dictionary: entry = value.duplicate()
    elif id.begins_with("unit.") and id.get_slice_count(".") == 2:
        entry = {"file": "units/%s.png" % id.get_slice(".", 1)}
    if _overrides.has(id): entry.file = _overrides[id]   # keeps frames, size, mipmaps
    return entry

static func _resolve(id: String, depth: int) -> String:
    if id.is_empty() or depth > 8: return ""
    if "/" in id:
        return id if _exists(id) else ""
    var entry := _entry(id)
    var file := str(entry.get("file", ""))
    if not file.is_empty() and _exists(file): return file
    return _resolve(str(entry.get("fallback", "")), depth + 1)

static func _exists(relative: String) -> bool:
    return ResourceLoader.exists(ROOT + relative) or FileAccess.file_exists(ROOT + relative)

static func _load(relative: String, mipmaps: bool) -> Texture2D:
    var full := ROOT + relative
    var texture: Texture2D = null
    if ResourceLoader.exists(full):
        texture = load(full) as Texture2D
    if texture == null and FileAccess.file_exists(full):
        # A freshly dropped-in file that has not been imported yet.
        var raw := Image.load_from_file(full)
        if raw: texture = ImageTexture.create_from_image(raw)
    if texture != null and mipmaps:
        var bitmap := texture.get_image()
        if bitmap:
            if not bitmap.has_mipmaps(): bitmap.generate_mipmaps()
            texture = ImageTexture.create_from_image(bitmap)
    return texture

## Another creature drawn from shared art: every pixel multiplied by `tint`
## ("tint": "#rrggbb" in art.json), so a reused sprite still reads as its own unit.
static func _tinted(texture: Texture2D, tint: Color) -> Texture2D:
    var bitmap := texture.get_image()
    if bitmap == null: return texture
    bitmap = bitmap.duplicate()
    if bitmap.is_compressed(): bitmap.decompress()
    bitmap.convert(Image.FORMAT_RGBA8)
    for y in bitmap.get_height():
        for x in bitmap.get_width():
            var pixel := bitmap.get_pixel(x, y)
            if pixel.a > 0.0: bitmap.set_pixel(x, y, Color(pixel.r * tint.r, pixel.g * tint.g, pixel.b * tint.b, pixel.a))
    return ImageTexture.create_from_image(bitmap)

static func _describe(id: String) -> String:
    if "/" in id: return "file %s%s not found" % [ROOT, id]
    var entry := _entry(id)
    if entry.is_empty(): return "id not listed"
    return "file %s%s not found" % [ROOT, entry.get("file", "")]

static func _initials(id: String) -> String:
    var name := id.get_file().get_basename() if "/" in id else id.get_slice(".", id.get_slice_count(".") - 1)
    var words := name.replace("-", "_").split("_", false)
    var text := ""
    if words.size() >= 2: text = words[0].left(1) + words[1].left(1)
    elif words.size() == 1: text = words[0].left(2)
    text = text.to_upper()
    var known := ""
    for c in text: known += c if GLYPHS.has(c) else "?"
    return known if not known.is_empty() else "?"

static func _draw_shape(tile: Image, category: String, colour: Color) -> void:
    var w := tile.get_width()
    var h := tile.get_height()
    if not category in ["unit", "hero", "portrait", "object", "kind", "feature", "tree"]:
        # Pictures and tiles: a framed flat panel.
        tile.fill(OUTLINE)
        tile.fill_rect(Rect2i(2, 2, w - 4, h / 2 - 2), colour.lightened(0.12))
        tile.fill_rect(Rect2i(2, h / 2, w - 4, h - h / 2 - 2), colour)
        return
    for y in h:
        for x in w:
            var inside := _inside(category, (x + 0.5) / w, (y + 0.5) / h, 0.0)
            if inside:
                var edge := not _inside(category, (x + 0.5) / w, (y + 0.5) / h, 2.0 / mini(w, h))
                # Upper-left light: a slightly brighter top half.
                var shade := colour.lightened(0.12) if y < h / 2 else colour
                tile.set_pixel(x, y, OUTLINE if edge else shade)

## Shape test in unit coordinates; `inset` shrinks the shape (for the outline).
static func _inside(category: String, u: float, v: float, inset: float) -> bool:
    match category:
        "unit", "hero", "portrait":
            # Figure: round head over a tapered body.
            var head := Vector2(u - 0.5, v - 0.28).length() < 0.15 - inset
            var body := v > 0.42 + inset and v < 0.94 - inset and absf(u - 0.5) < 0.16 + (v - 0.42) * 0.38 - inset
            return head or body
        "object", "kind", "feature", "tree":
            # Squat diamond, like a hex footprint seen from above.
            return absf(u - 0.5) / 0.46 + absf(v - 0.62) / 0.3 < 1.0 - inset * 3.0
        _:
            return u > 0.02 + inset and u < 0.98 - inset and v > 0.02 + inset and v < 0.98 - inset

static func _draw_initials(tile: Image, text: String) -> void:
    var w := tile.get_width()
    var h := tile.get_height()
    var scale := maxi(1, mini(w, h) / 24)
    var width := (text.length() * 4 - 1) * scale
    var origin := Vector2i((w - width) / 2, int(h * 0.62) - (5 * scale) / 2)
    for i in text.length():
        var rows: Array = GLYPHS[text[i]]
        for row in 5:
            for col in 3:
                if int(rows[row]) & (4 >> col):
                    var at := origin + Vector2i((i * 4 + col) * scale, row * scale)
                    tile.fill_rect(Rect2i(at, Vector2i(scale, scale)), INK)
