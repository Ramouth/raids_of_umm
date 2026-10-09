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
    _build_snow(terrain)
    for cell: Vector2i in data.tiles:
        var tile: Dictionary = data.tiles[cell]
        if tile.terrain in WATER_TERRAIN or tile.terrain == data.ground or tile.terrain == "snow":
            continue
        if north and tile.terrain in ["forest", "mountain", "highland", "swamp"]:
            continue  # Northern relief is composed as scenery, without polygon-shaped tile bases.
        if not Art.is_listed("feature." + tile.terrain):
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

## The snowy north is one continuous layer, not a sprite per hex, so no cell
## outline shows. A coarse field holds, per point: (red) the snow fraction of the
## land around it (snow hexes, map or margin, count fully; forest and ridges
## touching snow count half, since snow lies under those trees and rocks too),
## smoothed with a Gaussian across neighbouring hexes; (green) "dust", how near
## the nearest snow hex is, 1 on it and 0 about two hexes out; (blue) "trodden",
## near a camp or site standing in the snow, whose ground is thin snow broken by
## frost and grass rather than a deep drift around a green hole. The shader
## (snow_field.gdshader) draws deep snow where red (less SNOW_TRODDEN x blue) is
## high, with a low-frequency drift outline, and beyond it frost on the grass:
## irregular patches, thick beside the snow, fewer and smaller two hexes out.
## Woods and ridges with dust >= SNOW_CAP get snow on their tops.
const SNOW_SHADER = preload("res://shaders/snow_field.gdshader")
const SNOW_DUST_SHADER = preload("res://shaders/snow_dust.gdshader")
const SNOW_STEP := 8.0      # world units per density texel
const SNOW_SIGMA := 38.0    # smoothing radius, about half a hex
const SNOW_SPACING := 87.0  # average distance between neighbouring hex centres
const SNOW_REACH := 2.0     # hexes of frosted grass beyond the snow
const SNOW_CAP := 0.2       # dust at which trees and ridges carry snow (about two hexes out)
const SNOW_TRODDEN := 0.75  # how much a camp in the snow thins it (snow_field.gdshader's `trodden`)
const SNOW_CAMP_SIGMA := 55.0   # reach of that thinning, a little over half a hex
var snow_density: Image     # the field (red = snow, green = dust, blue = trodden), null when the map has no snow
var snow_origin := Vector2.ZERO

func _terrain_at(cell: Vector2i) -> String:
    if data.tiles.has(cell): return str(data.tiles[cell].terrain)
    if margin.has(cell): return str(margin[cell].terrain)
    return ""

func _touches_snow(cell: Vector2i) -> bool:
    for direction: Vector2i in UmmMapData.DIRECTIONS:
        if _terrain_at(cell + direction) == "snow": return true
    return false

func _snow_weight(cell: Vector2i) -> float:
    var terrain := _terrain_at(cell)
    if terrain == "snow": return 1.0
    if terrain in ["forest", "mountain"] and _touches_snow(cell): return 0.5
    return 0.0

## Snow fraction (0..1) at a world point, as the shader sees it.
func snow_at(point: Vector2) -> float:
    if snow_density == null: return 0.0
    var texel := ((point - snow_origin) / SNOW_STEP - Vector2(0.5, 0.5)).round()
    var x := clampi(int(texel.x), 0, snow_density.get_width() - 1)
    var y := clampi(int(texel.y), 0, snow_density.get_height() - 1)
    var texel_value := snow_density.get_pixel(x, y)
    return maxf(0.0, texel_value.r - texel_value.b * SNOW_TRODDEN)

## Dust (0..1) at a world point: how thickly the grass there is dotted with snow.
func dust_at(point: Vector2) -> float:
    if snow_density == null: return 0.0
    var texel := ((point - snow_origin) / SNOW_STEP - Vector2(0.5, 0.5)).round()
    if texel.x < 0 or texel.y < 0 or texel.x >= snow_density.get_width() or texel.y >= snow_density.get_height(): return 0.0
    return snow_density.get_pixel(int(texel.x), int(texel.y)).g

