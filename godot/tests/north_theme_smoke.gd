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

## A grass-ground map with a snowy northern strip: every snow hex gets the snow
## drift (real art, not a placeholder), no undergrowth grows on it, and the faded
## land beyond the snowy border is snow too.
func _check_snow() -> void:
    var tiles: Array = []
    var snowy: Array[Vector2i] = []
    for q in range(-3, 4):
        for r in range(-3, 4):
            if absi(q + r) > 3: continue
            var north_edge := r - maxi(-3, -3 - q) <= 0   # the top hex of each column
            tiles.append({"q": q, "r": r, "terrain": "snow" if north_edge else "grass", "passable": true, "moveCost": 1.5 if north_edge else 1.0})
            if north_edge: snowy.append(Vector2i(q, r))
    var fixture := "user://snow_fixture.json"
    var file := FileAccess.open(fixture, FileAccess.WRITE)
    file.store_string(JSON.stringify({"version": 1, "name": "Snow fixture", "ground": "grass", "tiles": tiles, "objects": []}))
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
    for cell in snowy:
        var name := "snow_%d_%d" % [cell.x, cell.y]
        check(features.has_node(name), "Snow hex %s draws a snow drift" % cell)
        if features.has_node(name):
            var drift: Sprite2D = features.get_node(name)
            check(drift.texture == snow_art and drift.material is ShaderMaterial, "Snow drift uses feature.snow, feathered: %s" % cell)
            check(drift.scale.x * snow_art.get_width() > UmmMapData.HEX_SIZE.x, "Snow drift overlaps its neighbours: %s" % cell)
        check(not view.get_node("Scenery").has_node("Scrub_%d_%d" % [cell.x, cell.y]), "No undergrowth on snow: %s" % cell)
    var margin_snow := 0
    if view.has_node("MarginGround"):
        margin_snow = view.get_node("MarginGround").get_child_count()
        check(view.get_node("MarginGround").get_index() < features.get_index(), "Margin snow lies under the map's own features")
    var expected := 0
    for cell: Vector2i in view.margin:
        if view.margin[cell].terrain == "snow": expected += 1
    check(expected > 0 and margin_snow == expected, "Land beyond a snowy border is snow (%d of %d)" % [margin_snow, expected])
    if capture:
        var camera := Camera2D.new()
        view.add_child(camera)
        camera.position = UmmMapData.cell_to_world(Vector2i(0, -2))
        camera.zoom = Vector2.ONE * 1.5
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
