extends SceneTree
## Run with --headless --script res://tests/smoke.gd.
## Omit --headless and append -- --capture to also save rendered screenshots.

var failures := 0

func _initialize() -> void:
    create_timer(20.0).timeout.connect(func():
        push_error("Smoke test timed out; a script error may have interrupted execution.")
        quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func _run() -> void:
    var map := UmmMapData.new()
    check(map.read("res://content/maps/default.json"), "Existing C++ JSON map loads")
    check(map.tiles.size() == 169, "All 169 canonical tiles import")
    check(map.objects.size() == 11, "All 11 landmarks import")
    check(map.spawn == Vector2i(-5, 1), "Hero starts at Khemret")
    for cell: Vector2i in map.tiles:
        check(UmmMapData.world_to_cell(UmmMapData.cell_to_world(cell)) == cell, "Negative and positive axial picking round trip")
    check(map.path_between(map.spawn, Vector2i(999, 999)).is_empty(), "Off-map movement rejected")
    check(map.path_between(map.spawn, Vector2i(-7, 0)).is_empty(), "Impassable mountain rejected")
    var route := map.path_between(map.spawn, Vector2i(5, -1))
    check(not route.is_empty(), "Town-to-town route exists")
    var previous := map.spawn
    for cell in route:
        check(cell - previous in UmmMapData.DIRECTIONS, "Every route step is a hex neighbor")
        check(map.is_passable(cell), "Route avoids blocked tiles")
        previous = cell
    for cell: Vector2i in map.tiles:
        if map.tiles[cell].get("road", false):
            check(is_equal_approx(map.step_cost(cell), 0.5), "Road cost matches C++ loader without double discount")
    _check_weighted_routing()

    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    check(scene.hero.cell == map.spawn, "Playable scene initializes at map spawn")
    check(scene.hero.get_parent().y_sort_enabled, "Hero participates in Godot scenery occlusion")
    check(scene.map_view.get_node("TerrainFeatures").get_child_count() == 110, "108 terrain sprites plus one joined oasis and shore")
    check(scene.hero.sprite.sprite_frames.get_frame_count("walk") == 4, "Walk animation imports all frames")
    check(not scene.travel_to(Vector2i(-7, 0)), "Scene rejects blocked destination")
    if "--capture" in OS.get_cmdline_user_args():
        await _capture("start")
    # Drive an actual mouse event through Godot's input dispatch, including camera transforms.
    var target := Vector2i(-3, 2)
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    click.position = scene.get_canvas_transform() * UmmMapData.cell_to_world(target)
    root.push_input(click, true)
    await process_frame
    check(scene.hero.moving, "Mouse click starts hero movement")
    check(not scene.travel_to(Vector2i.ZERO), "A second order cannot interrupt an active step")
    var deadline := Time.get_ticks_msec() + 6000
    while scene.hero.moving and Time.get_ticks_msec() < deadline:
        await process_frame
    check(not scene.hero.moving and scene.hero.cell == target, "Hero completes the clicked route")
    check(scene.hero.sprite.animation == "idle", "Hero returns to idle after travelling")

    scene.sidebar.get_node("Grid").button_pressed = true
    check(scene.overlay.show_grid, "Grid control changes the world overlay")
    scene.sidebar.get_node("Center").pressed.emit()
    var on_screen: Vector2 = scene.get_canvas_transform() * scene.hero.position
    check(scene._over_map(on_screen), "Find hero centers inside the unobscured map area")
    scene.sidebar.get_node("Overview").pressed.emit()

    if "--capture" in OS.get_cmdline_user_args():
        scene.set_process(false)
        scene.sidebar.get_node("Grid").button_pressed = false
        scene._inspect(Vector2i(-3, 5))
        await _capture("overview")
        scene.camera.zoom = Vector2.ONE
        scene.center_hero()
        await _capture("detail")
    scene.queue_free()
    await process_frame
    print("Godot migration smoke: %s" % ("PASS" if failures == 0 else "%d failures" % failures))
    quit(0 if failures == 0 else 1)

func _check_weighted_routing() -> void:
    var map := UmmMapData.new()
    map.tiles = {
        Vector2i(0, 0): {"passable": true, "moveCost": 1.0},
        Vector2i(1, 0): {"passable": true, "moveCost": 5.0},
        Vector2i(2, 0): {"passable": true, "moveCost": 1.0},
        Vector2i(0, 1): {"passable": true, "moveCost": 1.0, "road": true},
        Vector2i(1, 1): {"passable": true, "moveCost": 0.5, "road": true},
        Vector2i(9, 9): {"passable": true, "moveCost": 1.0},
    }
    var path := map.path_between(Vector2i.ZERO, Vector2i(2, 0))
    check(path.size() == 3 and is_equal_approx(map.path_cost(path), 2.0), "Longer road route wins over costly direct terrain")
    check(map.path_between(Vector2i.ZERO, Vector2i(9, 9)).is_empty(), "Disconnected passable destination rejected")

func _capture(label: String) -> void:
    if DisplayServer.get_name() == "headless":
        check(false, "Screenshot capture requires a graphical display")
        return
    await process_frame
    await RenderingServer.frame_post_draw
    var folder := ProjectSettings.globalize_path("res://artifacts")
    DirAccess.make_dir_recursive_absolute(folder)
    var result := root.get_texture().get_image().save_png(folder.path_join(label + ".png"))
    check(result == OK, "Rendered screenshot saved")
