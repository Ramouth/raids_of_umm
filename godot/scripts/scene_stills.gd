extends RefCounted
## Pixel-art stills for full-screen story scenes, painted pixel by pixel into a
## small image and shown scaled up with nearest filtering (ART_DIRECTION.md).
## Each still covers the picture band of a ChapterScene: 1280×464 at scale 4.

const W := 320
const H := 116

static var _cache: Dictionary = {}

## The still for `id`, or null when there is none (the caller draws its own).
static func texture(id: String) -> Texture2D:
    if _cache.has(id): return _cache[id]
    var image: Image
    match id:
        "dog_tag": image = _dog_tag()
        _: return null
    _cache[id] = ImageTexture.create_from_image(image)
    return _cache[id]

## Item pictures cut from a still (the thing itself, close up, on its ground).
const ITEM_CROPS := {"dog_tag": Rect2i(132, 52, 88, 64)}

## The picture for an item drawn here ("dog_tag"), or null. Art id "item.dog_tag"
## names the PNG exported from it (tests/scene_capture.gd --export-art).
static func item_texture(id: String) -> Texture2D:
    var key := "item|" + id
    if _cache.has(key): return _cache[key]
    var still := texture(id)
    if still == null or not ITEM_CROPS.has(id): return null
    _cache[key] = ImageTexture.create_from_image(still.get_image().get_region(ITEM_CROPS[id]))
    return _cache[key]

# Palette (ART_DIRECTION.md), plus one cool tin mid-tone.
const TOMB := Color("080610")
const CRACK := Color("352018")
const EARTH := Color("4a200a")
const SIENNA := Color("884010")
const WARM_SAND := Color("b87828")
const SHADOW_STONE := Color("685040")
const SANDSTONE := Color("a08860")
const LIMESTONE := Color("c8b888")
const BONE := Color("eae0c0")
const COLD_DARK := Color("1e1c38")
const DUNGEON := Color("3a3850")
const LIT_STONE := Color("5a5870")
const TIN := Color("8c8aa0")
const DEEP_WATER := Color("1a5058")
const TEAL := Color("3a8080")
const SAGE := Color("607840")
const OLD_GOLD := Color("a07820")
const GOLD := Color("f8c840")

## Deterministic 0..1 noise for a pixel.
static func _hash(x: int, y: int, salt := 0) -> float:
    var h := (x * 374761393 + y * 668265263 + salt * 2147483647) & 0x7fffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0x7fffffff
    return float(h % 1000) / 1000.0

