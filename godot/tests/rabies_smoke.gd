extends SceneTree
## Rabies and the shut bridge on map 1.1, as the player sees them:
## bite marks and notes on bitten stacks, the mad turn named on the turn label
## and in the log, bitten survivors carried back into the army and cured by a
## night in your own town, and the bridge gate drawn shut, then open.
## Run: godot --headless --audio-driver Dummy --path godot --script res://tests/rabies_smoke.gd
const CombatView = preload("res://scripts/combat.gd")
const BRIDGE := Vector2i(13, -5)   # the east bridge on ch1_1_1_varenhold
var failures := 0

func check(ok: bool, message: String) -> void:
    if not ok:
        failures += 1
        push_error("FAIL: " + message)

func _initialize() -> void:
    create_timer(240).timeout.connect(func(): push_error("Rabies smoke timed out"); quit(1))
    _run.call_deferred()

func _run() -> void:
    check(ClassDB.class_exists("UmmCombat") and ClassDB.class_exists("UmmAdventure"), "Native extension loads")
    await _bitten_in_battle()
    await _madness()
    await _army_round_trip()
    await _bridge()
    print("Rabies: ", "PASS" if failures == 0 else "FAIL (%d)" % failures)
    quit(failures)

func _player_unit(battle: Control, id: String) -> Dictionary:
    for unit in battle.state.units:
        if unit.player and unit.id == id: return unit
    return {}

## A stack that comes into battle bitten wears the bite mark and says what it means.
func _bitten_in_battle() -> void:
    var battle: Control = CombatView.new()
    root.add_child(battle)
    await process_frame
    battle.animation_speed = 0.02
    var army := [{"id": "woad_runner", "count": 12, "bitten": true}, {"id": "painted_blade", "count": 4}]
    check(battle.begin(army, {"guards": [{"id": "rabid_hound", "count": 6}], "seed": 3}, "Orchard Dogs"), "A battle against rabid hounds starts")
    var runner := _player_unit(battle, "woad_runner")
    var blade := _player_unit(battle, "painted_blade")
    check(runner.get("bitten", false) and not blade.get("bitten", true), "The snapshot marks only the stack that came in bitten")
    check(battle.board.actors[runner.key].get_node("Bitten").visible, "The bitten stack wears the bite mark on the board")
    check(not battle.board.actors[blade.key].get_node("Bitten").visible, "An unbitten stack has no bite mark")
    battle._inspect(runner)
    var text: String = battle.inspection.get_parsed_text()
    check("BITTEN" in text and "friend or foe" in text and "town you hold" in text, "Inspecting the bitten stack explains madness and the cure: " + text)
    var portrait: Control = battle._portrait(runner.key, false)
    check("BITTEN" in portrait.tooltip_text, "The turn-order portrait tooltip names the bite")
    portrait.free()
    for unit in battle.state.units:
        if not unit.player:
            battle._inspect(unit)
            check("Rabid:" in battle.inspection.get_parsed_text(), "A rabid hound's details say its bite infects")
            break
    battle.queue_free()
    await process_frame

## Bitten stacks fight on auto-battle until one goes mad: the turn label and
## the log say so, the board tags it, and its blows are logged as madness.
func _madness() -> void:
    var seen_label := ""
    var seen_tag := false
    var seen_status := ""
    var log_text := ""
    for seed in range(1, 40):
        var battle: Control = CombatView.new()
        root.add_child(battle)
        await process_frame
        battle.animation_speed = 0.01
        var army := [{"id": "woad_runner", "count": 30, "bitten": true}, {"id": "cruth_slinger", "count": 20, "bitten": true},
            {"id": "painted_blade", "count": 10, "bitten": true}]
        check(battle.begin(army, {"guards": [{"id": "rabid_hound", "count": 4}], "seed": seed}, "Madness test"), "Madness battle starts")
        battle.auto_button.button_pressed = true   # the AI fights both sides; madness still takes its turns
        var started := Time.get_ticks_msec()
        while battle.state.result == "ongoing" and Time.get_ticks_msec() - started < 20000:
            if battle.state.get("mad_turn", false) and not battle.busy and seen_label.is_empty():
                seen_label = battle.turn_label.text
                seen_status = battle.status.get_parsed_text()
                var active: Dictionary = battle.units.get(battle.state.active, {})
                seen_tag = battle.board.actors[active.key].get_node("Mad").visible if not active.is_empty() else false
            await process_frame
        log_text = "\n".join(battle.history)
        battle.queue_free()
        await process_frame
        if "HAS GONE MAD" in log_text and not seen_label.is_empty(): break
    check("HAS GONE MAD from a rabid bite" in log_text, "The log announces a stack going mad and why")
    check("HAS GONE MAD" in seen_label, "The turn label names the mad stack on its turn: " + seen_label)
    check(seen_tag, "The mad stack wears the MAD tag on the board")
    check("friend or foe" in seen_status, "The status line says a mad stack attacks friend or foe: " + seen_status)
    check("In its madness" in log_text, "The mad stack's attack is logged as madness, naming whom it hit")

