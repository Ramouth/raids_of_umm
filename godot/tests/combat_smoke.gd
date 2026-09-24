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
    scene.state.battle_companions = []   # the tuned fixtures below fight without companions
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
    scene.state.battle_companions = []
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

    # HoMM3 move-and-attack: pick the side with the cursor, walk and strike in one turn.
    scene.cleared_dungeons.clear()
    scene.army = [{"id": "rider_knight", "count": 6}, {"id": "armoured_warrior", "count": 4}]
    scene.encounter.guards = [{"id": "skeleton_warrior", "count": 20}, {"id": "sand_scorpion", "count": 5}]
    scene.state.battle_companions = []
    check(scene.enter_dungeon(), "Move-and-attack fixture starts")
    battle = scene.battle
    battle.animation_speed = 0.02
    await _idle(battle)
    var walk_target := {}
    for turn in range(8):
        for preview in battle.state.previews:
            for option in preview.options:
                if not option.path.is_empty(): walk_target = preview
        if not walk_target.is_empty() and walk_target.options.size() >= 2: break
        walk_target = {}
        check(battle.issue("defend"), "Hold until a guard comes into striking reach")
        await _idle(battle)
    check(not walk_target.is_empty(), "A guard comes within move + strike reach")
    if not walk_target.is_empty():
        var target_cell: Array = walk_target.cell
        var centre: Vector2 = battle.board.cell_point(target_cell)
        battle._cell_hovered(Vector2i(target_cell[0], target_cell[1]), true)
        var picked := {}
        for option in walk_target.options:
            # Point the cursor at each legal side in turn: that side must be chosen.
            var toward: Vector2 = (battle.board.cell_point(option.from) - centre).normalized()
            battle._pointer_moved(centre + toward * 20.0)
            if battle._stand.from == option.from: picked[str(option.from)] = true
        check(picked.size() == walk_target.options.size(), "Cursor side picks every legal standing hex")
        var walking: Dictionary = {}
        for option in walk_target.options:
            if not option.path.is_empty(): walking = option
        var toward: Vector2 = (battle.board.cell_point(walking.from) - centre).normalized()
        battle._pointer_moved(centre + toward * 20.0)
        check(battle.board.stand_cell == walking.from and battle.board.walk_path == walking.path, "Standing hex and walk path are highlighted")
        check("attack" in battle.status.get_parsed_text() and "damage, kills" in battle.status.get_parsed_text(), "Forecast is shown for the chosen side")
        if capture: await _capture("combat_hover_strike")
        var mover: String = battle.state.active
        var hp_before := 0
        for unit in battle.state.units:
            if unit.cell == target_cell: hp_before = int(unit.hp)
        battle._cell_clicked(Vector2i(target_cell[0], target_cell[1]))
        check(battle.busy, "Clicking the target orders move + attack")
        await _idle(battle)
        var mover_cell: Array = []
        var hp_after := hp_before
        for unit in battle.state.units:
            if unit.key == mover: mover_cell = unit.cell
            if unit.key == walk_target.key: hp_after = int(unit.hp)
        check(mover_cell == walking.from, "The stack ends on the chosen standing hex")
        check(hp_after < hp_before, "The strike lands in the same turn as the walk")
    battle.retreat.pressed.emit()
    battle.confirm_retreat.confirmed.emit()
    battle.confirm_retreat.hide()
    await _idle(battle)
    battle.return_button.pressed.emit()
    await process_frame
    print("Combat integration: move-and-attack side choice PASS")

    # Companions: gold-ringed single figures with an aura and a bodyguard.
    scene.cleared_dungeons.clear()
    scene.army = [{"id": "desert_archer", "count": 12}, {"id": "armoured_warrior", "count": 6}]
    scene.encounter.guards = [{"id": "grey_wolf", "count": 14}, {"id": "brigand", "count": 8}]
    scene.state.battle_companions = [{"id": "ushari", "count": 1, "level": 2, "companion": true}]
    check(scene.enter_dungeon(), "Companion fixture starts")
    battle = scene.battle
    battle.animation_speed = 0.02
    await _idle(battle)
    var ushari := {}
    for unit in battle.state.units:
        if unit.get("companion", false): ushari = unit
    check(not ushari.is_empty() and ushari.name == "Ushari", "Ushari rides into battle")
    check(int(ushari.get("unit_hp", 0)) == 70, "Her health grows with her level (60 + 10)")
    check(ushari.get("cell", []) == [0, 2], "She starts at the centre of the back line")
    check(not str(ushari.get("bodyguard", "")).is_empty(), "A troop stack beside her is her bodyguard")
    var shielded := false
    for unit in battle.state.units:
        if unit.player and int(unit.get("aura_bonus", 0)) == 3: shielded = true
    check(shielded, "Her aura gives a neighbouring stack +3 defence")
    check(battle.board.actors[ushari.key].get_node("Count").text.begins_with("♥"), "Her badge shows health, not a head count")
    battle._cell_right_clicked(Vector2i(0, 2))
    check(battle.inspection.get_parsed_text().contains("Bodyguard") and battle.inspection.get_parsed_text().contains("Aura"), "Inspector explains her aura and bodyguard")
    if capture: await _capture("combat_companion")
    # Hold until the enemy can reach her: the warning must say so.
    var warned := false
    for turn in range(10):
        battle._cell_hovered(Vector2i.ZERO, false)
        if "exposed" in battle.warning.get_parsed_text():
            warned = true
            break
        if battle.state.result != "ongoing": break
        check(battle.issue("defend"), "Hold while the guards close in")
        await _idle(battle)
    check(warned, "A warning names the enemies that can reach a companion")
    if capture: await _capture("combat_companion_exposed")
    # An archer's shot shows its line of fire and whether it is clear.
    for turn in range(6):
        var active: Dictionary = battle.units.get(battle.state.active, {})
        if battle.state.result != "ongoing" or (active.get("ranged", false) and not battle.state.attackable.is_empty()): break
        battle.issue("defend")
        await _idle(battle)
    var shooter: Dictionary = battle.units.get(battle.state.active, {})
    if battle.state.result == "ongoing" and shooter.get("ranged", false) and not battle.state.attackable.is_empty():
        var aim: Array = battle.state.attackable[0]
        battle._cell_hovered(Vector2i(aim[0], aim[1]), true)
        check(battle.board.shot_line.size() == 2, "Hovering a shot draws the line of fire")
        var said: String = battle.status.get_parsed_text()
        check("line of sight" in said or "BLOCKED SHOT" in said, "The forecast says whether the shot is clear")
        if capture: await _capture("combat_line_of_sight")
    else:
        check(false, "An archer gets a shot during the companion fixture")
    battle.retreat.pressed.emit()
    battle.confirm_retreat.confirmed.emit()
    battle.confirm_retreat.hide()
    await _idle(battle)
    check(not battle.state.survivors.any(func(s): return s.id == "ushari"), "Companions never come back as troops")
    battle.return_button.pressed.emit()
    await process_frame
    print("Combat integration: companions, aura, bodyguard, line of sight PASS")

    scene.cleared_dungeons.clear()
    scene.army = [{"id": "skeleton_warrior", "count": 1}]
    scene.encounter.guards = [{"id": "djinn", "count": 1000}]
    scene.state.battle_companions = [{"id": "ushari", "count": 1, "level": 1, "companion": true}]
    check(scene.enter_dungeon(), "Defeat fixture starts")
    battle = scene.battle
    battle.animation_speed = 0.01
    battle.auto_button.button_pressed = true
    while battle.busy or battle.state.result == "ongoing": await process_frame
    check(battle.state.result == "defeat", "Native AI reaches defeat")
    check(battle.state.survivors.is_empty() and battle.state.rewards.is_empty(), "Defeat returns neither units nor new rewards")
    check(battle.state.fallen == ["ushari"], "A companion who falls in a lost battle is reported")
    check("FALLEN" in battle.inspection.get_parsed_text(), "The result screen names the fallen")
    battle.return_button.pressed.emit()
    await process_frame
    check(not scene.state.specials.any(func(sc): return sc.id == "ushari"), "Lost with the battle: Ushari is gone")
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
