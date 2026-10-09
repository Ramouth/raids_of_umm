@tool
extends Node2D
## Native Godot scene nodes compose the map in both the editor and the game.

const ArtTheme = preload("res://scripts/map_art_theme.gd")
const MEADOW_SHADER = preload("res://shaders/north_meadow.gdshader")
var art_theme = ArtTheme.new()
var mountain_groups: Array = []

const PATCH_SHADER = preload("res://shaders/terrain_patch.gdshader")
const WATER_SHADER = preload("res://shaders/water.gdshader")
const WATER_TERRAIN := ["oasis", "lake", "river"]
var anchors: Dictionary = {}  # cell -> object anchor node
## Map art comes from the Art registry: object.<type>, kind.<kind>, feature.<terrain>,
## tree.*, terrain.*_grain; biome packages (godot/themes) name ids for their overrides.
@export_file("*.json") var map_path := "res://content/maps/old_passage.json"
@export_group("Art direction")
@export var sand_color := Color("c89943")
@export var grass_color := Color("4f7a33")
@export_range(0.0, 1.0) var grass_detail_opacity := 0.32
@export_range(0.0, 1.0) var sand_detail_opacity := 0.13
@export_range(0.0, 1.0) var dune_opacity := 0.53
@export var show_landmark_names := true
@export_tool_button("Rebuild map preview") var rebuild_preview: Callable = rebuild
var data := UmmMapData.new()
var world_bounds := Rect2()
## Land beyond the map's edge, drawn faded and out of reach: cell ->
## {ring (1 = touching the map), source (the nearest map cell), terrain}.
## Never routed, never clickable, no objects.
const MARGIN_RINGS := 3
## Veil strength by distance from the playable land (rings 0..MARGIN_RINGS+1).
const MARGIN_FADE := [0.0, 0.42, 0.68, 0.9, 1.0]
var margin: Dictionary = {}
## Shut bridges (impassable road over water) -> their gate node; and the cells
## the story has opened since (snapshot "opened"), walkable from then on.
var gates: Dictionary = {}
var opened: Dictionary = {}

func _ready() -> void:
    rebuild()

