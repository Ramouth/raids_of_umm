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
## 4. Banter and a reckless companion (fixture data: Aldren "reckless", a
## "rushes" exchange Drest -> Aldren): the exchange plays as speech bubbles over
## each speaker's stack, in order, and in the log; Aldren is placed by clicks in
## deployment, then charges on his own turns inside the replies to the player's
## Defend presses (animated, announced), and clicks never order him.

const Fixture = preload("res://tests/campaign_fixture.gd")
const CombatView = preload("res://scripts/combat.gd")
const Board = preload("res://scripts/combat_board.gd")
const DIR := "user://combat_tips_smoke"
var failures := 0
var capture_path := ""

func _initialize() -> void:
    create_timer(240.0).timeout.connect(func():
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
    game.queue_free()
    await process_frame
    await _banter_and_reckless()
    _finish()

const RUSH_LINE := "Stay back behind the lines!"
const RUSH_REPLY := "Then keep up, old man."

## A copy of the staged rules data with Aldren reckless and one exchange:
## when he rushes ahead, Drest shouts and Aldren answers (Kharim, not in the
## battle, is dropped by the engine).
func _reckless_data() -> String:
    var dir := DIR + "/reckless_data"
    DirAccess.make_dir_recursive_absolute(dir)
    for file in DirAccess.get_files_at("res://content/data"):
        if file.ends_with(".json"):
            var copy := FileAccess.open(dir + "/" + file, FileAccess.WRITE)
            copy.store_string(FileAccess.get_file_as_string("res://content/data/" + file))
            copy.close()
    var units: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(dir + "/units.json"))
    for unit in units.units:
        if unit.id == "aldren" and not "reckless" in unit.get("abilities", []):
            if not unit.has("abilities"): unit["abilities"] = []
            unit.abilities.append("reckless")
    var out := FileAccess.open(dir + "/units.json", FileAccess.WRITE)
    out.store_string(JSON.stringify(units))
    out.close()
    var tips := {"tips": [{"id": "b_rush", "when": "rushes", "actor": "aldren", "repeat": "battle", "speaker": "Drest",
        "text": RUSH_LINE, "maps": ["1.0"],
        "reply": [{"speaker": "Aldren", "text": RUSH_REPLY}, {"speaker": "Kharim", "text": "Absent."}]}]}
    out = FileAccess.open(dir + "/combat_tips.json", FileAccess.WRITE)
    out.store_string(JSON.stringify(tips))
    out.close()
    return dir