## A studded collar lying in mud, its tin tag cut with northern marks.
static func _dog_tag() -> Image:
    var img := Image.create(W, H, false, Image.FORMAT_RGBA8)
    # Mud: clumps, a puddle, paw prints, a smear of blood, dead grass.
    for y in H:
        for x in W:
            var n := _hash(x / 3, y / 2, 1) * 0.6 + _hash(x, y, 2) * 0.4
            var c := EARTH
            if n < 0.22: c = CRACK
            elif n > 0.86: c = SHADOW_STONE
            elif n > 0.74: c = SIENNA
            img.set_pixel(x, y, c)
    _puddle(img, Vector2(268, 34), Vector2(34, 9))
    _puddle(img, Vector2(52, 100), Vector2(22, 6))
    for paw in [Vector2(236, 78), Vector2(252, 92), Vector2(280, 74), Vector2(110, 104), Vector2(296, 96)]:
        _paw(img, paw)
    for i in 18:   # flattened dead grass
        var root := Vector2(_hash(i, 9) * W, _hash(i, 10) * H)
        var lean := -1.0 if _hash(i, 11) > 0.5 else 1.0
        for s in 7:
            _put(img, int(root.x + s * lean), int(root.y - s * 0.4), SAGE if s < 4 else WARM_SAND)

    # The collar: an oval band seen from above, cut off by the top of the frame.
    var centre := Vector2(118, 26)
    var outer := Vector2(108, 40)
    var band := 12.0
    var collar := func(p: Vector2) -> bool:
        var d := p - centre
        var o := (d.x * d.x) / (outer.x * outer.x) + (d.y * d.y) / (outer.y * outer.y)
        var i := (d.x * d.x) / ((outer.x - band) ** 2) + (d.y * d.y) / ((outer.y - band) ** 2)
        return o <= 1.0 and i > 1.0
    # The tag: a beaten tin plate, a little askew, hanging from the collar's ring.
    var tag_centre := Vector2(176, 88)
    var tilt := deg_to_rad(-9.0)
    var tag_half := Vector2(34, 20)
    var to_tag := func(p: Vector2) -> Vector2: return (p - tag_centre).rotated(-tilt)
    var on_tag := func(p: Vector2) -> bool:
        var u: Vector2 = to_tag.call(p)
        var q := u.abs() - tag_half + Vector2(5, 5)
        var rounded := Vector2(maxf(q.x, 0.0), maxf(q.y, 0.0)).length() + minf(maxf(q.x, q.y), 0.0) - 5.0
        var dent := (_hash(int(u.x / 4.0), int(u.y / 4.0), 5) - 0.5) * 1.2   # hammered, not cut true
        return rounded + dent <= 0.0 and (u - Vector2(0, -14)).length() > 2.6   # the hole
    var ring := Vector2(170, 64)
    var on_ring := func(p: Vector2) -> bool:
        var r := (p - ring).length()
        return r >= 3.2 and r <= 5.4

    # Cast shadows first, down and to the right.
    for y in H:
        for x in W:
            var p := Vector2(x, y)
            var shade := Vector2(x - 2, y - 3)
            if not collar.call(p) and not on_tag.call(p) and (collar.call(shade) or on_tag.call(shade) or on_ring.call(shade)):
                img.set_pixel(x, y, TOMB if _hash(x, y, 6) > 0.4 else CRACK)

    # Leather band: lit upper edge, worn middle, dark lower edge; studs along it.
    for y in H:
        for x in W:
            var p := Vector2(x, y)
            if not collar.call(p): continue
            var d := p - centre
            var o := sqrt((d.x * d.x) / (outer.x * outer.x) + (d.y * d.y) / (outer.y * outer.y))
            var depth := (1.0 - o) * outer.y / band   # 0 at the outer edge, 1 at the inner
            var back := d.y < 0.0                     # the far side of the loop, in its own shade
            var c := SIENNA
            if depth < 0.09 or depth > 0.91: c = TOMB
            elif depth < 0.24: c = WARM_SAND if not back else SIENNA
            elif depth > 0.74: c = EARTH
            elif absf(depth - 0.5) < 0.05 and _hash(x / 2, y, 9) > 0.35: c = EARTH   # stitching
            elif _hash(x, y, 7) > 0.9: c = EARTH       # scuffs
            if back and c == SIENNA: c = EARTH if _hash(x, y, 8) > 0.5 else SIENNA
            img.set_pixel(x, y, c)
    for i in 22:
        var angle := -0.3 + i * (PI + 0.6) / 21.0
        var mid := centre + Vector2(cos(angle) * (outer.x - band / 2.0), sin(angle) * (outer.y - band / 2.0))
        if mid.y < 1.0: continue
        _stud(img, Vector2i(roundi(mid.x), roundi(mid.y)), sin(angle) < 0.0)
    _buckle(img, centre + Vector2(cos(2.55) * (outer.x - band / 2.0), sin(2.55) * (outer.y - band / 2.0)))

    # The ring and its link to the tag.
    for y in range(56, 74):
        for x in range(162, 180):
            if on_ring.call(Vector2(x, y)):
                var lit := Vector2(x, y).x < ring.x and Vector2(x, y).y < ring.y
                img.set_pixel(x, y, LIMESTONE if lit else SANDSTONE)
                if (Vector2(x, y) - ring).length() > 4.9: img.set_pixel(x, y, CRACK)

    # The tin plate: hammered facets, a dark outline, light caught on the top-left rim.
    var marks := _marks()
    for y in range(50, H):
        for x in range(120, 236):
            var p := Vector2(x, y)
            if not on_tag.call(p): continue
            var edge := false
            for n in [Vector2(1, 0), Vector2(-1, 0), Vector2(0, 1), Vector2(0, -1)]:
                if not on_tag.call(p + n): edge = true
            if edge:
                img.set_pixel(x, y, TOMB)
                continue
            var u: Vector2 = to_tag.call(p)
            var facet := _hash(int(floor((u.x + u.y * 0.5) / 5.0)), int(floor(u.y / 4.0)), 12)
            var c := LIT_STONE
            if facet > 0.78: c = TIN
            elif facet < 0.1: c = DUNGEON
            if not on_tag.call(p + Vector2(-2, -2)): c = TIN if _hash(x, y, 13) > 0.25 else BONE   # rim catches light
            elif not on_tag.call(p + Vector2(2, 2)): c = DUNGEON
            # The cut marks: dark grooves whose lower edge shines.
            if _on_marks(marks, u): c = TOMB
            elif _on_marks(marks, u + Vector2(0, -1.0)): c = TIN if c == DUNGEON else BONE
            img.set_pixel(x, y, c)
    for y in range(66, 78):   # the link through the hole
        var x := 172 + int((y - 66) * -0.15)
        _put(img, x, y, SANDSTONE)
        _put(img, x + 1, y, CRACK)

    # Vignette: the close-up falls off into dark at the edges.
    for y in H:
        for x in W:
            var e := Vector2((x - W / 2.0) / (W / 2.0), (y - H / 2.0) / (H / 2.0))
            var v := e.x * e.x * 0.55 + e.y * e.y * 0.6
            if y < 14: v += (14 - y) * 0.06   # dark above, under the heading
            if v > 0.62 and _hash(x, y, 14) < (v - 0.62) * 3.0: img.set_pixel(x, y, TOMB)
    return img