func rebuild() -> void:
    for child in get_children():
        child.free()
    anchors.clear()
    gates.clear()
    mountain_groups.clear()
    margin.clear()
    if not data.read(map_path):
        push_error(data.error)
        return
    for cell: Vector2i in opened:
        if data.tiles.has(cell): data.tiles[cell]["passable"] = true
    var edge_points := PackedVector2Array()
    for cell: Vector2i in data.tiles:
        edge_points.append_array(UmmMapData.hexagon(UmmMapData.cell_to_world(cell)))
    var playable := Geometry2D.convex_hull(edge_points)
    world_bounds = Rect2(playable[0], Vector2.ZERO)
    for point in playable:
        world_bounds = world_bounds.expand(point)
    _find_margin()
    for cell: Vector2i in margin:
        edge_points.append_array(UmmMapData.hexagon(UmmMapData.cell_to_world(cell)))
    # The ground runs on under the margin; the veil fades it out.
    var outline := Geometry2D.convex_hull(edge_points)
    var north := data.ground == "grass"
    art_theme.load_for(data.ground)
    var ground := Polygon2D.new()
    ground.name = "GrassFoundation" if north else "SandFoundation"
    ground.polygon = outline
    ground.color = art_theme.colour("ground")
    if north:
        var meadow := ShaderMaterial.new()
        meadow.shader = MEADOW_SHADER
        meadow.set_shader_parameter("meadow", art_theme.colour("ground"))
        ground.material = meadow
    add_child(ground)
    var grain := Polygon2D.new()
    grain.name = "GrassGrain" if north else "SandGrain"
    grain.polygon = outline
    # The seamless sand is optional worktree art; art.json falls back to tracked sand.
    grain.texture = Art.texture("terrain.grass_grain" if north else "terrain.sand_grain")
    grain.texture_repeat = CanvasItem.TEXTURE_REPEAT_ENABLED
    grain.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    grain.color = Color(1, 1, 1, grass_detail_opacity if north else sand_detail_opacity)
    grain.visible = not north
    add_child(grain)
    var terrain := Node2D.new()
    terrain.name = "TerrainFeatures"
    add_child(terrain)
    for cell: Vector2i in data.tiles:
        var tile: Dictionary = data.tiles[cell]
        if tile.terrain in WATER_TERRAIN or tile.terrain == data.ground:
            continue
        if north and tile.terrain in ["forest", "mountain", "highland", "swamp"]:
            continue  # Northern relief is composed as scenery, without polygon-shaped tile bases.
        if not Art.is_listed("feature." + tile.terrain):
            continue
        if tile.terrain == "snow":
            terrain.add_child(_snow_patch(cell))
            continue
        var feature := _sprite("feature." + tile.terrain)
        feature.name = "%s_%d_%d" % [tile.terrain, cell.x, cell.y]
        feature.position = UmmMapData.cell_to_world(cell)
        var material := ShaderMaterial.new()
        material.shader = PATCH_SHADER
        feature.material = material
        if tile.terrain == "dune":
            feature.scale = Vector2(1.65, 1.25)
            material.set_shader_parameter("opacity", dune_opacity)
            material.set_shader_parameter("edge_start", 0.35)
            material.set_shader_parameter("continuous_texture", true)
            material.set_shader_parameter("ground_color", sand_color)
        elif tile.terrain == "mountain":
            var variation := posmod(cell.x * 31 + cell.y * 17, 7)
            # Northern ranges read as walls: bigger peaks that overlap their neighbours.
            feature.scale = Vector2.ONE * ((1.95 + variation * 0.06) if north else (0.8 + variation * 0.04))
            feature.position += Vector2(variation - 3, (-34 if north else -18) - variation)
        elif north:
            # Hex-shaped northern ground art: feather it into the meadow.
            feature.scale = Vector2(0.95, 0.72)
            material.set_shader_parameter("edge_start", 0.55)
            material.set_shader_parameter("opacity", 0.9)
        else:
            feature.scale = Vector2(0.8, 0.65)
        terrain.add_child(feature)
    _build_water(terrain)
    _build_roads()
    _build_margin(north)
    var scenery := Node2D.new()
    scenery.name = "Scenery"
    scenery.y_sort_enabled = true
    add_child(scenery)
    for cell: Vector2i in data.objects:
        _build_object(scenery, cell, data.objects[cell])
    if north:
        _build_mountains(scenery)
        _build_trees(scenery)
        _build_undergrowth(scenery)

## Snow lies as a pale drift over its hex, wider than the hex so a strip of snow
## cells reads as one field, feathered at the rim into the meadow (or margin) around it.
## Flipped per cell so neighbouring drifts do not repeat. No undergrowth grows on it.
func _snow_patch(cell: Vector2i) -> Sprite2D:
    var patch := _sprite("feature.snow")
    patch.name = "snow_%d_%d" % [cell.x, cell.y]
    patch.position = UmmMapData.cell_to_world(cell)
    patch.scale = Vector2(1.2, 0.95)
    var h := posmod(cell.x * 73856093 ^ cell.y * 19349663, 1000)
    patch.flip_h = h % 2 == 0
    patch.flip_v = (h >> 1) % 2 == 0
    var material := ShaderMaterial.new()
    material.shader = PATCH_SHADER
    material.set_shader_parameter("edge_start", 0.5)
    material.set_shader_parameter("opacity", 1.0)
    patch.material = material
    return patch

