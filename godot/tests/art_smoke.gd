extends SceneTree
## The art registry: every id in art.json has a file, unknown ids get a
## placeholder, and screens survive art pointed at a missing file.
## Run: godot --headless --path godot --script res://tests/art_smoke.gd
var failures := 0

func check(ok: bool, message: String) -> void:
    if not ok:
        failures += 1
        push_error("FAIL: " + message)

func _initialize() -> void:
    create_timer(120).timeout.connect(func(): push_error("Art smoke timed out"); quit(1))
    _run.call_deferred()

func _key(keycode: Key) -> void:
    for pressed in [true, false]:
        var event := InputEventKey.new()
        event.keycode = keycode
        event.physical_keycode = keycode
        event.pressed = pressed
        root.push_input(event, true)
    await process_frame

func _click(control: Control) -> void:
    for pressed in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT
        event.pressed = pressed
        event.position = control.get_global_transform_with_canvas() * (control.size / 2)
        root.push_input(event, true)
    await process_frame

## True if any sprite, texture rect, button icon or atlas under `node` shows `texture`.
func _shows(node: Node, texture: Texture2D) -> bool:
    var shown: Texture2D = null
    if node is Sprite2D or node is TextureRect: shown = node.texture
    elif node is Button: shown = node.icon
    if shown is AtlasTexture: shown = shown.atlas
    if shown != null and shown == texture: return true
    for child in node.get_children():
        if _shows(child, texture): return true
    return false

func _run() -> void:
    # 1. Every id art.json lists resolves to its own file on disk.
    var registry: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://content_source/art.json"))
    var missing: Array[String] = []
    for id: String in registry.ids:
        var value: Variant = registry.ids[id]
        var file: String = value if value is String else str(value.file)
        var full := file if file.begins_with("res://") else "res://content/textures/" + file
        if not ResourceLoader.exists(full): missing.append("%s -> %s" % [id, file])
        check(Art.has_art(id) and Art.texture(id) != null, "Listed id resolves: " + id)
    check(missing.is_empty(), "Every art.json file exists; missing: " + ", ".join(missing))
    print("art.json: %d ids, %d missing" % [registry.ids.size(), missing.size()])

    # 2. Unknown ids and unlisted units still give a real, cached image.
    var unknown: Texture2D = Art.texture("object.no_such_landmark")
    check(unknown != null and unknown.get_width() > 0, "Unknown id returns a placeholder")
    check(Art.texture("object.no_such_landmark") == unknown, "Placeholders are cached")
    check(not Art.has_art("object.no_such_landmark") and Art.path("object.no_such_landmark") == "", "Unknown id reports no art")
    check(Art.texture("feature.no_such_terrain").get_image().get_pixel(64, 64).a > 0.0, "Placeholder is visibly drawn")
    check(Art.path("unit.armoured_warrior_anim").ends_with("units/armoured_warrior_anim.png"), "unit.<id> finds units/<id>.png without an art.json entry")
    check(Art.texture("unit.no_such_unit") == Art.placeholder("unit.no_such_unit"), "A unit without a file gets a placeholder")
    Art.override("hero.walk", "units/no_such_walk_sheet.png")
    check(Art.texture("hero.walk").get_width() == 256, "A missing animation sheet's placeholder has all four frames")
    Art.clear_overrides()

    # 3. The game with one portrait and one unit pointed at missing files.
    Art.override("hero.portrait", "portraits/no_such_commander.png")
    Art.override("portrait.ushari", "portraits/no_such_ushari.png")
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    var commander: Texture2D = Art.placeholder("hero.portrait")
    check(scene.sidebar.get_node("Portrait").texture == commander, "Sidebar shows the commander placeholder")
    await _key(KEY_H)
    check(scene.screens.depth() == 1 and _shows(scene.screens.top(), commander), "H opens the hero screen with the placeholder figure")
    await _key(KEY_ESCAPE)
    check(scene.screens.depth() == 0, "Esc closes the hero screen")
    scene.dialogue.say([{"speaker": "Ushari", "text": "Test line."}])
    check(scene.dialogue._portrait.texture == Art.placeholder("portrait.ushari"), "Dialogue shows Ushari's placeholder portrait")
    scene.dialogue.say([{"speaker": "Nobody In Particular", "text": "Test line."}])
    scene.dialogue.advance()
    check(scene.dialogue._portrait.texture == Art.texture("portrait.default"), "An unlisted speaker uses the default portrait")
    scene.dialogue.skip_all()
    var stack_id := str(scene.army[0].id) if not scene.army.is_empty() else "levy_spearman"
    Art.override("unit." + stack_id, "units/no_such_%s.png" % stack_id)
    check(scene._town_button.visible, "The commander starts in an owned town")
    await _click(scene._town_button)
    var town: Control = scene.screens.top()
    check(town != null and town.get_script() == preload("res://scripts/town_screen.gd"), "Enter town opens the town screen")
    if town != null:
        check(_shows(town, Art.placeholder("unit." + stack_id)), "Town screen shows the missing unit's placeholder")
        await _key(KEY_ESCAPE)
    check(scene.screens.depth() == 0, "Esc leaves the town")
    scene.queue_free()
    await process_frame

    # 4. A battle with a unit whose file is missing.
    Art.override("unit.mummy", "units/no_such_mummy.png")
    scene = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = "res://content/maps/default.json"
    root.add_child(scene)
    await process_frame
    var dungeon := Vector2i.ZERO
    for cell in scene.data.objects:
        if scene.data.objects[cell].type == "dungeon": dungeon = cell
    scene.fog.reveal([[dungeon.x, dungeon.y]])
    scene.army = [{"id": "desert_archer", "count": 10}, {"id": "mummy", "count": 10}]
    scene.state.battle_companions = []
    check(scene.travel_to(dungeon), "A route reaches the dungeon")
    while scene.hero.moving: await process_frame
    await _click(scene._enter_button)
    check(is_instance_valid(scene.battle), "Entering the dungeon starts a battle")
    if is_instance_valid(scene.battle):
        for i in 3: await process_frame
        check(_shows(scene.battle, Art.placeholder("unit.mummy")), "Battle shows the missing unit's placeholder")
        check(_shows(scene.battle, Art.texture("unit.desert_archer")), "Battle still shows real unit art")
    Art.clear_overrides()
    scene.queue_free()
    await process_frame
    print("Art registry smoke: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
    quit(1 if failures else 0)
