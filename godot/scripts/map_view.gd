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
    "guard": "objects/guard.png",
}
var anchors: Dictionary = {}  # cell -> object anchor node
const FEATURE_TEXTURES := {
    "dune": "terrain/dune/dune.png", "mountain": "terrain/mountain/mountain2.png",
    "oasis": "terrain/oasis/oasis1.png", "ruins": "terrain/ruins/ruins1.png",
    "rock": "terrain/rock/rock1.png", "obsidian": "terrain/obsidian/obsidian0.png",
    "river": "terrain/river/river.png", "forest": "terrain/forest/forest.png",
    "grass": "terrain/grass/grass.png", "highland": "terrain/highland/highland.png",
    "wall": "terrain/wall/wall.png",
}

@export_file("*.json") var map_path := "res://content/maps/old_passage.json"
@export_group("Art direction")
@export var sand_color := Color("c89943")
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
    var ground := Polygon2D.new()
    ground.name = "SandFoundation"
    ground.polygon = outline
    ground.color = sand_color
    add_child(ground)
    var grain := Polygon2D.new()
    grain.name = "SandGrain"
    grain.polygon = outline
    var grain_path := "res://content/textures/terrain/sand/sand_seamless.png"
    if not ResourceLoader.exists(grain_path):
        # The seamless texture is optional worktree art; tracked assets also run.
        grain_path = "res://content/textures/terrain/sand/sand1.png"
    grain.texture = load(grain_path)
    grain.texture_repeat = CanvasItem.TEXTURE_REPEAT_ENABLED
    grain.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    grain.color = Color(1, 1, 1, sand_detail_opacity)
    add_child(grain)
    var terrain := Node2D.new()
    terrain.name = "TerrainFeatures"
    add_child(terrain)
    for cell: Vector2i in data.tiles:
        var tile: Dictionary = data.tiles[cell]
        if tile.terrain == "oasis":
            continue
        if not FEATURE_TEXTURES.has(tile.terrain):
            continue
        var feature := _sprite(FEATURE_TEXTURES[tile.terrain])
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
            feature.scale = Vector2.ONE * (0.8 + variation * 0.04)
            feature.position += Vector2(variation - 3, -18 - variation)
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

## Beaten guard camps disappear from the map; `guarded` holds the ones still standing.
func set_cleared_guards(guarded: Dictionary) -> void:
    for cell: Vector2i in anchors:
        if data.objects[cell].type == "guard":
            anchors[cell].visible = guarded.has(cell)

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
            var line := Line2D.new()
            line.points = PackedVector2Array([UmmMapData.cell_to_world(cell), UmmMapData.cell_to_world(neighbor)])
            line.width = 13.0
            line.default_color = Color("ac7935")
            line.begin_cap_mode = Line2D.LINE_CAP_ROUND
            line.end_cap_mode = Line2D.LINE_CAP_ROUND
            roads.add_child(line)
            var center := Line2D.new()
            center.points = line.points
            center.width = 8.0
            center.default_color = Color("e0b762")
            roads.add_child(center)

func _build_object(parent: Node2D, cell: Vector2i, object: Dictionary) -> void:
    var anchor := Node2D.new()
    anchor.name = str(object.get("name", object.type)).validate_node_name()
    anchor.position = UmmMapData.cell_to_world(cell)
    parent.add_child(anchor)
    anchors[cell] = anchor
    if OBJECT_TEXTURES.has(object.type):
        var sprite := _sprite(OBJECT_TEXTURES[object.type])
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
        if data.tiles[cell].terrain != "oasis":
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
            bank.default_color = Color("ead39a")
            bank.joint_mode = Line2D.LINE_JOINT_ROUND
            parent.add_child(bank)