## One painted ridge spans a connected group of up to three blocked cells.
## Geometry stays authoritative: no passable cells or map objects enter a group.
func _build_mountains(scenery: Node2D) -> void:
    var cells: Array = data.tiles.keys()
    cells.sort_custom(func(a: Vector2i, b: Vector2i): return a.y < b.y if a.x == b.x else a.x < b.x)
    var used := {}
    for start: Vector2i in cells:
        if used.has(start) or data.tiles[start].terrain != "mountain" or data.is_passable(start) or data.objects.has(start): continue
        var group: Array[Vector2i] = [start]
        used[start] = true
        for direction: Vector2i in [Vector2i(1, 0), Vector2i(1, -1), Vector2i(0, 1)]:
            for step in range(1, int(art_theme.config.ridge_cells)):
                var next := start + direction * step
                if used.has(next) or not data.tiles.has(next) or data.tiles[next].terrain != "mountain" or data.is_passable(next) or data.objects.has(next): break
                group.append(next)
                used[next] = true
            if group.size() > 1: break
        mountain_groups.append(group)
        var ridge := _sprite(str(art_theme.config.scenery.ridge))
        ridge.name = "Ridge_%d_%d" % [start.x, start.y]
        var first := UmmMapData.cell_to_world(group[0])
        var last := UmmMapData.cell_to_world(group[-1])
        var width := 132.0 + absf(last.x - first.x)
        _fit_sprite(ridge, width)
        ridge.flip_h = posmod(start.x * 7 + start.y * 13, 2) == 0
        ridge.position = (first + last) / 2.0 + Vector2(0, 24)
        ridge.offset.y = -ridge.texture.get_height() * 0.37
        # A wide ridge cannot cover the vertical span of an axial group.
        # Give each blocked cell its own visible foothill before adding the crest.
        for cell: Vector2i in group:
            var foothill := _sprite(str(art_theme.config.scenery.ridge))
            foothill.name = "MountainFootprint_%d_%d" % [cell.x, cell.y]
            _fit_sprite(foothill, 112.0)
            foothill.position = UmmMapData.cell_to_world(cell) + Vector2(0, 22)
            foothill.offset.y = -foothill.texture.get_height() * 0.37
            foothill.flip_h = posmod(cell.x + cell.y, 2) == 0
            scenery.add_child(foothill)
        scenery.add_child(ridge)

func _fit_sprite(sprite: Sprite2D, width: float) -> void:
    sprite.scale = Vector2.ONE * width / float(sprite.texture.get_width())
    sprite.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS

## Scatter plants across cell boundaries, avoiding roads and interaction anchors.
func _build_undergrowth(scenery: Node2D, only: Dictionary = {}) -> void:
    var tiles: Dictionary = data.tiles if only.is_empty() else only
    for cell: Vector2i in tiles:
        var tile: Dictionary = tiles[cell]
        if data.objects.has(cell) or tile.get("road", false) or tile.terrain not in ["grass", "highland", "forest", "swamp"]: continue
        var h := posmod(cell.x * 73856093 ^ cell.y * 19349663, 1000)
        if tile.terrain == "grass" and h % 5 != 0: continue
        var tuft := _sprite(str(art_theme.config.scenery.scrub))
        tuft.name = "Scrub_%d_%d" % [cell.x, cell.y]
        _fit_sprite(tuft, float(art_theme.config.scrub_width) + h % 29)
        tuft.position = UmmMapData.cell_to_world(cell) + Vector2(h % 41 - 20, (h >> 2) % 29 - 14)
        tuft.offset.y = -tuft.texture.get_height() * 0.25
        tuft.flip_h = h % 2 == 0
        if tile.terrain == "swamp": tuft.modulate = Color("829573")
        elif tile.terrain == "highland": tuft.modulate = Color("b5b9a2")
        scenery.add_child(tuft)

## Forest hexes become clumps of pines and oaks, Y-sorted with the hero and
## buildings so the expedition walks behind trunks, HoMM3-style.
func _build_trees(scenery: Node2D, only: Dictionary = {}) -> void:
    var tiles: Dictionary = data.tiles if only.is_empty() else only
    for cell: Vector2i in tiles:
        if tiles[cell].terrain != "forest" or data.objects.has(cell):
            continue
        var center := UmmMapData.cell_to_world(cell)
        var h := posmod(cell.x * 73856093 ^ cell.y * 19349663, 1000)
        var offsets := [Vector2(-24, -11), Vector2(21, -6), Vector2(-4, 19)]
        if h % 3 == 0: offsets.resize(2)
        for i in range(offsets.size()):
            var tree_art: Array = art_theme.config.scenery.trees
            var pick: String = tree_art[(h >> i) & 1]
            if h % 5 == 0: pick = tree_art[0]  # some stands are all pine
            var tree := _sprite(pick)
            tree.name = "Tree_%d_%d_%d" % [cell.x, cell.y, i]
            var jitter := Vector2(((h >> (i * 3)) % 23) - 11, ((h >> (i * 2)) % 19) - 9)
            # The sprite is drawn up from its feet so Y-sorting uses the trunk base.
            tree.offset = Vector2(0, -52)
            tree.position = center + offsets[i] + jitter + Vector2(0, 26)
            tree.scale = Vector2.ONE * (0.38 + ((h >> i) % 6) * 0.035)
            scenery.add_child(tree)

