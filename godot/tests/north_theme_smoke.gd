extends SceneTree
## Checks the biome package, navigation, zoom and rendered grid together.
## --headless runs logic checks; with a display add -- --capture <directory>.
var failures := 0
var capture := false
var folder := "/tmp/umm-north-review"

func check(ok: bool, message: String) -> void:
    if not ok:
        failures += 1
        push_error("FAIL: " + message)

func _initialize() -> void:
    create_timer(60).timeout.connect(func(): push_error("North package test timed out"); quit(1))
    _run.call_deferred()

func shot(name: String) -> Image:
    for i in 4: await process_frame
    await RenderingServer.frame_post_draw
    var picture := root.get_texture().get_image()
    picture.save_png(folder.path_join(name + ".png"))
    return picture

func _run() -> void:
    var args := OS.get_cmdline_user_args()
    capture = "--capture" in args and DisplayServer.get_name() != "headless"
    if args.size() > 1: folder = args[1]
    if capture:
        DirAccess.make_dir_recursive_absolute(folder)
        DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    scene.fog.visible = false
    var map: Node2D = scene.map_view
    check(map.art_theme.config.id == "north", "Grass maps use the North package")
    for id in [map.art_theme.config.objects.quarry, map.art_theme.config.scenery.ridge, map.art_theme.config.scenery.scrub]:
        check(ResourceLoader.exists(Art.path(id)), "Package asset imports: " + id)
        var texture: Texture2D = Art.texture(id)
        check(texture.get_image().detect_alpha() != Image.ALPHA_NONE, "Scenery has genuine alpha: " + id)
    var quarry := Vector2i(-13, 10)
    check(map.object_texture(map.data.objects[quarry]) == "object.quarry.north", "North quarry cannot fall back to desert art")
    check(not map.get_node("GrassGrain").visible, "Legacy patterned grass is replaced with a continuous meadow")
    var joined := 0
    var seen := {}
    for group in map.mountain_groups:
        if group.size() > 1: joined += 1
        check(group.size() <= 3, "Ridges respect their footprint limit")
        for i in group.size():
            var cell: Vector2i = group[i]
            check(not seen.has(cell) and not map.data.is_passable(cell), "A ridge covers a unique blocked mountain cell")
            if i > 0: check(cell - group[i - 1] in UmmMapData.DIRECTIONS, "Ridge cells remain connected")
            seen[cell] = true
    for cell: Vector2i in seen:
        check(map.get_node("Scenery").has_node("MountainFootprint_%d_%d" % [cell.x, cell.y]), "Every blocked mountain hex has visible scenery")
    check(not map.data.path_between(map.data.spawn, quarry).is_empty(), "Vale Quarry is reachable from Varenhold")
    for cell: Vector2i in map.data.objects:
        check(cell == map.data.spawn or not map.data.path_between(map.data.spawn, cell).is_empty(), "Every northern landmark has an approach: " + str(map.data.objects[cell].name))
    check(joined > 10, "The northern range uses multi-cell scenery")
    check(map.get_node("Scenery").y_sort_enabled, "Scenery sorts with the hero")
    var baseline := UmmMapData.new()
    check(baseline.read(map.map_path), "Logical map loads independently of art")
    check(map.data.path_between(baseline.spawn, Vector2i(-7, 6)) == baseline.path_between(baseline.spawn, Vector2i(-7, 6)), "New art preserves navigation")
    check(scene.overlay.z_index > map.get_node("Scenery").z_index and scene.fog.z_index > scene.overlay.z_index, "Grid is above scenery but below fog")
    scene.fit_map()
    if capture: await shot("north_overview")
    var focus: Vector2 = scene._map_focus()
    scene._set_zoom(0.7, focus, false)
    var anchor: Vector2 = scene.get_canvas_transform().affine_inverse() * focus
    scene._zoom(1, focus)
    await create_timer(0.22).timeout
    var after: Vector2 = scene.get_canvas_transform().affine_inverse() * focus
    check(anchor.distance_to(after) < 1.0, "Zoom keeps its world anchor beneath the pointer")
    check(is_equal_approx(scene.camera.zoom.x, 0.84), "Wheel zoom uses small smooth increments")
    scene._set_zoom(10.0, focus, false)
    check(is_equal_approx(scene.camera.zoom.x, 2.5), "Zoom reaches and clamps at 250 percent")
    scene._set_zoom(0.01, focus, false)
    check(is_equal_approx(scene.camera.zoom.x, 0.10), "Zoom out reaches and clamps at 10 percent")
    scene._set_zoom(1.1, focus, false)
    scene.camera.position = UmmMapData.cell_to_world(quarry) - (focus - scene.get_viewport_rect().size * 0.5) / scene.camera.zoom
    scene.camera.force_update_scroll()
    var off: Image
    if capture: off = await shot("north_quarry")
    scene._grid_button.button_pressed = true
    check(scene.overlay.show_grid and scene.sidebar.get_node("Grid").button_pressed, "Toolbar grid toggle updates both controls")
    if capture:
        var on: Image = await shot("north_quarry_grid")
        var changed := 0
        for y in range(0, on.get_height(), 3):
            for x in range(0, on.get_width(), 3):
                if on.get_pixel(x, y) != off.get_pixel(x, y): changed += 1
        check(changed > 500, "Grid toggle changes visible rendered pixels")
    scene.sidebar.get_node("Grid").button_pressed = false
    check(not scene.overlay.show_grid and not scene._grid_button.button_pressed, "Sidebar grid toggle stays synchronized")
    scene.camera.position = UmmMapData.cell_to_world(Vector2i(7, -11))
    scene.camera.force_update_scroll()
    if capture: await shot("north_ridges")
    scene.queue_free()
    await process_frame
    await _check_snow()
    _check_dogs()
    var desert: Node2D = load("res://scripts/map_view.gd").new()
    desert.map_path = "res://content/maps/default.json"
    root.add_child(desert)
    check(desert.art_theme.config.id == "desert", "Desert remains a separate package")
    check(desert.object_texture({"type": "quarry"}) == "object.quarry", "Desert retains its own quarry")
    print("North package: %s" % ("PASS" if failures == 0 else "%d failures" % failures))
    quit(0 if failures == 0 else 1)

