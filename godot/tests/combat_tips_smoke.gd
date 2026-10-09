extends SceneTree
## Run with --headless --script res://tests/combat_tips_smoke.gd [-- --capture <png path>].
## Companion tips (data/combat_tips.json) through the real game root. The
## engine decides which tip is due (CombatSession, fed the adventure's
## tip_context); the combat screen only shows state.tip. On a map a tip lists,
## with Drest riding, his battle_start line shows in the first battle, beside
## the board, without stopping auto-battle; his victory line follows the first
## win; the adventure records both (tip_context.shown) and Save + Continue keep
## them, so a later battle does not repeat it. A map the tips do not list, or a
## company without Drest, hears nothing.

const Fixture = preload("res://tests/campaign_fixture.gd")
const DIR := "user://combat_tips_smoke"
var failures := 0
var capture_path := ""

func _initialize() -> void:
    create_timer(150.0).timeout.connect(func():
        push_error("Combat tips smoke timed out")
        quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func _run() -> void:
    var args := OS.get_cmdline_user_args()
    var at := args.find("--capture")
    if at >= 0 and at + 1 < args.size() and DisplayServer.get_name() != "headless":
        capture_path = args[at + 1]
        DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    var weak := [{"id": "brigand", "count": 3}]
    var strong := [{"id": "brigand", "count": 40}]

    # 1. Map 1.0 with Drest: his first-battle line, once; his victory line after the win.
    var game := await _start(Fixture.write_guard_map(DIR + "/listed", "1.0", ["drest", "aldren"],
        {"Copse Dogs": weak, "Field Brigands": strong}, []), DIR + "/listed")
    var scene: Node2D = game.adventure
    if scene == null: return _finish()
    var battle := await _fight(scene, game, Vector2i(2, 0))
    check(battle != null, "Walking into the first guard starts a battle")
    if battle == null: return _finish()
    await _until(func(): return not battle.busy)
    check(not battle.has_method("_check_tips") and not battle.has_method("_tip_conditions"), "The combat screen keeps no tip rules of its own")
    check(scene.state.get("tip_context", {}).get("map", "") == "1.0", "The adventure hands the engine this map's tip context")
    check(battle.tip_panel.visible and battle.tip_shown == "drest_battle_start", "Drest's battle_start tip shows in the first battle")
    check("drest_battle_start" in battle.state.get("battle", {}).get("tips", []), "The engine counts the tip as shown in this battle (state.battle.tips)")
    check(battle.tip_speaker.text == "DREST" and battle.tip_text.text.contains("Keep me next to whoever takes the first blow"), "The panel names Drest and gives his line")
    check(battle.tip_portrait.texture == Art.texture("portrait.drest") and Art.has_art("portrait.drest"), "The panel shows Drest's own portrait")
    var panel_rect := Rect2(battle.tip_panel.position, battle.tip_panel.size)
    check(not panel_rect.intersects(Rect2(battle.board.position, battle.board.size)), "The tip sits beside the board, never over a stack")
    check(battle.history.any(func(line): return line.begins_with("Drest: ")), "The line stays readable in the combat log")
    if not capture_path.is_empty():
        await RenderingServer.frame_post_draw
        DirAccess.make_dir_recursive_absolute(capture_path.get_base_dir())
        root.get_texture().get_image().save_png(capture_path)
    _press(battle.tip_panel)
    await process_frame
    check(not battle.tip_panel.visible, "A click on the tip closes it")
    # Auto-battle (TAB's checkbox) runs on with tips coming and going.
    battle.animation_speed = 0.01
    battle.auto_button.button_pressed = true
    await _until(func(): return battle.state.get("result", "ongoing") != "ongoing" and not battle.busy, 40000)
    check(battle.state.result == "victory", "Auto-battle runs to victory with tips on screen")
    check(battle.state.get("tip") is Dictionary and str(battle.state.tip.get("id", "")) == "drest_lone_wolf", "The final reply's state.tip is Drest's victory_first tip")
    check(battle.tip_shown == "drest_lone_wolf" and battle.tip_panel.visible, "Drest's victory_first tip shows with the result")
    var here: Array = battle.state.get("battle", {}).get("tips", [])
    check(here.count("drest_battle_start") == 1 and here.has("drest_lone_wolf"), "This battle's tips are listed once each (%s)" % [here])
    battle.return_button.pressed.emit()
    await _close_reports(game)
    var shown: Array = scene.state.get("tip_context", {}).get("shown", [])
    check(shown.count("drest_battle_start") == 1 and shown.has("drest_lone_wolf"), "resolve_encounter records the battle's tips for the campaign (%s)" % [shown])

    # Save, then Continue: what Drest has said stays said.
    check(scene.save_game(), "The game saves after the battle")
    var save: Variant = JSON.parse_string(FileAccess.get_file_as_string(DIR + "/listed/campaign_map.save.json"))
    check(save is Dictionary and not save.get("extra", {}).has("shown_tips"), "The client keeps no tip list of its own in the save")
    var said := shown.duplicate()
    said.sort()
    check(game.continue_campaign(), "Continue reloads the campaign")
    await _until(func(): return game.adventure != null and game.adventure != scene)
    scene = game.adventure
    var restored: Array = scene.state.get("tip_context", {}).get("shown", []).duplicate()
    restored.sort()
    check(restored == said, "Continue restores the shown tips from the engine's save (%s)" % [restored])
    battle = await _fight(scene, game, Vector2i(2, -2))
    check(battle != null, "The second guard starts a battle")
    if battle != null:
        await _until(func(): return not battle.busy)
        check(battle.tip_shown != "drest_battle_start", "The battle_start tip does not repeat in a later battle")
        check(not "drest_battle_start" in battle.state.get("battle", {}).get("tips", []), "...the engine did not make it due again")
        await _retreat(battle, game)

    # 2. A map the tips do not list: Drest says nothing.
    game.queue_free()
    await process_frame
    game = await _start(Fixture.write_guard_map(DIR + "/unlisted", "9.9", ["drest"], {"Copse Dogs": weak}, []), DIR + "/unlisted")
    await _silent(game, "a map no tip lists")
    # 3. A listed map without Drest in the company: nothing either.
    game.queue_free()
    await process_frame
    game = await _start(Fixture.write_guard_map(DIR + "/alone", "1.0", [], {"Copse Dogs": weak}, []), DIR + "/alone")
    await _silent(game, "a company without Drest")
    _finish()

func _start(campaign_file: String, dir: String) -> Node:
    Fixture.clear_saves(dir)
    var game: Node = load("res://scenes/game.tscn").instantiate()
    game.campaign_path = campaign_file
    game.save_dir = dir
    game.skip_intro = true
    root.add_child(game)
    await process_frame
    check(game.new_campaign(), "The fixture campaign starts")
    if is_instance_valid(game._menu_layer): game._menu_layer.queue_free()
    await _until(func(): return game.adventure != null and not game.adventure.turn_busy)
    return game

## The first battle in `game` shows no tip on its opening turns.
func _silent(game: Node, why: String) -> void:
    var scene: Node2D = game.adventure
    var battle := await _fight(scene, game, Vector2i(2, 0))
    check(battle != null, "A battle starts on %s" % why)
    if battle == null: return
    await _until(func(): return not battle.busy and battle.state.player_turn)
    check(not battle.tip_panel.visible and battle.tip_shown.is_empty() and battle.state.get("battle", {}).get("tips", []).is_empty(), "No tip shows on %s" % why)
    check(battle.state.get("tip") == null, "The engine's state.tip stays null on %s" % why)
    await _retreat(battle, game)

## Walks into the guard at `cell` and returns the battle screen (null if none).
func _fight(scene: Node2D, game: Node, cell: Vector2i) -> Control:
    await _clear_popups(scene, game)
    scene.fog.reveal([[cell.x, cell.y]])
    _focus(scene, cell)
    _click(scene, cell)
    await process_frame
    _click(scene, cell)
    await process_frame
    await process_frame
    if not scene.hero.moving and scene.state.get("encounter") == null:
        _key(KEY_ENTER)   # the plan stands: set off
        await process_frame
    await _until(func(): return is_instance_valid(scene.battle), 8000)
    return scene.battle if is_instance_valid(scene.battle) else null

func _retreat(battle: Control, game: Node) -> void:
    battle.animation_speed = 0.01
    await _until(func(): return not battle.busy)
    battle.retreat.pressed.emit()
    battle.confirm_retreat.confirmed.emit()
    battle.confirm_retreat.hide()
    await _until(func(): return battle.state.get("result", "ongoing") != "ongoing" and not battle.busy, 20000)
    battle.return_button.pressed.emit()
    await _close_reports(game)

## Closes the result and spoils screens, as Continue would.
func _close_reports(game: Node) -> void:
    for i in 6:
        await process_frame
        var top: Control = game.screens.top()
        if top != null and top.get_script() == load("res://scripts/battle_report.gd"):
            top.continue_button.pressed.emit()

## A level-up after a win asks for a path: take the first and leave the screen.
func _clear_popups(scene: Node2D, game: Node) -> void:
    await _until(func(): return not scene.turn_busy and not scene.hero.moving)
    for i in 4:
        if is_instance_valid(scene.popup): scene.popup.choose(0)
        await process_frame
        if game.screens.depth() > 0: _key(KEY_ESCAPE)
        await process_frame

func _finish() -> void:
    print("Combat tips: ", "PASS" if failures == 0 else "FAIL")
    quit(failures)

func _press(control: Control) -> void:
    var event := InputEventMouseButton.new()
    event.pressed = true
    event.button_index = MOUSE_BUTTON_LEFT
    event.position = control.get_global_transform_with_canvas() * (control.size / 2)
    root.push_input(event, true)
    event = event.duplicate()
    event.pressed = false
    root.push_input(event, true)

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

func _until(condition: Callable, timeout_ms := 6000) -> void:
    var deadline := Time.get_ticks_msec() + timeout_ms
    while not condition.call() and Time.get_ticks_msec() < deadline:
        await process_frame