## Collected pickups vanish; weekly sites already used this week fade.
func mark_sites(sites: Array) -> void:
    for entry in sites:
        var cell := Vector2i(entry.cell[0], entry.cell[1])
        if not anchors.has(cell): continue
        var kind := str(data.objects[cell].type)
        if kind in ["pickup", "artifact"]:
            anchors[cell].visible = not entry.used
        else:
            anchors[cell].modulate = Color(1, 1, 1, 0.6) if entry.used else Color.WHITE

## The art id for a map object: the most specific candidate that has an image,
## else the most specific one (which shows as a placeholder).
func object_texture(object: Dictionary) -> String:
    if str(object.get("name", "")).ends_with("Rune Stone"):
        return "object.obelisk"
    var type := str(object.type)
    var kind := str(object.get("kind", ""))
    var candidates: Array[String] = []
    if type == "dwelling": candidates.append("object.dwelling." + kind)
    if type == "town" and int(object.get("factionId", 0)) == 2 and data.ground == "grass" and kind != "cruths":
        candidates.append("object.town.shariw_north")
    if not kind.is_empty(): candidates.append("kind." + kind)
    var themed: Dictionary = art_theme.config.get("objects", {})
    if themed.has(type): candidates.append(str(themed[type]))
    candidates.append("object." + type)
    for id in candidates:
        if Art.has_art(id): return id
    return candidates[0]

## Beaten guard camps disappear from the map; `guarded` holds the ones still standing.
func set_cleared_guards(guarded: Dictionary) -> void:
    for cell: Vector2i in anchors:
        if data.objects[cell].type == "guard":
            anchors[cell].visible = guarded.has(cell)

## Story figures who walked off (a "vanish" beat) leave the map for good.
func remove_objects(cells: Array) -> void:
    for c in cells:
        var cell := Vector2i(c[0], c[1])
        if anchors.has(cell):
            anchors[cell].queue_free()
            anchors.erase(cell)
        data.objects.erase(cell)

## Old mines the expedition has ruled out get a "dead end" tag and fade.
func mark_ruled_out(ruled: Dictionary) -> void:
    for cell: Vector2i in anchors:
        if data.objects[cell].type != "old_mine": continue
        var label: Label = anchors[cell].get_node("Label")
        var dead := ruled.has(cell)
        label.text = str(data.objects[cell].name) + ("  ·  dead end" if dead else "")
        anchors[cell].modulate = Color(1, 1, 1, 0.55) if dead else Color.WHITE

func _sprite(art_id: String) -> Sprite2D:
    var sprite := Sprite2D.new()
    sprite.texture = Art.texture(art_id)
    sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    return sprite

func _build_roads() -> void:
    var roads := Node2D.new()
    roads.name = "Roads"
    add_child(roads)
    for cell: Vector2i in data.tiles:
        if not data.tiles[cell].get("road", false):
            continue
        for direction: Vector2i in UmmMapData.DIRECTIONS.slice(0, 3):
            var neighbor := cell + direction
            if not data.tiles.has(neighbor) or not data.tiles[neighbor].get("road", false):
                continue
            var from := UmmMapData.cell_to_world(cell)
            var to := UmmMapData.cell_to_world(neighbor)
            # Bridges draw their own deck; the road just runs up onto it.
            var cell_wet: bool = data.tiles[cell].terrain in WATER_TERRAIN
            var next_wet: bool = data.tiles[neighbor].terrain in WATER_TERRAIN
            if cell_wet and next_wet: continue
            if cell_wet: from = to.lerp(from, 0.6)
            if next_wet: to = from.lerp(to, 0.6)
            var north := data.ground == "grass"
            var line := Line2D.new()
            line.points = PackedVector2Array([from, to])
            line.width = 13.0
            line.default_color = art_theme.colour("road_edge")
            line.begin_cap_mode = Line2D.LINE_CAP_ROUND
            line.end_cap_mode = Line2D.LINE_CAP_ROUND
            roads.add_child(line)
            var center := Line2D.new()
            center.points = line.points
            center.width = 8.0
            center.default_color = art_theme.colour("road")
            roads.add_child(center)

