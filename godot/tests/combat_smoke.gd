extends SceneTree

var failures := 0
var scene: Node2D
var dungeon := Vector2i.ZERO
var capture := false

func _initialize() -> void:
    create_timer(90).timeout.connect(func(): push_error("Combat integration timed out"); quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func _idle(battle: Control) -> void:
    while battle.busy or (battle.state.result == "ongoing" and not battle.state.player_turn):
        await process_frame

func _click(control: Control, local: Vector2) -> void:
    var event := InputEventMouseButton.new()
    event.pressed = true
    event.button_index = MOUSE_BUTTON_LEFT
    event.position = control.get_global_transform_with_canvas() * local
    root.push_input(event, true)
    event = event.duplicate()
    event.pressed = false
    root.push_input(event, true)

func _run() -> void:
    capture = "--capture" in OS.get_cmdline_user_args()
    if capture and DisplayServer.get_name() != "headless":
        # Capture windows must never steal the user's keyboard or mouse.
        DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    check(ClassDB.class_exists("UmmCombat"), "Real C++ extension loads in Godot")
    scene = load("res://scenes/desert.tscn").instantiate()
    # Written against the canonical map, not the demo map.
    scene.get_node("Map").map_path = "res://content/maps/default.json"
    root.add_child(scene)
    await process_frame
    check(not scene.enter_dungeon(), "Cannot enter a dungeon from a town")
    for cell in scene.data.objects:
        if scene.data.objects[cell].type == "dungeon":
            dungeon = cell
            break
    scene.fog.reveal([[dungeon.x, dungeon.y]])  # scouted: the test targets it directly
    # Fixture army the casualty/retaliation checks below were tuned against.
    scene.army = [{"id": "desert_archer", "count": 10}, {"id": "mummy", "count": 3}]
    check(scene.travel_to(dungeon), "A real map route reaches a dungeon")
    while scene.hero.moving: await process_frame
    check(scene._enter_button.visible and not scene._enter_button.disabled, "Arrival enables dungeon entry")
    _click(scene._enter_button, scene._enter_button.size / 2)
    await process_frame
    check(is_instance_valid(scene.battle), "Actual dungeon button starts native combat")
    var battle: Control = scene.battle
    battle.animation_speed = 0.02
    await _idle(battle)
    check(not scene.is_processing(), "Adventure camera input is suspended")
    check(not scene.travel_to(scene.data.spawn), "Adventure movement is blocked during battle")
    check(battle.state.units.size() == 4, "Original expedition and dungeon guards are present")
    if capture: await _capture("combat")
    # Clarity UI: turn order, inspector, hover forecast.
    check(battle.initiative.get_child_count() >= 4, "Turn-order bar shows every stack")
    check(battle.inspection.get_parsed_text().contains("ACTING NOW"), "Inspector describes the active stack")
    var target: Array = battle.state.attackable[0]
    battle._cell_hovered(Vector2i(target[0], target[1]), true)
    check(battle.board.hover_kind == "attack" and "slain" in battle.board.hover_label, "Hovering a target shows the damage forecast on the board")
    check("damage, kills" in battle.status.get_parsed_text(), "Status bar spells out damage and kills")
    if capture: await _capture("combat_hover_attack")
    var enemy_far: Dictionary = {}
    for unit in battle.state.units:
        if not unit.player and not (unit.cell in battle.state.attackable): enemy_far = unit
    if not enemy_far.is_empty() and not battle.state.units[0].ranged:
        battle._cell_hovered(Vector2i(enemy_far.cell[0], enemy_far.cell[1]), true)
        check("out of reach" in battle.status.get_parsed_text(), "Unreachable melee target is explained")
    var walk: Array = battle.state.reachable[0]
    battle._cell_hovered(Vector2i(walk[0], walk[1]), true)
    check(battle.board.hover_kind == "move" and "ends" in battle.status.get_parsed_text(), "Movement hover explains that moving ends the turn")
    battle._cell_right_clicked(Vector2i(battle.state.units[3].cell[0], battle.state.units[3].cell[1]))
    battle._cell_hovered(Vector2i.ZERO, false)
    check(battle.inspection.get_parsed_text().contains(battle.state.units[3].name), "Right-click pins a stack in the inspector")
    _click(battle.board, battle.board.cell_point(target))
    check(battle.busy, "Actual battlefield click starts an attack animation")
    check(not battle.issue("defend"), "Rapid follow-up command is blocked during animation")
    await _idle(battle)
    check(battle.history.any(func(line: String): return "damage" in line), "Native damage events reach the combat log")
    battle.defend.pressed.emit()
    check(battle.busy, "Defend button dispatches a native action")
    await _idle(battle)
    var destination: Array = battle.state.reachable[0]
    var moving_key: String = battle.state.active
    _click(battle.board, battle.board.cell_point(destination))
    check(battle.busy, "Actual green-hex click starts movement")
    await _idle(battle)
    check(battle.board.actors[moving_key].position == battle.board.cell_point(destination), "Movement animation ends on the authoritative hex")
    # Let the guards close and retaliate, then verify casualties survive return.
    for turn in range(2):
        check(battle.issue("defend"), "Player can hold position while guards advance")
        await _idle(battle)
    var remaining := 0
    for stack in battle.state.survivors: remaining += int(stack.count)
    check(remaining < 13, "Enemy melee inflicts real expedition casualties")
    check(battle.history.any(func(line: String): return "retaliation" in line), "Retaliation events are presented")
    check(battle.history.any(func(line: String): return "Round 2" in line), "Round changes are logged")
    if capture: await _capture("combat_log")
    # Retreat through the real confirmation dialog; preserve casualties exactly.
    var survivors: Array = battle.state.survivors.duplicate(true)
    battle.retreat.pressed.emit()
    check(battle.confirm_retreat.visible, "Retreat requires confirmation")
    battle.confirm_retreat.confirmed.emit()
    battle.confirm_retreat.hide()
    await _idle(battle)
    check(battle.state.result == "retreat", "Retreat reaches its result screen")
    battle.return_button.pressed.emit()
    await process_frame
    check(scene.army == survivors, "Returning preserves exact surviving stack counts")
    check(scene.inventory.is_empty() and scene.cleared_dungeons.is_empty(), "Retreat grants no loot and leaves dungeon guarded")
    check(scene.is_processing() and scene.hero.cell == dungeon, "Map resumes at the same dungeon")
    print("Combat integration: manual attack, enemy AI, defend, retreat PASS")

    # Lopsided fixtures make outcomes deterministic without changing production RNG.
    scene.army = [{"id": "desert_archer", "count": 1000}]
    check(scene.enter_dungeon(), "Uncleared dungeon can be challenged again")
    battle = scene.battle
    battle.animation_speed = 0.01
    battle.auto_button.button_pressed = true
    while battle.busy or battle.state.result == "ongoing": await process_frame
    check(battle.state.result == "victory", "Native auto-battle reaches victory")
    survivors = battle.state.survivors.duplicate(true)
    if capture: await _capture("combat_victory")
    battle.return_button.pressed.emit()
    # Duplicate callbacks must not pay out twice.
    battle.return_button.pressed.emit()
    await process_frame
    check(scene.army == survivors, "Victory survivors reach the expedition")
    check(scene.inventory.size() == 1 and scene.inventory[0].id == "scarab_amulet", "Victory awards exactly one configured artifact")
    check(scene.cleared_dungeons.has(dungeon) and not scene.enter_dungeon(), "Cleared dungeon cannot be farmed for duplicate loot")
    print("Combat integration: victory, survivors, one-time loot PASS")

    scene.cleared_dungeons.clear()
    scene.army = [{"id": "skeleton_warrior", "count": 1}]
    scene.encounter.guards = [{"id": "djinn", "count": 1000}]
    check(scene.enter_dungeon(), "Defeat fixture starts")
    battle = scene.battle
    battle.animation_speed = 0.01
    battle.auto_button.button_pressed = true
    while battle.busy or battle.state.result == "ongoing": await process_frame
    check(battle.state.result == "defeat", "Native AI reaches defeat")
    check(battle.state.survivors.is_empty() and battle.state.rewards.is_empty(), "Defeat returns neither units nor new rewards")
    battle.return_button.pressed.emit()
    await process_frame
    check(scene.army.is_empty() and not scene.enter_dungeon(), "Defeated army is not silently replenished")
    scene.new_expedition()
    check(scene.army == scene.STARTING_ARMY and scene.inventory.is_empty() and scene.hero.cell == scene.data.spawn, "Explicit new expedition resets session state")
    scene.queue_free()
    await process_frame
    print("Godot combat integration: %s" % ("PASS" if failures == 0 else "%d failures" % failures))
    quit(0 if failures == 0 else 1)

func _capture(label: String) -> void:
    if DisplayServer.get_name() == "headless": return
    await process_frame
    await RenderingServer.frame_post_draw
    var folder := ProjectSettings.globalize_path("res://artifacts")
    DirAccess.make_dir_recursive_absolute(folder)
    check(root.get_texture().get_image().save_png(folder.path_join(label + ".png")) == OK, "Combat screenshot saved")
