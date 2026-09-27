@tool
extends Node2D
## Native Godot scene nodes compose the map in both the editor and the game.

const ArtTheme = preload("res://scripts/map_art_theme.gd")
const MEADOW_SHADER = preload("res://shaders/north_meadow.gdshader")
var art_theme = ArtTheme.new()
var mountain_groups: Array = []

const PATCH_SHADER = preload("res://shaders/terrain_patch.gdshader")
const WATER_SHADER = preload("res://shaders/water.gdshader")
const OBJECT_TEXTURES := {
    "town": "objects/town.png", "dungeon": "objects/dungeon.png",
    "gold_mine": "objects/goldmine.png", "crystal_mine": "objects/crystal_mine.png",
    "artifact": "objects/artifact.png", "sawmill": "objects/sawmill.png",
    "quarry": "objects/quarry.png", "obsidian_vent": "objects/obsidian_vent.png",
    "old_mine": "objects/old_mine.png", "quest_giver": "objects/quest_giver.png",
    "guard": "objects/guard.png", "watchtower": "objects/watchtower.png",
    "stables": "objects/stables.png", "learning_stone": "objects/learning_stone.png",
    "obelisk": "objects/obelisk.png",
}
## Sprite by object kind (pickups, mills, dwellings, relic artifacts).
const KIND_TEXTURES := {
    "wood": "objects/wood_pile.png", "stone": "objects/stone_pile.png", "gold": "objects/gold_pile.png",
    "crystal": "objects/crystal_pile.png", "obsidian": "objects/obsidian_pile.png",
    "chest": "objects/treasure_chest.png", "campfire": "objects/campfire.png",
    "windmill": "objects/windmill.png", "watermill": "objects/watermill.png",
    "listening_shard": "objects/veined_relic.png", "warm_stone": "objects/veined_relic.png",
    "druid": "objects/druid.png",
}
const WATER_TERRAIN := ["oasis", "lake", "river"]
const TREE_SPRITES := ["terrain/forest/pine_cluster.png", "terrain/forest/oak_cluster.png"]
var anchors: Dictionary = {}  # cell -> object anchor node
const FEATURE_TEXTURES := {
    "dune": "terrain/dune/dune.png", "mountain": "terrain/mountain/mountain2.png",
    "oasis": "terrain/oasis/oasis1.png", "ruins": "terrain/ruins/ruins1.png",
    "rock": "terrain/rock/rock1.png", "obsidian": "terrain/obsidian/obsidian0.png",
    "river": "terrain/river/river.png", "forest": "terrain/forest/forest.png",
    "grass": "terrain/grass/grass.png", "highland": "terrain/highland/highland.png",
    "wall": "terrain/wall/wall.png", "swamp": "terrain/highland/highland1.png",
}
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

func _ready() -> void:
    rebuild()

func rebuild() -> void:
    for child in get_children():
        child.free()
    anchors.clear()
    mountain_groups.clear()
    if not data.read(map_path):
        push_error(data.error)
        return
    var edge_points := PackedVector2Array()
    for cell: Vector2i in data.tiles:
        edge_points.append_array(UmmMapData.hexagon(UmmMapData.cell_to_world(cell)))
    var outline := Geometry2D.convex_hull(edge_points)
    world_bounds = Rect2(outline[0], Vector2.ZERO)
    for point in outline:
        world_bounds = world_bounds.expand(point)
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
    var grain_path := "res://content/textures/terrain/sand/sand_seamless.png"
    if north:
        grain_path = "res://content/textures/terrain/grass/grasstexture.png"
    elif not ResourceLoader.exists(grain_path):
        # The seamless texture is optional worktree art; tracked assets also run.
        grain_path = "res://content/textures/terrain/sand/sand1.png"
    grain.texture = load(grain_path)
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
        if not FEATURE_TEXTURES.has(tile.terrain):
            continue
        var art: String = FEATURE_TEXTURES[tile.terrain]
        var feature := _sprite(art)
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
func _build_undergrowth(scenery: Node2D) -> void:
    for cell: Vector2i in data.tiles:
        var tile: Dictionary = data.tiles[cell]
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
func _build_trees(scenery: Node2D) -> void:
    for cell: Vector2i in data.tiles:
        if data.tiles[cell].terrain != "forest" or data.objects.has(cell):
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

func object_texture(object: Dictionary) -> String:
    var type := str(object.type)
    var kind := str(object.get("kind", ""))
    var candidates: Array[String] = []
    if type == "dwelling": candidates.append("objects/dwelling_%s.png" % kind)
    if type == "town" and int(object.get("factionId", 0)) == 2 and data.ground == "grass":
        candidates.append("objects/town_shariw_north.png")
    if KIND_TEXTURES.has(kind): candidates.append(KIND_TEXTURES[kind])
    var themed: Dictionary = art_theme.config.get("objects", {})
    if themed.has(type): candidates.append(str(themed[type]))
    if OBJECT_TEXTURES.has(type): candidates.append(OBJECT_TEXTURES[type])
    for path in candidates:
        if ResourceLoader.exists("res://content/textures/" + path): return path
    return ""

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

var _theme_textures: Dictionary = {}

func _sprite(relative_path: String) -> Sprite2D:
    var sprite := Sprite2D.new()
    if relative_path.begins_with("themes/"):
        if not _theme_textures.has(relative_path):
            var source: Texture2D = load("res://content/textures/" + relative_path)
            var bitmap := source.get_image()
            if not bitmap.has_mipmaps(): bitmap.generate_mipmaps()
            _theme_textures[relative_path] = ImageTexture.create_from_image(bitmap)
        sprite.texture = _theme_textures[relative_path]
    else:
        sprite.texture = load("res://content/textures/" + relative_path)
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
    var art := object_texture(object)
    if not art.is_empty():
        var sprite := _sprite(art)
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
    else:
        # Missing art remains an explicit map marker, never a different building.
        var marker := Polygon2D.new()
        marker.polygon = PackedVector2Array([Vector2(0, -22), Vector2(13, -9), Vector2(0, 4), Vector2(-13, -9)])
        marker.color = Color("685040")
        anchor.add_child(marker)
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

func _build_water(parent: Node2D) -> void:
    var regions: Array[PackedVector2Array] = []
    for cell: Vector2i in data.tiles:
        if not data.tiles[cell].terrain in WATER_TERRAIN:
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
    _build_bridges(parent)

## A passable water cell is a bridge: timber planks laid along its road.
func _build_bridges(parent: Node2D) -> void:
    for cell: Vector2i in data.tiles:
        var tile: Dictionary = data.tiles[cell]
        if not tile.terrain in WATER_TERRAIN or not tile.get("passable", false) or tile.terrain == "oasis":
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
