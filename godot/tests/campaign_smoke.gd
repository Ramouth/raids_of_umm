extends SceneTree
## Run with --headless --script res://tests/campaign_smoke.gd.
## Plays a two-map fixture campaign through the real game root the way a
## player would: menu, intro, a story choice (one option locked), walking to
## the map's ending, the ending text, the title card, the next map with the
## company carried, a click into the faded margin, F5, and Continue.

const Fixture = preload("res://tests/campaign_fixture.gd")
const DIR := "user://campaign_smoke"
var failures := 0

func _initialize() -> void:
    create_timer(90.0).timeout.connect(func():
        push_error("Campaign smoke timed out")
        quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func _run() -> void:
    var campaign_file := Fixture.write_two_maps(DIR)
    Fixture.clear_saves(DIR)
    var game: Node = load("res://scenes/game.tscn").instantiate()
    game.campaign_path = campaign_file
    game.save_dir = DIR
    game.skip_intro = true
    root.add_child(game)
    await process_frame
    var continue_button := _button(game._menu_layer, "Continue")
    check(continue_button != null and continue_button.disabled, "Continue is unavailable without a campaign save")
    _button(game._menu_layer, "New expedition").pressed.emit()
    await process_frame
    _key(KEY_ESCAPE)   # skip the new-expedition cutscene
    await _until(func(): return game.adventure != null)
    var scene: Node2D = game.adventure
    check(scene != null and scene.campaign_map.get("id", "") == "a", "New expedition starts the campaign's first map")
    if scene == null: return _finish()
    check(scene.data.title == "The Vale of Oaths", "The first map's own file is drawn (no old_passage default)")
    check(int(scene.state.get("level_cap", 0)) == 2, "The first map's level cap is in force")
    check("cap 2" in scene._xp_title.text, "The HUD shows the level cap beside the commander's level")

    # The start-of-map choice: one option is locked behind a flag we do not have.
    await _until(func(): return game.screens.top() != null and game.screens.top().get("choice") is Dictionary and not game.screens.top().choice.is_empty())
    var choice: Control = game.screens.top()
    var burn := _button(choice, "Burn the village")
    check(burn != null and burn.disabled, "A locked option is shown, disabled, with its text")
    choice.select("burn")
    check(game.screens.top() == choice, "A locked option cannot be taken")
    _button(choice, "Spare the village").pressed.emit()
    await process_frame
    check(str(scene.state.get("flags", {}).get("choice.oath", "")) == "spare", "The choice is recorded as a story flag")
    # The level-up pop-up asks for a path; the player opens the path screen and leaves it.
    await _until(func(): return is_instance_valid(scene.popup))
    if is_instance_valid(scene.popup): scene.popup.choose(0)
    await _until(func(): return game.screens.depth() > 0)
    _key(KEY_ESCAPE)
    await _until(func(): return game.screens.depth() == 0)
    var progress: Dictionary = scene.state.hero_progress
    check(int(progress.level) == 2 and int(scene.state.get("xp_lost_to_cap", 0)) > 0, "XP past the map's cap is lost and counted")
    var army_a: Array = scene.army.duplicate(true)
    var xp_a := int(progress.xp)

    # A click on the faded land beyond the edge does nothing.
    check(not scene.map_view.margin.is_empty(), "The map is ringed by a faded margin")
    var outside := _margin_cell(scene)
    var before_cell: Vector2i = scene.hero.cell
    _click(scene, outside)
    await process_frame
    check(scene._planned == scene.NO_CELL and scene.hero.cell == before_cell and not scene.hero.moving, "A click in the margin plans no route")
    scene._inspect(outside)
    check(scene.sidebar.get_node("Route").text == "Out of reach.", "Hovering the margin says it is out of reach")
    check(JSON.parse_string(scene.adventure.preview(outside.x, outside.y)).path.is_empty(), "No route preview enters the margin")
    var veil: Node = scene.map_view.get_node_or_null("MarginVeil")
    check(veil != null and veil.get_index() < scene.map_view.get_node("Scenery").get_index(), "The veil lies under the map's own scenery")

    # Walk to the Old Stone: the ending fires.
    await _walk(scene, Vector2i(3, -1))
    check(scene.state.get("won", false) and str(scene.state.get("next_map", "")) == "b", "Reaching the stone ends the map, naming the next one")
    await _until(func(): return game.screens.top() != null and game.screens.top().get("outcome") is Dictionary and not game.screens.top().outcome.is_empty())
    var frames: Control = game.screens.top()
    if frames != null and frames.has_method("advance"):
        _key(KEY_SPACE)
        await process_frame
    await _until(func(): return game.screens.top() != null and game.screens.top().has_node("VBoxContainer"))
    var ending: Control = game.screens.top()
    var onward := _button(ending, "Onward: The Ribbon Road")
    check(onward != null, "The ending offers the next map by name")
    check(_has_text(ending, "The Stone Remembers") and _has_text(ending, "THE VALE OF OATHS  ·  MAP COMPLETE"), "The map's own ending text is shown")
    check(game.can_continue(), "The campaign is saved at the next map as soon as it moves on")
    if onward == null: return _finish()
    onward.pressed.emit()
    await process_frame
    var card: Control = game.title_card()
    check(card != null, "A title card stands between the maps")
    if card == null: return _finish()
    check(_has_text(card, "A NEW CHAPTER  ·  THE ROAD EAST"), "The card names the new chapter")
    check(_has_text(card, "The Ribbon Road"), "The card names the next map")
    check(_has_text(card, "[prophecy fragment]"), "The card carries the marked prophecy placeholder")
    _key(KEY_SPACE)
    await _until(func(): return game.adventure != null and game.adventure.campaign_map.get("id", "") == "b")
    scene = game.adventure
    check(scene != null and scene.data.title == "The Ribbon Road", "The next map starts after the card")
    if scene == null: return _finish()

    # The company rode in.
    check(_count(scene.army, "woad_runner") == _count(army_a, "woad_runner") and _count(army_a, "woad_runner") == 29, "The army is carried (with the troops the choice gave)")
    check(int(scene.state.hero_progress.level) == 2 and int(scene.state.hero_progress.xp) == xp_a, "Commander level and XP are carried")
    check("bearhide_cloak" in scene.state.get("items", []) and scene.loot().any(func(i): return i.id == "bearhide_cloak"), "Items are carried")
    check(scene.state.get("specials", []).any(func(sc): return sc.id == "ushari"), "Companions are carried")
    check(int(scene.state.get("level_cap", 0)) == 4 and "cap 4" in scene._xp_title.text, "The new map's level cap replaces the old one")
    check(scene.dialogue._text.text == "You spared them. The vale will remember it.", "A line gated on the first map's choice plays on the second")
    var leaked := false
    for event in scene.state.get("story", []): leaked = leaked or "FLAG LEAK" in str(event.get("text", ""))
    for line in scene.dialogue._queue: leaked = leaked or "FLAG LEAK" in str(line.get("text", ""))
    check(not leaked, "A line gated on the absence of a carried flag stays silent")
    scene.dialogue.skip_all()

    # Walk a little, save with F5, and come back through Continue.
    await _walk(scene, Vector2i(-1, -1))
    var saved_cell: Vector2i = scene.hero.cell
    check(saved_cell == Vector2i(-1, -1), "The hero walks the second map")
    _key(KEY_F5)
    await process_frame
    check(FileAccess.file_exists(DIR.path_join("campaign.save.json")) and FileAccess.file_exists(DIR.path_join("campaign_map.save.json")), "F5 writes the campaign save beside the map save")
    _key(KEY_ESCAPE)   # the game menu
    await process_frame
    var load_button := _button(game.screens.top(), "Load game")
    check(load_button != null and not load_button.disabled, "The game menu offers the saved campaign")
    if load_button == null: return _finish()
    load_button.pressed.emit()
    await _until(func(): return game.adventure != null and game.adventure != scene and game.adventure.is_inside_tree())
    scene = game.adventure
    check(scene.campaign_map.get("id", "") == "b", "Continue restores the campaign position")
    check(scene.hero.cell == saved_cell, "Continue restores the hero's place on the map")
    check(_count(scene.army, "woad_runner") == 29 and int(scene.state.hero_progress.level) == 2, "Continue restores the carried company")
    scene.dialogue.skip_all()

    # The last map's ending closes the campaign with the end screen.
    await _walk(scene, Vector2i(4, -2))
    await _until(func(): return _button(game.screens.top(), "New expedition") != null)
    var last: Control = game.screens.top()
    check(_has_text(last, "THE CAMPAIGN IS COMPLETE") and _has_text(last, "The Far Stone"), "The final ending shows the end screen")
    check(not game.can_continue(), "A finished campaign cannot be continued")
    _finish()

func _finish() -> void:
    print("Campaign smoke: ", "PASS" if failures == 0 else "%d failures" % failures)
    quit(failures)

## Plans and walks to `cell` with two clicks, scouting toward it through
## explored land and ending days (E) as needed.
func _walk(scene: Node2D, cell: Vector2i) -> void:
    for attempt in 6:
        await _until(func(): return not scene.turn_busy and not is_instance_valid(scene.popup))
        var goal := cell if scene.fog.is_explored(cell) else _nearest_explored(scene, cell)
        _focus(scene, goal)
        _click(scene, goal)
        await process_frame
        _click(scene, goal)
        await process_frame
        await _until(func(): return not scene.hero.moving, 8000)
        if scene.hero.cell == cell: return
        if scene.hero.cell != goal:
            _key(KEY_E)
            await _until(func(): return not scene.turn_busy)
    check(false, "Could not walk to %s (stopped at %s: %s)" % [cell, scene.hero.cell, scene.notice.text])

func _nearest_explored(scene: Node2D, cell: Vector2i) -> Vector2i:
    var best: Vector2i = scene.hero.cell
    for candidate: Vector2i in scene.data.tiles:
        if scene.fog.is_explored(candidate) and scene.data.is_passable(candidate) \
                and UmmMapData.cell_to_world(candidate).distance_to(UmmMapData.cell_to_world(cell)) < UmmMapData.cell_to_world(best).distance_to(UmmMapData.cell_to_world(cell)):
            best = candidate
    return best

func _focus(scene: Node2D, cell: Vector2i) -> void:
    scene._stop_zoom()
    var offset: Vector2 = (scene._map_focus() - scene.get_viewport_rect().size * 0.5) / scene.camera.zoom
    scene.camera.position = UmmMapData.cell_to_world(cell) - offset
    scene.camera.force_update_scroll()

func _click(scene: Node2D, cell: Vector2i) -> void:
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    click.position = scene.get_canvas_transform() * UmmMapData.cell_to_world(cell)
    root.push_input(click, true)

func _key(code: Key) -> void:
    var press := InputEventKey.new()
    press.keycode = code
    press.physical_keycode = code
    press.pressed = true
    root.push_input(press)

## A ring-1 margin cell that shows on screen next to the hero's side of the map.
func _margin_cell(scene: Node2D) -> Vector2i:
    var best := Vector2i.ZERO
    var best_distance := INF
    for cell: Vector2i in scene.map_view.margin:
        if int(scene.map_view.margin[cell].ring) != 1: continue
        var distance := UmmMapData.cell_to_world(cell).distance_to(scene.hero.position)
        if distance < best_distance:
            best_distance = distance
            best = cell
    _focus(scene, best)
    return best

func _until(condition: Callable, timeout_ms := 6000) -> void:
    var deadline := Time.get_ticks_msec() + timeout_ms
    while not condition.call() and Time.get_ticks_msec() < deadline:
        await process_frame

func _button(node: Node, text: String) -> Button:
    if node == null: return null
    for button in node.find_children("*", "Button", true, false):
        if button.text == text or button.text.begins_with(text + "\n"): return button
    return null

func _has_text(node: Node, text: String) -> bool:
    for label in node.find_children("*", "Label", true, false):
        if text in label.text: return true
    return false

func _count(stacks: Array, id: String) -> int:
    var total := 0
    for stack in stacks:
        if str(stack.id) == id: total += int(stack.count)
    return total