## Ogham-like marks along a stem line, in the tag's own coordinates.
static func _marks() -> Array:
    var segs: Array = []
    segs.append([Vector2(-29, 1), Vector2(29, 1)])   # the stem
    var x := -27.0
    # [count, kind]: 0 above, 1 below, 2 across, 3 slanted across.
    for group in [[2, 0], [4, 1], [1, 3], [3, 2], [4, 0], [2, 3]]:
        for i in int(group[0]):
            match int(group[1]):
                0: segs.append([Vector2(x, 1), Vector2(x, -7)])
                1: segs.append([Vector2(x, 1), Vector2(x, 9)])
                2: segs.append([Vector2(x, -6), Vector2(x, 8)])
                3: segs.append([Vector2(x - 3, 8), Vector2(x + 3, -6)])
            x += 2.6
        x += 3.4
    # An angular sign below the stem, like a mark of the maker.
    segs.append([Vector2(-6, 13), Vector2(0, 8)])
    segs.append([Vector2(0, 8), Vector2(6, 13)])
    return segs

static func _on_marks(segs: Array, u: Vector2) -> bool:
    for seg in segs:
        if Geometry2D.get_closest_point_to_segment(u, seg[0], seg[1]).distance_to(u) < 0.62: return true
    return false

static func _stud(img: Image, at: Vector2i, far: bool) -> void:
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            if absi(dx) + absi(dy) > 3: continue
            var c := TOMB if absi(dx) == 2 or absi(dy) == 2 else SANDSTONE
            if c != TOMB and dx <= 0 and dy <= 0: c = LIMESTONE if far else BONE
            if c != TOMB and dx > 0 and dy > 0: c = SHADOW_STONE
            _put(img, at.x + dx, at.y + dy, c)

static func _buckle(img: Image, at: Vector2) -> void:
    var r := Rect2i(int(at.x) - 5, int(at.y) - 7, 11, 15)
    for y in range(r.position.y, r.end.y):
        for x in range(r.position.x, r.end.x):
            var border := x == r.position.x or x == r.end.x - 1 or y == r.position.y or y == r.end.y - 1
            var frame := x <= r.position.x + 2 or x >= r.end.x - 3 or y <= r.position.y + 2 or y >= r.end.y - 3
            if border: _put(img, x, y, TOMB)
            elif frame: _put(img, x, y, GOLD if (x + y) % 5 == 0 or y == r.position.y + 1 else OLD_GOLD)
    for x in range(r.position.x + 2, r.end.x + 3):   # the tongue across the strap
        _put(img, x, int(at.y), SANDSTONE)
        _put(img, x, int(at.y) + 1, TOMB)

static func _puddle(img: Image, centre: Vector2, radius: Vector2) -> void:
    for y in range(int(centre.y - radius.y) - 1, int(centre.y + radius.y) + 2):
        for x in range(int(centre.x - radius.x) - 1, int(centre.x + radius.x) + 2):
            var d := Vector2((x - centre.x) / radius.x, (y - centre.y) / radius.y)
            var r := d.length() + (_hash(x / 2, y, 15) - 0.5) * 0.25
            if r > 1.0: continue
            var c := DEEP_WATER
            if r > 0.85: c = CRACK
            elif d.y < -0.4 and d.x > -0.3 and _hash(x, y, 16) > 0.55: c = TEAL   # sky caught in it
            _put(img, x, y, c)

static func _paw(img: Image, at: Vector2) -> void:
    for pad in [Vector2(0, 0), Vector2(-4, -5), Vector2(-1, -7), Vector2(3, -7), Vector2(5, -4)]:
        var size := Vector2(3.0, 2.4) if pad == Vector2(0, 0) else Vector2(1.3, 1.3)
        for y in range(-3, 4):
            for x in range(-4, 5):
                var d := Vector2(x / size.x, y / size.y)
                if d.length() <= 1.0:
                    _put(img, int(at.x + pad.x) + x, int(at.y + pad.y) + y, CRACK if y <= 0 else TOMB)

static func _put(img: Image, x: int, y: int, c: Color) -> void:
    if x >= 0 and y >= 0 and x < W and y < H: img.set_pixel(x, y, c)
