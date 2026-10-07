extends SceneTree
var failures := 0
func _initialize() -> void:
    create_timer(30).timeout.connect(func(): quit(1))
    _run.call_deferred()
func check(ok: bool, message: String) -> void:
    if not ok:
        failures += 1
        push_error(message)
func _run() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    scene.camera.position = Vector2.ZERO
    for key in [KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN]:
        var event := InputEventKey.new()
        event.physical_keycode = key
        event.pressed = true
        Input.parse_input_event(event)
        Input.flush_buffered_events()
        var before: Vector2 = scene.camera.position
        scene._process(0.1)
        check(scene.camera.position != before, "Arrow key pans the map")
        event = event.duplicate()
        event.pressed = false
        Input.parse_input_event(event)
        Input.flush_buffered_events()
    var camp := Vector2i(-8, -3)
    var badge: Label = scene.map_view.anchors[camp].get_node("GuardCount")
    check(badge.text == "12 guards", "Camp badge shows the configured creature total")
    check(scene.data.tiles.size() == 1261, "Expanded map loads with the village on the larger map")
    var stones := 0
    for object in scene.data.objects.values():
        if str(object.name).ends_with("Rune Stone"):
            stones += 1
            check(scene.map_view.object_texture(object) == "object.obelisk", "Rune stones use stone artwork")
    check(stones == 3, "Three historical inscriptions survive across the map")
    scene.queue_free()
    await process_frame
    var puzzle: Control = load("res://scripts/gate_puzzle.gd").new()
    var source: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://content/maps/old_passage.triggers.json"))
    for trigger in source.triggers:
        for action in trigger["do"]:
            if action.has("lore") and str(action.lore[0]).begins_with("Gate rune"):
                puzzle.readings.append({"title": action.lore[0], "text": action.lore[1]})
    check(puzzle.readings.size() == 4, "Three discoveries and Ushari's family knowledge supply the names")
    root.add_child(puzzle)
    await process_frame
    puzzle.ask_ushari()
    check(puzzle.feedback.text.begins_with("Ushari:") and puzzle.feedback.text.contains("Ishara"), "Ushari recalls her family knowledge in her own words")
    if "--capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
        await process_frame
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("/tmp/umm_gate_puzzle.png")
    var results: Array = []
    puzzle.finished.connect(func(result): results.append(result))
    check(not puzzle.try_lock() and results.is_empty(), "A wrong combination does not complete the quest")
    for i in 4:
        while puzzle.wheels[i] != i: puzzle.buttons[i].pressed.emit()
    puzzle.choice = {"route_ready": true, "options": [{"requires_lore": ["Gate rune · Esh Varen", "Gate rune · Tal Eren", "Gate rune · Orun"], "requires_route": true}]}
    var missing: Dictionary = puzzle.readings.pop_back()
    check(not puzzle.try_lock(), "Knowing the answer cannot skip an undiscovered inscription")
    puzzle.readings.append(missing)
    puzzle.choice.route_ready = false
    check(not puzzle.try_lock(), "Departure requires a secured approach")
    puzzle.choice.route_ready = true
    check(puzzle.try_lock(), "Turning the actual controls with all prerequisites completes the survey")
    check(not puzzle.try_lock() and results.size() == 1, "The gate commits only once")
    puzzle.queue_free()
    await process_frame
    print("Expanded demo: ", "PASS" if failures == 0 else "FAIL")
    quit(failures)
