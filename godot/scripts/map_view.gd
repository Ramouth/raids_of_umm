@tool
extends Node2D
## Native Godot scene nodes compose the map in both the editor and the game.

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
## Northern art for the same object types, used on grass-ground maps.
const NORTH_TEXTURES := {
    "town": "objects/town_north.png", "sawmill": "objects/sawmill_north.png",
    "gold_mine": "objects/gold_mine_north.png", "guard": "objects/guard_north.png",
    "dungeon": "objects/dungeon_north.png", "artifact": "objects/cairn.png",
    "old_mine": "objects/old_mine_north.png", "quest_giver": "objects/quest_giver_north.png",
}
const NORTH_MOUNTAINS := ["terrain/mountain/mountain_north.png", "terrain/mountain/mountain_north2.png"]
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
## On grass-ground maps these replace the desert-era hex art.
const NORTH_FEATURES := {"highland": "terrain/highland/highland2.png"}

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
    var ground := Polygon2D.new()
    ground.name = "GrassFoundation" if north else "SandFoundation"
    ground.polygon = outline
    ground.color = grass_color if north else sand_color
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
    add_child(grain)
    var terrain := Node2D.new()
    terrain.name = "TerrainFeatures"
    add_child(terrain)
    for cell: Vector2i in data.tiles:
        var tile: Dictionary = data.tiles[cell]
        if tile.terrain in WATER_TERRAIN or tile.terrain == data.ground:
            continue
        if north and tile.terrain == "forest":
            continue  # trees are scenery: they Y-sort with the hero (see _build_trees)
        if not FEATURE_TEXTURES.has(tile.terrain):
            continue
        var art: String = NORTH_FEATURES.get(tile.terrain, FEATURE_TEXTURES[tile.terrain]) if north else FEATURE_TEXTURES[tile.terrain]
        if north and tile.terrain == "mountain":
            var peak: String = NORTH_MOUNTAINS[posmod(cell.x * 7 + cell.y * 13, NORTH_MOUNTAINS.size())]
            if ResourceLoader.exists("res://content/textures/" + peak): art = peak
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
        _build_trees(scenery)

## Forest hexes become clumps of pines and oaks, Y-sorted with the hero and
## buildings so the expedition walks behind trunks, HoMM3-style.
func _build_trees(scenery: Node2D) -> void:
    for cell: Vector2i in data.tiles:
        if data.tiles[cell].terrain != "forest" or data.objects.has(cell):
            continue
        var center := UmmMapData.cell_to_world(cell)
        var h := posmod(cell.x * 73856093 ^ cell.y * 19349663, 1000)
        var offsets := [Vector2(-18, -8), Vector2(16, -4), Vector2(-2, 14)]
        for i in range(offsets.size()):
            var pick: String = TREE_SPRITES[(h >> i) & 1]
            if h % 5 == 0: pick = TREE_SPRITES[0]  # some stands are all pine
            var tree := _sprite(pick)
            tree.name = "Tree_%d_%d_%d" % [cell.x, cell.y, i]
            var jitter := Vector2(((h >> (i * 3)) % 7) - 3, ((h >> (i * 2)) % 5) - 2)
            # The sprite is drawn up from its feet so Y-sorting uses the trunk base.
            tree.offset = Vector2(0, -52)
            tree.position = center + offsets[i] + jitter + Vector2(0, 26)
            tree.scale = Vector2.ONE * (0.42 + ((h >> i) % 4) * 0.03)
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
    if data.ground == "grass" and NORTH_TEXTURES.has(type): candidates.append(NORTH_TEXTURES[type])
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

## Old mines Kharim has ruled out get a "dead end" tag and fade.
func mark_ruled_out(ruled: Dictionary) -> void:
    for cell: Vector2i in anchors:
        if data.objects[cell].type != "old_mine": continue
        var label: Label = anchors[cell].get_node("Label")
        var dead := ruled.has(cell)
        label.text = str(data.objects[cell].name) + ("  ·  dead end" if dead else "")
        anchors[cell].modulate = Color(1, 1, 1, 0.55) if dead else Color.WHITE

func _sprite(relative_path: String) -> Sprite2D:
    var sprite := Sprite2D.new()
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
            line.default_color = Color("6e5231") if north else Color("ac7935")
            line.begin_cap_mode = Line2D.LINE_CAP_ROUND
            line.end_cap_mode = Line2D.LINE_CAP_ROUND
            roads.add_child(line)
            var center := Line2D.new()
            center.points = line.points
            center.width = 8.0
            center.default_color = Color("a3845a") if north else Color("e0b762")
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
        anchor.add_child(sprite)
    else:
        # Missing art remains an explicit map marker, never a different building.
        var marker := Polygon2D.new()
        marker.polygon = PackedVector2Array([Vector2(0, -22), Vector2(13, -9), Vector2(0, 4), Vector2(-13, -9)])
        marker.color = Color("685040")
        anchor.add_child(marker)
    var label := Label.new()
    label.name = "Label"
    label.text = str(object.get("name", object.type))
    label.visible = show_landmark_names
    label.position = Vector2(-95, 7)
    label.size = Vector2(190, 24)
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.add_theme_font_size_override("font_size", 13)
    label.add_theme_color_override("font_color", Color("fff0c0"))
    label.add_theme_color_override("font_outline_color", Color("352018"))
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
            bank.default_color = Color("7d6a43") if data.ground == "grass" else Color("ead39a")
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