func _banter_and_reckless() -> void:
    var battle: Control = CombatView.new()
    battle.data_dir = _reckless_data()
    root.add_child(battle)
    await process_frame
    battle.animation_speed = 0.25
    var army := [{"id": "woad_runner", "count": 20}, {"id": "drest", "level": 1, "companion": true}, {"id": "aldren", "level": 1, "companion": true}]
    var encounter := {"guards": [{"id": "levy_spearman", "count": 6}], "seed": 4, "tips": {"map": "1.0", "shown": []}}
    check(battle.begin(army, encounter, "Reckless test"), "The reckless fixture battle starts")
    await _until(func(): return not battle.busy)
    var aldren := {}
    var drest := {}
    for unit in battle.state.units:
        if unit.id == "aldren": aldren = unit
        if unit.id == "drest": drest = unit
    check(aldren.get("reckless", false) and not drest.get("reckless", true), "The bridge marks Aldren reckless, Drest not")
    if aldren.is_empty() or drest.is_empty():
        battle.queue_free()
        return
    var key: String = aldren.key
    # Hovering him says he charges on his own; so does his turn-order tooltip.
    _hover_board(battle, aldren.cell)   # read at once: a real pointer leaving the window may clear the hover next frame
    check("Charges on his own" in battle.inspection.get_parsed_text(), "Hovering Aldren's stack says he charges on his own: " + battle.inspection.get_parsed_text())
    var portrait: Control = battle._portrait(key, false)
    check("Charges on his own" in portrait.tooltip_text, "His turn-order portrait says it too")
    portrait.free()
    # Deployment: the player still places him, by clicks.
    check(battle._deploying(), "The battle opens with deployment")
    var spot: Array = []
    for cell in battle.state.get("deploy_zone", []):
        if battle._unit_at(cell).is_empty() and battle._obstacle_at(cell).is_empty() and cell != aldren.cell:
            spot = cell
            break
    check(not spot.is_empty(), "A free deployment hex")
    _click_board(battle, aldren.cell)
    await process_frame
    _click_board(battle, spot)
    await _until(func(): return not battle.busy)
    check(battle.units[key].cell == spot, "Clicking Aldren, then a green hex, places him like any stack")
    _key(KEY_ENTER)   # Finish placement
    await process_frame
    # The player only ever presses Defend; Aldren's turns play themselves.
    var seen: Array = []          # bubbles, in the order they appeared: [speaker, text, on Drest/Aldren's own stack]
    var aldren_cells := {}        # Aldren's on-screen positions while replies animate
    var defends := 0
    var ordered_aldren := false
    var captured := false
    var deadline := Time.get_ticks_msec() + 90000
    while Time.get_ticks_msec() < deadline and battle.state.get("result", "ongoing") == "ongoing":
        if is_instance_valid(battle.speech_bubble):
            var bubble: Control = battle.speech_bubble
            var said := [str(bubble.get_node("Speaker").text), str(bubble.get_node("Line").text)]
            if seen.is_empty() or [seen[-1][0], seen[-1][1]] != said:
                var speaker_key := ""
                for unit in battle.state.units:
                    if unit.name.to_upper() == said[0]: speaker_key = unit.key
                seen.append([said[0], said[1], battle.board.actors.get(speaker_key) == bubble.get_parent(), battle.busy])
            if not captured and not capture_path.is_empty() and said[0] == "ALDREN":
                captured = true
                await RenderingServer.frame_post_draw
                root.get_texture().get_image().save_png(capture_path.get_base_dir() + "/combat_banter.png")
        if battle.busy and battle.board.actors.has(key):
            aldren_cells[battle.board.actors[key].position.round()] = true
        var heard: bool = seen.any(func(entry): return entry[1] == RUSH_REPLY)
        var charged: bool = battle.history.any(func(line): return "charges on his own — he takes no orders" in line)
        if heard and charged and not battle.busy and battle.state.player_turn: break
        if not battle.busy and battle.state.player_turn and not battle._deploying():
            if battle.state.active == key: ordered_aldren = true
            _key(KEY_D)
            defends += 1
            await process_frame
        await process_frame
    check(not ordered_aldren, "Aldren is never left waiting for the player's orders")
    check(defends > 0, "The player held the line with Defend (%d presses)" % defends)
    # The exchange: Drest, then Aldren, as bubbles over their own stacks, then in the log.
    var lines: Array = []
    for entry in seen:
        if entry[1] in [RUSH_LINE, RUSH_REPLY]: lines.append(entry)
    check(lines.size() == 2 and lines[0][0] == "DREST" and lines[0][1] == RUSH_LINE and lines[1][0] == "ALDREN" and lines[1][1] == RUSH_REPLY,
        "Drest's line, then Aldren's answer, show as bubbles in order (%s)" % [lines])
    check(lines.size() == 2 and lines[0][2] and lines[1][2], "Each bubble hangs over its speaker's own stack (%s)" % [lines])
    check(lines.size() == 2 and lines[0][3], "The exchange plays inside the reply's animation chain")
    check(not seen.any(func(entry): return entry[0] == "KHARIM"), "Kharim, not in the battle, says nothing")
    var first: int = battle.history.find("Drest: “%s”" % RUSH_LINE)
    var second: int = battle.history.find("Aldren: “%s”" % RUSH_REPLY)
    check(first >= 0 and second > first, "The log has Drest's line, then Aldren's (%d, %d)" % [first, second])
    var spoken: Array = battle.spoken.filter(func(entry): return entry.id == "b_rush")
    check(spoken.size() == 2, "The exchange is spoken once in the battle (repeat: battle)")
    # Aldren charged on his own, animated: announced, and seen between hexes.
    check(battle.history.any(func(line): return "Aldren" in line and "charges on his own — he takes no orders" in line), "The log says Aldren charges on his own")
    var between := 0
    for point in aldren_cells:
        var on_hex := false
        for cell in battle.board.grid_cells():
            if (Board.cell_point(cell) - point).length() < 1.5: on_hex = true
        if not on_hex: between += 1
    check(between > 0, "Aldren's charge is animated, not teleported (%d in-between positions)" % between)
    # Clicks never order him: on a turn where he stands active, nothing reaches the engine.
    if battle.state.result == "ongoing" and not battle.busy:
        var real: Dictionary = battle.state
        var forced: Dictionary = real.duplicate(true)
        forced.active = key   # as when his turn waits on a level-up choice
        battle._set_state(forced)
        battle._refresh()
        check(battle.board.locked, "On his turn the board suggests no moves")
        check(battle.defend.disabled, "...and Defend is off")
        check("CHARGES ON HIS OWN" in battle.turn_label.text, "The turn line says he charges on his own: " + battle.turn_label.text)
        var target: Array = []
        for unit in real.units:
            if not unit.player and int(unit.count) > 0: target = unit.cell
        var before: int = battle.history.size()
        _click_board(battle, target)
        await process_frame
        if not real.get("reachable", []).is_empty():
            _click_board(battle, real.reachable[0])
            await process_frame
        _key(KEY_D)
        await process_frame
        check(not battle.busy and battle.state == forced, "Clicking an enemy, a hex or D sends no order for Aldren")
        check(battle.history.slice(before).any(func(line): return line == "Aldren charges on his own and takes no orders."), "The log says why: he takes no orders")
        battle._set_state(real)
        battle._refresh()
        battle.animation_speed = 0.01
        battle.retreat.pressed.emit()
        battle.confirm_retreat.confirmed.emit()
        battle.confirm_retreat.hide()
        await _until(func(): return battle.state.get("result", "ongoing") != "ongoing" and not battle.busy, 20000)
    battle.queue_free()
    await process_frame

func _board_point(battle: Control, cell: Array) -> Vector2:
    return battle.board.get_global_transform_with_canvas() * Board.cell_point(cell)

func _click_board(battle: Control, cell: Array) -> void:
    var event := InputEventMouseButton.new()
    event.pressed = true
    event.button_index = MOUSE_BUTTON_LEFT
    event.position = _board_point(battle, cell)
    root.push_input(event, true)
    event = event.duplicate()
    event.pressed = false
    root.push_input(event, true)

func _hover_board(battle: Control, cell: Array) -> void:
    var motion := InputEventMouseMotion.new()
    motion.position = _board_point(battle, cell)
    root.push_input(motion, true)

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