## Trees and ridges this close to the snow carry it on their tops.
func _snow_capped(cell: Vector2i) -> bool:
    return dust_at(UmmMapData.cell_to_world(cell)) >= SNOW_CAP

## A material that lays snow on the upper edges of a sprite (snow_dust.gdshader).
## `depth` is in texels of that sprite's texture.
func _snow_cap_material(depth: float, cap := 1.0, rock := false) -> ShaderMaterial:
    var material := ShaderMaterial.new()
    material.shader = SNOW_DUST_SHADER
    material.set_shader_parameter("depth", depth)
    material.set_shader_parameter("cap", cap)
    material.set_shader_parameter("rock", rock)
    return material

## Ridge art near the snow wears snow caps on its upper rock.
func _cap_ridge(sprite: Sprite2D, cells: Array) -> void:
    for cell: Vector2i in cells:
        if _snow_capped(cell):
            sprite.material = _snow_cap_material(sprite.texture.get_height() * 0.03, 0.5, true)
            return

func _build_snow(parent: Node2D) -> void:
    snow_density = null
    var cells: Array[Vector2i] = []
    for cell: Vector2i in data.tiles:
        if data.tiles[cell].terrain == "snow": cells.append(cell)
    for cell: Vector2i in margin:
        if margin[cell].terrain == "snow": cells.append(cell)
    if cells.is_empty(): return
    var area := Rect2(UmmMapData.cell_to_world(cells[0]), Vector2.ZERO)
    for cell in cells: area = area.expand(UmmMapData.cell_to_world(cell))
    area = area.grow(SNOW_SPACING * (SNOW_REACH + 0.6))
    var size := Vector2i(ceili(area.size.x / SNOW_STEP), ceili(area.size.y / SNOW_STEP))
    snow_origin = area.position
    var weights := {}
    var field := Image.create_empty(size.x, size.y, false, Image.FORMAT_RGB8)
    var centres := PackedVector2Array()
    for cell in cells: centres.append(UmmMapData.cell_to_world(cell))
    # Camps and sites in the snow: their ground is trodden to thin snow and frost.
    var camps := {}
    for cell: Vector2i in data.objects:
        var snowy := 0
        for direction: Vector2i in UmmMapData.DIRECTIONS:
            if _terrain_at(cell + direction) == "snow": snowy += 1
        if _terrain_at(cell) == "snow" or snowy >= 3: camps[cell] = true
    var reach := []
    for dq in range(-2, 3):
        for dr in range(-2, 3):
            if absi(dq + dr) <= 2: reach.append(Vector2i(dq, dr))
    for y in size.y:
        for x in size.x:
            var point := snow_origin + (Vector2(x, y) + Vector2(0.5, 0.5)) * SNOW_STEP
            var home := UmmMapData.world_to_cell(point)
            var snow := 0.0
            var trodden := 0.0
            var total := 0.0
            for offset: Vector2i in reach:
                var cell: Vector2i = home + offset
                var k := exp(-point.distance_squared_to(UmmMapData.cell_to_world(cell)) / (2.0 * SNOW_SIGMA * SNOW_SIGMA))
                if not weights.has(cell): weights[cell] = _snow_weight(cell)
                snow += k * weights[cell]
                if camps.has(cell): trodden += exp(-point.distance_squared_to(UmmMapData.cell_to_world(cell)) / (2.0 * SNOW_CAMP_SIGMA * SNOW_CAMP_SIGMA))
                total += k
            var nearest := INF
            for centre in centres: nearest = minf(nearest, point.distance_squared_to(centre))
            var dust := clampf(1.0 - (sqrt(nearest) - SNOW_SPACING * 0.5) / (SNOW_SPACING * SNOW_REACH), 0.0, 1.0)
            field.set_pixel(x, y, Color(snow / total, dust, minf(1.0, trodden)))
    snow_density = field
    var layer := Polygon2D.new()
    layer.name = "SnowField"
    var extent := Vector2(size) * SNOW_STEP
    layer.polygon = PackedVector2Array([snow_origin, snow_origin + Vector2(extent.x, 0), snow_origin + extent, snow_origin + Vector2(0, extent.y)])
    layer.texture = Art.texture("feature.snow")
    layer.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    layer.texture_repeat = CanvasItem.TEXTURE_REPEAT_ENABLED
    var material := ShaderMaterial.new()
    material.shader = SNOW_SHADER
    material.set_shader_parameter("density", ImageTexture.create_from_image(field))
    material.set_shader_parameter("origin", snow_origin)
    material.set_shader_parameter("extent", extent)
    material.set_shader_parameter("trodden", SNOW_TRODDEN)
    layer.material = material
    parent.add_child(layer)

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
        _cap_ridge(ridge, group)
        # A wide ridge cannot cover the vertical span of an axial group.
        # Give each blocked cell its own visible foothill before adding the crest.
        for cell: Vector2i in group:
            var foothill := _sprite(str(art_theme.config.scenery.ridge))
            foothill.name = "MountainFootprint_%d_%d" % [cell.x, cell.y]
            _fit_sprite(foothill, 112.0)
            foothill.position = UmmMapData.cell_to_world(cell) + Vector2(0, 22)
            foothill.offset.y = -foothill.texture.get_height() * 0.37
            foothill.flip_h = posmod(cell.x + cell.y, 2) == 0
            _cap_ridge(foothill, [cell])
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
        var dusted := _snow_capped(cell)   # woods near the snow carry it on their crowns
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
            if dusted: tree.material = _snow_cap_material(7.0)
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
        elif tile.terrain == "snow":
            pass   # part of the SnowField layer (_build_snow), laid under the map's features
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
            _cap_ridge(hill, [cell])
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