func _build_object(parent: Node2D, cell: Vector2i, object: Dictionary) -> void:
    var anchor := Node2D.new()
    anchor.name = str(object.get("name", object.type)).validate_node_name()
    anchor.position = UmmMapData.cell_to_world(cell)
    parent.add_child(anchor)
    anchors[cell] = anchor
    # Missing art shows this object's own placeholder, never a different building.
    var sprite := _sprite(object_texture(object))
    sprite.position.y = -39
    sprite.scale = Vector2(0.85, 0.85)
    if object.type == "town":
        sprite.scale = Vector2.ONE
        sprite.position.y = -49
        if object.get("name", "") == "Varenhold":
            sprite.scale = Vector2.ONE * 1.28
            sprite.position.y = -62
    var widths: Dictionary = art_theme.config.get("landmark_widths", {})
    if widths.has(str(object.type)):
        _fit_sprite(sprite, float(widths[str(object.type)]))
        sprite.position.y = -sprite.texture.get_height() * sprite.scale.y * 0.34
    anchor.add_child(sprite)
    if object.get("name", "") == "Varenhold":
        var pole := Line2D.new()
        pole.points = PackedVector2Array([Vector2(43, -37), Vector2(43, -105)])
        pole.width = 2
        pole.default_color = Color("b9a37b")
        anchor.add_child(pole)
        var banner := Polygon2D.new()
        banner.polygon = PackedVector2Array([Vector2(44, -105), Vector2(68, -101), Vector2(68, -77), Vector2(56, -82), Vector2(44, -80)])
        banner.color = Color("496c5b")
        anchor.add_child(banner)
    var label := Label.new()
    label.name = "Label"
    label.text = str(object.get("name", object.type))
    if object.get("name", "") == "Varenhold": label.text = "VARENHOLD  ·  HOME"
    label.visible = show_landmark_names
    label.position = Vector2(-95, 7)
    label.size = Vector2(190, 24)
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.add_theme_font_size_override("font_size", 13)
    label.add_theme_color_override("font_color", art_theme.colour("label"))
    label.add_theme_color_override("font_outline_color", art_theme.colour("label_shadow"))
    label.add_theme_constant_override("outline_size", 5)
    anchor.add_child(label)

func _build_water(parent: Node2D, only: Dictionary = {}) -> void:
    var tiles: Dictionary = data.tiles if only.is_empty() else only
    var regions: Array[PackedVector2Array] = []
    for cell: Vector2i in tiles:
        if not tiles[cell].terrain in WATER_TERRAIN:
            continue
        # Slight overlap avoids floating-point cracks when Godot joins hex outlines.
        var merged := UmmMapData.hexagon(UmmMapData.cell_to_world(cell), -0.3)
        var index := 0
        while index < regions.size():
            var union := Geometry2D.merge_polygons(merged, regions[index])
            if union.size() == 1:
                merged = union[0]
                regions.remove_at(index)
                index = 0
            else:
                index += 1
        regions.append(merged)
    for outline in regions:
        # Inset the shared outline once, rather than outlining every water cell.
        var shores := Geometry2D.offset_polygon(outline, -10.0, Geometry2D.JOIN_ROUND)
        for shore in shores:
            for iteration in range(2):
                var rounded := PackedVector2Array()
                for i in range(shore.size()):
                    var next: Vector2 = shore[(i + 1) % shore.size()]
                    rounded.append(shore[i].lerp(next, 0.25))
                    rounded.append(shore[i].lerp(next, 0.75))
                shore = rounded
            var water := Polygon2D.new()
            water.name = "OasisWater"
            water.polygon = shore
            var material := ShaderMaterial.new()
            material.shader = WATER_SHADER
            water.material = material
            parent.add_child(water)
            var bank := Line2D.new()
            bank.name = "OasisShore"
            bank.points = shore
            bank.closed = true
            bank.width = 6.0
            bank.default_color = art_theme.colour("shore")
            bank.joint_mode = Line2D.LINE_JOINT_ROUND
            parent.add_child(bank)
    if only.is_empty(): _build_bridges(parent)