## A grass-ground map with a snowy northern strip (two hexes deep) and a wood at
## its eastern end: the snow is one continuous layer (no per-hex sprites), every
## snow hex is covered, deep snow is full cover, edge hexes are partial, it fades
## out one hex onto the meadow and is gone two hexes out, frost on the grass
## reaches two hexes and no further, a camp in the snow stands on trodden
## ground, no undergrowth grows on snow, the land beyond a snowy border is snow too, and the trees touching
## the snow carry it on their crowns.
func _check_snow() -> void:
    var tiles: Array = []
    var snowy: Array[Vector2i] = []
    var woods := Vector2i(6, -4)
    var peak := Vector2i(-6, 2)
    var camp := Vector2i(0, -6)   # a camp standing in the deep snow
    for q in range(-6, 7):
        for r in range(-6, 7):
            if absi(q + r) > 6: continue
            var depth := r - maxi(-6, -6 - q)   # 0 = the top hex of each column
            var terrain := "snow" if depth <= 1 else "grass"
            if Vector2i(q, r) == woods: terrain = "forest"
            if Vector2i(q, r) == peak: terrain = "mountain"
            tiles.append({"q": q, "r": r, "terrain": terrain, "passable": terrain in ["snow", "grass"], "moveCost": 1.5 if terrain != "grass" else 1.0})
            if terrain == "snow": snowy.append(Vector2i(q, r))
    var fixture := "user://snow_fixture.json"
    var file := FileAccess.open(fixture, FileAccess.WRITE)
    file.store_string(JSON.stringify({"version": 1, "name": "Snow fixture", "ground": "grass", "tiles": tiles,
        "objects": [{"q": camp.x, "r": camp.y, "type": "quest_giver", "name": "Snow Camp"}]}))
    file.close()
    check(Art.is_listed("feature.snow") and Art.has_art("feature.snow"), "feature.snow has a real image")
    var snow_art: Texture2D = Art.texture("feature.snow")
    var pixel := snow_art.get_image().get_pixel(64, 64)
    check(pixel.b >= pixel.r and pixel.get_luminance() > 0.5, "Snow art is pale and cool")
    var view: Node2D = load("res://scripts/map_view.gd").new()
    view.map_path = fixture
    root.add_child(view)
    await process_frame
    check(view.art_theme.config.id == "north", "Snow fixture uses the North package")
    var features: Node2D = view.get_node("TerrainFeatures")
    check(features.has_node("SnowField") and features.get_node("SnowField").get_index() == 0, "Snow is one layer under the other ground features")
    if features.has_node("SnowField"):
        var layer: Polygon2D = features.get_node("SnowField")
        check(layer.texture == snow_art and layer.material is ShaderMaterial, "The snow layer paints feature.snow through its shader")
    var deep := 0
    var edge := 0
    # A camp in the snow stands on trodden ground: thin snow and frost, not a drift.
    check(view.snow_at(UmmMapData.cell_to_world(camp)) < 0.5, "A camp in the snow stands on trodden ground (%.2f)" % view.snow_at(UmmMapData.cell_to_world(camp)))
    check(view.dust_at(UmmMapData.cell_to_world(camp)) > 0.9, "Trodden ground by a camp is frosted")
    for cell in snowy:
        if cell == camp: continue
        var here: float = view.snow_at(UmmMapData.cell_to_world(cell))
        check(here > 0.5, "Snow hex %s is covered (%.2f)" % [cell, here])
        var inland := true
        for direction: Vector2i in UmmMapData.DIRECTIONS:
            if view._terrain_at(cell + direction) == "grass": inland = false
        if inland:
            deep += 1
            if _hex_distance(cell, camp) > 1:   # beside the camp it is trodden thinner
                check(here > 0.9, "Deep snow at %s is full cover (%.2f)" % [cell, here])
        else:
            edge += 1
            check(here < 0.95, "Edge snow at %s is broken cover (%.2f)" % [cell, here])
        check(not view.get_node("Scenery").has_node("Scrub_%d_%d" % [cell.x, cell.y]), "No undergrowth on snow: %s" % cell)
    check(deep > 0 and edge > 0, "Fixture has deep and edge snow (%d, %d)" % [deep, edge])
    var fringe := 0
    for cell: Vector2i in view.data.tiles:
        if view.data.tiles[cell].terrain != "grass": continue
        var here: float = view.snow_at(UmmMapData.cell_to_world(cell))
        if view._touches_snow(cell):
            fringe += 1
            check(here < 0.5, "Meadow beside snow is not deep snow: %s (%.2f)" % [cell, here])
        else:
            var near_snowy_wood := false
            for direction: Vector2i in UmmMapData.DIRECTIONS:
                if view._snow_weight(cell + direction) > 0.0: near_snowy_wood = true
            check(here < (0.3 if near_snowy_wood else 0.05), "Meadow away from the snow is bare: %s (%.2f)" % [cell, here])
    check(fringe > 0, "Fixture has meadow beside the snow")
    # Frosted grass: thick beside the snow, sparse two hexes out, none three hexes or more away.
    var bands := {1: 0, 2: 0, 3: 0}
    for cell: Vector2i in view.data.tiles:
        if view.data.tiles[cell].terrain != "grass": continue
        var steps := 99
        for other in snowy: steps = mini(steps, _hex_distance(cell, other))
        var dust: float = view.dust_at(UmmMapData.cell_to_world(cell))
        if steps == 1:
            bands[1] += 1
            check(dust > 0.6, "Grass beside the snow is frosted: %s (%.2f)" % [cell, dust])
        elif steps == 2:
            bands[2] += 1
            check(dust > 0.0 and dust < 0.5, "Grass two hexes out is sparsely frosted: %s (%.2f)" % [cell, dust])
        elif steps >= 3:
            bands[3] += 1
            check(dust == 0.0, "Grass three or more hexes from the snow has no frost: %s (%.2f)" % [cell, dust])
    check(bands[1] > 0 and bands[2] > 0 and bands[3] > 0, "Fixture covers near, mid and far grass: %s" % bands)
    var ridge: Node = view.get_node("Scenery").get_node_or_null("Ridge_%d_%d" % [peak.x, peak.y])
    check(ridge != null and ridge.material is ShaderMaterial, "A ridge beside the snow wears a snow cap")
    var margin_snow := 0
    for cell: Vector2i in view.margin:
        if view.margin[cell].terrain != "snow": continue
        margin_snow += 1
        check(view.snow_at(UmmMapData.cell_to_world(cell)) > 0.5, "Land beyond a snowy border is snow: %s" % cell)
    check(margin_snow > 0, "Fixture has a snowy border")
    var dusted := 0
    for tree in view.get_node("Scenery").get_children():
        if not tree.name.begins_with("Tree_%d_%d_" % [woods.x, woods.y]): continue
        dusted += 1
        check(tree.material is ShaderMaterial, "Trees touching snow carry snow: " + tree.name)
    check(dusted > 0, "The wood beside the snow has trees")
    if capture:
        var camera := Camera2D.new()
        view.add_child(camera)
        camera.position = UmmMapData.cell_to_world(Vector2i(0, -2))
        camera.make_current()
        await shot("north_snow")
        camera.queue_free()
    view.queue_free()
    await process_frame

## 1.0's Foaming Dogs: the wolf sprite in their own sickly tint, not a placeholder.
func _check_dogs() -> void:
    check(Art.is_listed("unit.feral_dog") and Art.has_art("unit.feral_dog"), "unit.feral_dog is listed with real art")
    var dog: Image = Art.texture("unit.feral_dog").get_image()
    var hound: Image = Art.texture("unit.rabid_hound").get_image()
    var differs := false
    for y in range(0, dog.get_height(), 4):
        for x in range(0, dog.get_width(), 4):
            if dog.get_pixel(x, y).a > 0.0 and not dog.get_pixel(x, y).is_equal_approx(hound.get_pixel(x, y)): differs = true
    check(differs, "Foaming Dogs are tinted apart from rabid hounds")

static func _hex_distance(a: Vector2i, b: Vector2i) -> int:
    var d := a - b
    return (absi(d.x) + absi(d.y) + absi(d.x + d.y)) / 2
