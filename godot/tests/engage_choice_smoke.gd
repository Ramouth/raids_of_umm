extends SceneTree
## Run with --headless --script res://tests/engage_choice_smoke.gd.
## A guard whose engage trigger raises a conversation (the Northern Warrior on
## 1.2, the Hooded Stranger on 1.1) must be heard out before any battle: the
## player walks up to two such guards through the real game root. Answering
## "Let him go" (a withdraw) starts no battle; answering "Fight" starts it
## after the conversation.

const Fixture = preload("res://tests/campaign_fixture.gd")
const DIR := "user://engage_choice_smoke"
var failures := 0

func _initialize() -> void:
    create_timer(90.0).timeout.connect(func():
        push_error("Engage choice smoke timed out")
        quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func _choice(id: String, name: String) -> Dictionary:
    return {"id": id, "when": {"event": "engage", "name": name}, "do": [{"choice": {
        "id": id, "title": "A Man in Grey", "presentation": "conversation",
        "text": "A man in grey stands in your road, hand on his axe. He has not drawn it yet.",
        "options": [
            {"id": id + "_go", "label": "Let him go", "then": [{"withdraw": name}]},
            {"id": id + "_fight", "label": "Fight", "then": []}]}}]}

func _run() -> void:
    var campaign_file := Fixture.write_guard_map(DIR, "9.9", [],
        {"Grey Stranger": [{"id": "brigand", "count": 2}], "Red Stranger": [{"id": "brigand", "count": 2}]},
        [_choice("grey", "Grey Stranger"), _choice("red", "Red Stranger")])
    Fixture.clear_saves(DIR)
    var game: Node = load("res://scenes/game.tscn").instantiate()
    game.campaign_path = campaign_file
    game.save_dir = DIR
    game.skip_intro = true
    root.add_child(game)
    await process_frame
    _button(game._menu_layer, "New expedition").pressed.emit()
    await process_frame
    _key(KEY_ESCAPE)   # skip the new-expedition cutscene
    await _until(func(): return game.adventure != null)
    var scene: Node2D = game.adventure
    check(scene != null, "The fixture map starts")
    if scene == null: return _finish()
    await _until(func(): return not scene.turn_busy and game.screens.depth() == 0)

    # 1. Walk into the Grey Stranger: his conversation comes first, not a battle.
    var conversation := await _engage(scene, game, Vector2i(2, 0))
    check(conversation != null and str(conversation.choice.get("id", "")) == "grey", "Walking into the Grey Stranger opens his conversation")
    check(not is_instance_valid(scene.battle), "No battle starts while his question is open")
    check(scene.state.get("encounter_awaits_choice", false), "The engine says the fight waits for the answer (encounter_awaits_choice)")
    check(not scene.has_method("_choice_before_fight"), "The client does not guess at it any more")
    if conversation != null:
        var go := _button(conversation, "Let him go")
        check(go != null and go.visible and not go.disabled, "\"Let him go\" is offered")
        if go != null: go.pressed.emit()
    for i in 20: await process_frame
    check(not is_instance_valid(scene.battle), "Letting him go starts no battle")
    check(scene.state.get("encounter") == null, "He withdraws: no fight is left pending")

    # 2. Walk into the Red Stranger and choose to fight: the battle follows the answer.
    conversation = await _engage(scene, game, Vector2i(2, -2))
    check(conversation != null and str(conversation.choice.get("id", "")) == "red", "Walking into the Red Stranger opens his conversation")
    check(not is_instance_valid(scene.battle), "Still no battle before the answer")
    var early: Dictionary = JSON.parse_string(scene.adventure.resolve_encounter(true))
    check(not early.get("ok", true) and str(early.get("error", "")).contains("Answer the story choice"), "The engine refuses to settle the fight before the answer")
    if conversation != null:
        var fight := _button(conversation, "Fight")
        check(fight != null and fight.visible, "\"Fight\" is offered")
        if fight != null: fight.pressed.emit()
    await _until(func(): return is_instance_valid(scene.battle))
    check(is_instance_valid(scene.battle), "Choosing Fight starts the battle after the conversation")
    check(not scene.state.get("encounter_awaits_choice", true), "Once answered, the fight no longer waits")
    if is_instance_valid(scene.battle):
        var battle: Control = scene.battle
        battle.animation_speed = 0.01
        await _until(func(): return not battle.busy)
        battle.retreat.pressed.emit()
        battle.confirm_retreat.confirmed.emit()
        battle.confirm_retreat.hide()
        await _until(func(): return battle.state.get("result", "ongoing") != "ongoing" and not battle.busy, 20000)
        battle.return_button.pressed.emit()
        await process_frame
    _finish()

## Clicks the guard's hex (plan, then go) and returns the conversation that opens.
func _engage(scene: Node2D, game: Node, cell: Vector2i) -> Control:
    scene.fog.reveal([[cell.x, cell.y]])
    _focus(scene, cell)
    _click(scene, cell)
    await process_frame
    _click(scene, cell)
    await process_frame
    await process_frame
    if not scene.hero.moving and scene.state.get("encounter") == null:
        _key(KEY_ENTER)   # the plan stands: set off, as the notice says
        await process_frame
    await _until(func(): return not scene.hero.moving, 8000)
    await _until(func():
        var top: Control = game.screens.top()
        return is_instance_valid(scene.battle) or (top != null and top.get("choice") is Dictionary and not top.choice.is_empty()))
    var top: Control = game.screens.top()
    if top != null and top.get("choice") is Dictionary and not top.choice.is_empty(): return top
    return null

func _finish() -> void:
    print("Engage choice: ", "PASS" if failures == 0 else "FAIL")
    quit(failures)

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

func _button(node: Node, text: String) -> Button:
    if node == null: return null
    for button in node.find_children("*", "Button", true, false):
        if (button as Button).text.begins_with(text): return button
    return null