## True for a bridge whose gate is still barred.
func is_shut_bridge(cell: Vector2i) -> bool:
    return gates.has(cell) and not opened.has(cell)

## Cells the story opened (snapshot "opened": [[q, r], …]): their gates swing
## aside and the tiles become walkable for the client's own route and hover logic.
func set_opened(cells: Array) -> void:
    for c in cells:
        var cell := Vector2i(int(c[0]), int(c[1]))
        if opened.has(cell): continue
        opened[cell] = true
        if data.tiles.has(cell): data.tiles[cell]["passable"] = true
        if gates.has(cell): _show_gate(gates[cell], false)

static func _show_gate(gate: Node2D, shut: bool) -> void:
    gate.get_node("Barrier").visible = shut
    gate.get_node("Open").visible = not shut
    gate.set_meta("shut", shut)

## A passable water cell is a bridge: timber planks laid along its road. A road
## over water that is not passable is a shut bridge: the same deck, barred by a gate.
func _build_bridges(parent: Node2D) -> void:
    for cell: Vector2i in data.tiles:
        var tile: Dictionary = data.tiles[cell]
        if not tile.terrain in WATER_TERRAIN or tile.terrain == "oasis":
            continue
        var shut: bool = not tile.get("passable", false) and tile.get("road", false)
        if not tile.get("passable", false) and not shut:
            continue
        var center := UmmMapData.cell_to_world(cell)
        var ends: Array[Vector2] = []
        for direction: Vector2i in UmmMapData.DIRECTIONS:
            var next := cell + direction
            if data.tiles.has(next) and data.is_passable(next) and not data.tiles[next].terrain in WATER_TERRAIN:
                ends.append(UmmMapData.cell_to_world(next))
        if ends.size() < 2: continue
        # Span between the two most opposite banks.
        var a := ends[0]
        var b := ends[1]
        for i in range(ends.size()):
            for j in range(i + 1, ends.size()):
                if ends[i].distance_to(ends[j]) > a.distance_to(b):
                    a = ends[i]
                    b = ends[j]
        a = center.lerp(a, 0.62)
        b = center.lerp(b, 0.62)
        var deck := Line2D.new()
        deck.name = "Bridge_%d_%d" % [cell.x, cell.y]
        deck.points = PackedVector2Array([a, b])
        deck.width = 30.0
        deck.default_color = Color("5a3e22")
        parent.add_child(deck)
        var planks := Line2D.new()
        planks.points = deck.points
        planks.width = 24.0
        planks.default_color = Color("9a7446")
        parent.add_child(planks)
        var across := (b - a).orthogonal().normalized() * 11.0
        for k in range(1, 9):
            var p := a.lerp(b, k / 9.0)
            var seam := Line2D.new()
            seam.points = PackedVector2Array([p - across, p + across])
            seam.width = 1.5
            seam.default_color = Color("5a3e22")
            parent.add_child(seam)
        if shut or opened.has(cell):
            var gate := _gate(a.lerp(b, 0.5), (b - a).orthogonal().normalized())
            gate.name = "Gate_%d_%d" % [cell.x, cell.y]
            parent.add_child(gate)
            gates[cell] = gate
            _show_gate(gate, not opened.has(cell))

## A gate across the deck at `at` (`across` runs from rail to rail): a stout
## post at each rail always; when shut, two pale beams crossed flat over the deck
## from rail to rail; when open, only the posts and one beam laid by the rail.
func _gate(at: Vector2, across: Vector2) -> Node2D:
    var gate := Node2D.new()
    var ink := Color("2a1a0e")
    var wood := Color("5a3e22")
    var light := Color("c49a5c")
    var along := across.orthogonal()
    var barrier := Node2D.new()
    barrier.name = "Barrier"
    for d in [1.0, -1.0]:   # an X of beams nailed across the planks, rail to rail
        barrier.add_child(_beam(at + along * 14.0 * d + across * -14.0, at - along * 14.0 * d + across * 14.0, 8.0, ink, light))
    gate.add_child(barrier)
    var leaves := Node2D.new()
    leaves.name = "Open"
    leaves.add_child(_beam(at + across * 15.0 + along * 3.0 + Vector2(0, -2), at + across * 15.0 + along * 17.0 + Vector2(0, -2), 5.0, ink, light))
    gate.add_child(leaves)
    for side in [-1.0, 1.0]:
        gate.add_child(_stake(at + across * 16.0 * side, 6.0, 14.0, ink, wood))
    return gate