## One layer over the whole map and its margin; its shader (margin_veil.gdshader)
## blends the margin rings of the hexes around each point, so the fade is one
## smooth sweep with no hex-shaped bands: faint at the map's edge, solid
## background a few hexes out.
const VEIL_SHADER = preload("res://shaders/margin_veil.gdshader")

func _margin_veil() -> Polygon2D:
    var shade: Color = ProjectSettings.get_setting("rendering/environment/defaults/default_clear_color", Color(0.075, 0.06, 0.045))
    var area := Rect2(world_bounds)
    for cell: Vector2i in margin: area = area.expand(UmmMapData.cell_to_world(cell))
    area = area.grow(UmmMapData.HEX_SIZE.x)
    var first := Vector2i(MAX_INT, MAX_INT)
    var last := Vector2i(-MAX_INT, -MAX_INT)
    for cell: Vector2i in margin:
        first = first.min(cell)
        last = last.max(cell)
    first -= Vector2i(3, 3)
    last += Vector2i(3, 3)
    var rings := Image.create_empty(last.x - first.x + 1, last.y - first.y + 1, false, Image.FORMAT_RF)
    for x in rings.get_width():
        for y in rings.get_height():
            rings.set_pixel(x, y, Color(_margin_ring(first + Vector2i(x, y)) / 4.0, 0, 0))
    var veil := Polygon2D.new()
    veil.name = "MarginVeil"
    veil.polygon = PackedVector2Array([area.position, Vector2(area.end.x, area.position.y), area.end, Vector2(area.position.x, area.end.y)])
    var material := ShaderMaterial.new()
    material.shader = VEIL_SHADER
    material.set_shader_parameter("rings", ImageTexture.create_from_image(rings))
    material.set_shader_parameter("first", first)
    material.set_shader_parameter("hex", UmmMapData.HEX_SIZE)
    material.set_shader_parameter("shade", shade)
    material.set_shader_parameter("fade", Vector4(MARGIN_FADE[1], MARGIN_FADE[2], MARGIN_FADE[3], MARGIN_FADE[4]))
    veil.material = material
    return veil

const MAX_INT := 1 << 30