## Bitten survives the trip into the session, into battle and back, and a
## night in your own town cures it, with the cure said on screen.
func _army_round_trip() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    scene.army = [{"id": "woad_runner", "count": 30, "bitten": true}, {"id": "cruth_slinger", "count": 10}]
    check(scene.state.army.size() == 2 and scene.state.army[0].get("bitten", false) and not scene.state.army[1].get("bitten", true),
        "set_army carries the bite into the native session")
    scene._update_expedition()
    check("30 woad runner (bitten)" in scene._army_label.text and "End the day in a town you hold" in scene._army_label.text,
        "The sidebar marks the bitten stack and says how to cure it: " + scene._army_label.text)
    scene.state.battle_companions = []
    check(scene.start_battle({"guards": [{"id": "rabid_hound", "count": 2}], "seed": 5}, "Test dogs", func(_r): pass), "A battle starts from the map")
    var battle: Control = scene.battle
    battle.animation_speed = 0.01
    var runner := _player_unit(battle, "woad_runner")
    check(runner.get("bitten", false), "The bitten stack goes into battle still bitten")
    battle.auto_button.button_pressed = true
    var started := Time.get_ticks_msec()
    while battle.state.result == "ongoing" and Time.get_ticks_msec() - started < 30000: await process_frame
    check(battle.state.result == "victory", "The expedition beats two hounds")
    var survivor_bitten := false
    for unit in battle.state.survivors:
        if unit.id == "woad_runner": survivor_bitten = unit.get("bitten", false)
    check(survivor_bitten, "Survivors report the bitten stack as bitten")
    if battle.state.result != "ongoing":
        check("(bitten)" in battle.inspection.get_parsed_text() and "town you hold" in battle.inspection.get_parsed_text(), "The result screen marks bitten survivors and the cure")
        battle._return()
        await process_frame
    var bitten_after := false
    for stack in scene.state.army:
        if stack.id == "woad_runner": bitten_after = stack.get("bitten", false)
    check(bitten_after, "Bitten survivors go back into the session army bitten")
    # The expedition starts in Varenhold, a town you hold: end the day there.
    var town: Dictionary = scene.data.objects.get(scene.hero.cell, {})
    check(town.get("type", "") == "town", "The hero starts in a town")
    scene.end_day()
    var card: Node = scene.get_node_or_null("HUD/DayTransition")
    check(card != null and "tended" in card.get_child(0).text, "The night card says the bitten were tended")
    while scene.turn_busy: await process_frame
    check("The bitten are tended in" in scene.notice.text, "The notice after End Day names the town that tended them: " + scene.notice.text)
    var cured := true
    for stack in scene.army: cured = cured and not stack.get("bitten", false)
    check(cured, "The client's army mirror drops the bite after the night in town")
    check(not "(bitten)" in scene._army_label.text, "The sidebar no longer marks anyone bitten")
    scene.queue_free()
    await process_frame

## The east bridge on map 1.1 draws barred until the story opens it.
func _bridge() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = "res://content/maps/ch1_1_1_varenhold.json"
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    var view: Node2D = scene.map_view
    check(view.is_shut_bridge(BRIDGE), "The east bridge is shut")
    var gate: Node2D = view.gates.get(BRIDGE)
    check(gate != null and gate.get_node("Barrier").visible and not gate.get_node("Open").visible, "A shut bridge draws its barred gate")
    check(not scene.data.is_passable(BRIDGE), "The shut bridge is not walkable")
    scene.fog.reveal([[BRIDGE.x, BRIDGE.y]])
    scene._inspect(BRIDGE)
    var about: String = scene.sidebar.get_node("Inspection").text
    check("Bridge · shut" in about and "Bridge Keeper" in about, "Hovering the shut bridge names it and its keeper: " + about)
    var next: Dictionary = scene.state.duplicate(true)
    next["opened"] = [[BRIDGE.x, BRIDGE.y]]
    scene._apply_state(next)
    check(not view.is_shut_bridge(BRIDGE), "The snapshot's opened cell opens the bridge")
    check(not gate.get_node("Barrier").visible and gate.get_node("Open").visible, "An opened bridge draws its gate swung aside")
    check(scene.data.is_passable(BRIDGE), "The opened bridge is walkable for the client's hover and route logic")
    scene._inspect(BRIDGE)
    check(not "shut" in scene.sidebar.get_node("Inspection").text, "Hovering the open bridge no longer says shut")
    scene.queue_free()
    await process_frame