## A timber beam from `a` to `b`: an ink outline under a wood fill.
static func _beam(a: Vector2, b: Vector2, width: float, ink: Color, fill: Color) -> Node2D:
    var beam := Node2D.new()
    for layer in [[ink, width + 2.0], [fill, width]]:
        var line := Line2D.new()
        line.points = PackedVector2Array([a, b])
        line.default_color = layer[0]
        line.width = layer[1]
        beam.add_child(line)
    return beam

## One upright pointed stake standing on `foot`, outlined in `ink`.
static func _stake(foot: Vector2, width: float, height: float, ink: Color, fill: Color) -> Polygon2D:
    var half := width / 2.0
    var shape := PackedVector2Array([foot + Vector2(-half, 0), foot + Vector2(-half, -height), foot + Vector2(0, -height - half - 2),
        foot + Vector2(half, -height), foot + Vector2(half, 0)])
    var outline := Polygon2D.new()
    var grown := Geometry2D.offset_polygon(shape, 1.0)
    outline.polygon = grown[0] if not grown.is_empty() else shape
    outline.color = ink
    var body := Polygon2D.new()
    body.polygon = shape
    body.color = fill
    outline.add_child(body)
    return outline

## The rings of land around the map: each takes the terrain of the map cell
## it grew from, so forest runs on as forest and water as water.
func _find_margin() -> void:
    var frontier: Array = data.tiles.keys()
    for ring in range(1, MARGIN_RINGS + 1):
        var next: Array = []
        for cell: Vector2i in frontier:
            var source: Vector2i = cell if data.tiles.has(cell) else margin[cell].source
            for direction: Vector2i in UmmMapData.DIRECTIONS:
                var neighbor := cell + direction
                if data.tiles.has(neighbor) or margin.has(neighbor): continue
                margin[neighbor] = {"ring": ring, "source": source, "terrain": str(data.tiles[source].terrain)}
                next.append(neighbor)
        frontier = next

func _margin_ring(cell: Vector2i) -> int:
    if data.tiles.has(cell): return 0
    return int(margin[cell].ring) if margin.has(cell) else MARGIN_RINGS + 1

## Margin scenery (darkened, under the map's own scenery), then one mesh that
## veils it: faint at the map's edge, solid background a few hexes out.
func _build_margin(north: bool) -> void:
    if margin.is_empty(): return
    var far := Node2D.new()
    far.name = "MarginScenery"
    far.modulate = Color(0.72, 0.72, 0.72)
    add_child(far)
    var ground_tiles := {}
    var tall_tiles := {}
    for cell: Vector2i in margin:
        var tile := {"q": cell.x, "r": cell.y, "terrain": margin[cell].terrain}
        if tile.terrain in WATER_TERRAIN:
            tile["passable"] = false
            ground_tiles[cell] = tile
        elif tile.terrain == "snow":   # flat ground art, laid under the map's own features
            var drift := _snow_patch(cell)
            drift.name = "MarginSnow_%d_%d" % [cell.x, cell.y]
            drift.modulate = far.modulate
            if not has_node("MarginGround"):
                var flat := Node2D.new()
                flat.name = "MarginGround"
                add_child(flat)
                move_child(flat, get_node("TerrainFeatures").get_index())
            get_node("MarginGround").add_child(drift)
        elif _clear_above(cell):   # tall art here would reach up over the playable map
            tall_tiles[cell] = tile
    if not ground_tiles.is_empty(): _build_water(far, ground_tiles)
    if north:
        if not tall_tiles.is_empty(): _build_trees(far, tall_tiles)
        for cell: Vector2i in tall_tiles:
            if tall_tiles[cell].terrain != "mountain": continue
            var hill := _sprite(str(art_theme.config.scenery.ridge))
            hill.name = "MarginHill_%d_%d" % [cell.x, cell.y]
            _fit_sprite(hill, 112.0)
            hill.position = UmmMapData.cell_to_world(cell) + Vector2(0, 22)
            hill.offset.y = -hill.texture.get_height() * 0.37
            hill.flip_h = posmod(cell.x + cell.y, 2) == 0
            far.add_child(hill)
    else:
        for cell: Vector2i in tall_tiles:
            var terrain := str(tall_tiles[cell].terrain)
            if terrain == data.ground or not Art.is_listed("feature." + terrain): continue
            var feature := _sprite("feature." + terrain)
            feature.position = UmmMapData.cell_to_world(cell)
            feature.scale = Vector2(0.8, 0.65)
            var material := ShaderMaterial.new()
            material.shader = PATCH_SHADER
            feature.material = material
            far.add_child(feature)
    add_child(_margin_veil())   # a sibling, so the darkening does not tint the veil

func _clear_above(cell: Vector2i) -> bool:
    for direction in [Vector2i(0, -1), Vector2i(1, -1), Vector2i(-1, 0)]:
        if data.tiles.has(cell + direction): return false
    return true

## Triangles over every non-map cell in the ground's reach. Each hex is a fan
## whose corners average the rings of the three cells that meet there, so the
## fade is smooth and the map's edge itself stays clear.
func _margin_veil() -> MeshInstance2D:
    var shade: Color = ProjectSettings.get_setting("rendering/environment/defaults/default_clear_color", Color(0.075, 0.06, 0.045))
    var area := Rect2(world_bounds)
    for cell: Vector2i in margin: area = area.expand(UmmMapData.cell_to_world(cell))
    area = area.grow(UmmMapData.HEX_SIZE.x)
    var step := UmmMapData.HEX_SIZE.x * 0.75
    var corner_cells := [[Vector2i(1, 0), Vector2i(1, -1)], [Vector2i(1, 0), Vector2i(0, 1)], [Vector2i(0, 1), Vector2i(-1, 1)],
        [Vector2i(-1, 1), Vector2i(-1, 0)], [Vector2i(-1, 0), Vector2i(0, -1)], [Vector2i(0, -1), Vector2i(1, -1)]]
    var points := PackedVector2Array()
    var colours := PackedColorArray()
    for q in range(floori(area.position.x / step), ceili(area.end.x / step) + 1):
        var r_from := floori(area.position.y / UmmMapData.HEX_SIZE.y - q * 0.5)
        var r_to := ceili(area.end.y / UmmMapData.HEX_SIZE.y - q * 0.5)
        for r in range(r_from, r_to + 1):
            var cell := Vector2i(q, r)
            if data.tiles.has(cell): continue
            var center := UmmMapData.cell_to_world(cell)
            var corners := UmmMapData.hexagon(center)   # no overlap: seams would blend twice
            var own := _margin_ring(cell)
            var middle := Color(shade, MARGIN_FADE[own])
            for i in 6:
                var j := (i + 1) % 6
                points.append_array([center, corners[i], corners[j]])
                if own > MARGIN_RINGS:   # past the margin: solid, so no ground shows through
                    colours.append_array([middle, middle, middle])
                else:
                    colours.append_array([middle, _corner_shade(shade, cell, own, corner_cells[i]), _corner_shade(shade, cell, own, corner_cells[j])])
    var arrays := []
    arrays.resize(Mesh.ARRAY_MAX)
    arrays[Mesh.ARRAY_VERTEX] = points
    arrays[Mesh.ARRAY_COLOR] = colours
    var mesh := ArrayMesh.new()
    mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
    var veil := MeshInstance2D.new()
    veil.name = "MarginVeil"
    veil.mesh = mesh
    return veil

func _corner_shade(shade: Color, cell: Vector2i, own: int, touching: Array) -> Color:
    var rings := float(own + _margin_ring(cell + touching[0]) + _margin_ring(cell + touching[1])) / 3.0
    var low := floori(rings)
    var fade: float = MARGIN_FADE[low] if low >= MARGIN_FADE.size() - 1 else lerpf(MARGIN_FADE[low], MARGIN_FADE[low + 1], rings - low)
    return Color(shade, fade)
