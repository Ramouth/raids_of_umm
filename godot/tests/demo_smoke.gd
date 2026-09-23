extends SceneTree
## Demo loop: guards block the pass, battles run through the screen stack,
## the passage mine wins, and losing the army ends the expedition.
## Run with --headless --script res://tests/demo_smoke.gd

const WEEK2_ARMY := [{"id": "levy_spearman", "count": 50}, {"id": "desert_archer", "count": 26},
    {"id": "armoured_warrior", "count": 10}, {"id": "rider_archer", "count": 4}]
const RIDGE_GUARD := Vector2i(-6, 3)

var failures := 0

func _initialize() -> void:
    create_timer(120).timeout.connect(func(): push_error("Demo smoke timed out"); quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func _run() -> void:
    await _root_scene()
    await _story()
    await _town_recruiting()
    await _ridge_pass()
    await _passage_wins()
    await _defeat_ends()
    await _rival_raids()
    print("Demo smoke: ", "PASS" if failures == 0 else "%d failures" % failures)
    quit(failures)

func _root_scene() -> void:
    var game: Node = load("res://scenes/game.tscn").instantiate()
    root.add_child(game)
    await process_frame
    check(game.adventure != null and game.screens.base == game.adventure, "Game root hosts the adventure as the base screen")
    check(game.adventure.screens == game.screens, "Adventure pushes onto the root screen stack")
    game.queue_free()
    await process_frame

func _scene(map_path: String, army: Array) -> Node2D:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = map_path
    scene.passage_seed = 1
    root.add_child(scene)
    await process_frame
    scene.army = army.duplicate(true)
    return scene

func _auto_battle(scene: Node2D) -> String:
    var battle: Control = scene.battle
    battle.animation_speed = 0.02
    battle.auto_button.button_pressed = true
    while battle.busy or battle.state.result == "ongoing": await process_frame
    var result: String = battle.state.result
    battle.return_button.pressed.emit()
    await process_frame
    return result

func _story() -> void:
    var scene := await _scene("res://content/maps/old_passage.json", WEEK2_ARMY)
    check(scene.dialogue.is_speaking(), "The intro transmission plays at the start")
    check(scene.dialogue._speaker.text == "USHARI", "Ushari opens the story")
    check(scene.state.quests.size() == 1 and scene.state.quests[0].main, "The main quest is given")
    scene.toggle_quest_log()
    check(scene.quest_log.visible, "Q opens the quest log")
    scene.toggle_quest_log()
    scene.dialogue.skip_all()
    check(not scene.dialogue.is_speaking(), "Dialogue can be dismissed")
    check(scene.state.specials.size() == 1 and scene.state.specials[0].id == "ushari", "Ushari rides with the hero")
    check(scene.open_party(), "The companions screen opens")
    var party: Control = scene.screens.top()
    party._station("ushari", true)
    check(scene.state.specials[0].stationed != null, "Ushari can govern Khemret")
    party._station("ushari", false)
    check(scene.state.specials[0].stationed == null, "Ushari can be recalled")
    party.find_child("Done", true, false).pressed.emit()
    await process_frame
    scene.queue_free()
    await process_frame
    # Offers: a tiny map where a camp sells a clue for gold.
    var path := _tiny_map("demo_offer", [{"id": "skeleton_warrior", "count": 3}], true)
    scene = await _scene(path, WEEK2_ARMY)
    check(scene.travel_to(Vector2i(0, 1)), "Walk to the scholar's camp")
    while scene.hero.moving: await process_frame
    check(scene._offer_box.get_child_count() == 1, "The camp's offer appears in the sidebar")
    var gold := int(scene.state.treasury.Gold)
    check(scene.accept_offer("clue"), "Paying for a clue succeeds")
    check(int(scene.state.treasury.Gold) == gold - 100, "The clue costs its price")
    check(scene.dialogue.is_speaking(), "The clue is delivered as dialogue")
    scene.queue_free()
    await process_frame

func _town_recruiting() -> void:
    var scene := await _scene("res://content/maps/old_passage.json", [{"id": "levy_spearman", "count": 24}])
    check(scene._town_button.visible, "Enter-town button shows in Khemret")
    check(scene.open_town(), "The hero can enter Khemret")
    var town: Control = scene.screens.top()
    check(town != null and town.has_method("recruit"), "Town screen is pushed")
    check(town._cards.get_child_count() == 5, "Khemret offers the five Ivory Compact tiers")
    var gold := int(scene.state.treasury.Gold)
    check(town.recruit("levy_spearman", 6), "Recruiting levies succeeds")
    check(int(scene.state.treasury.Gold) == gold - 300, "Recruiting spends 50 gold per levy")
    check(scene.army.any(func(s): return s.id == "levy_spearman" and s.count == 30), "Recruits merge into the existing stack")
    check(not town.recruit("rider_knight", 2), "Cannot afford two knights (1800 gold, 1700 left)")
    check(not town.recruit("levy_spearman", 999), "Cannot recruit past the weekly pool")
    town.find_child("Leave", true, false).pressed.emit()
    await process_frame
    check(scene.open_garrison(), "The hero can garrison Khemret")
    var garrison: Control = scene.screens.top()
    check(garrison.move("levy_spearman", 10, true), "Ten levies stay behind as a garrison")
    check(scene.army.any(func(s): return s.id == "levy_spearman" and s.count == 20), "The garrisoned levies leave the army")
    check(not garrison.move("levy_spearman", 20, true), "The hero cannot leave without an army")
    check(garrison.move("levy_spearman", 5, false), "Troops can rejoin from the garrison")
    garrison.find_child("Done", true, false).pressed.emit()
    await process_frame
    check(scene.screens.depth() == 0 and scene.get_node("HUD").visible, "Leaving the town returns to the map")
    scene.queue_free()
    await process_frame

func _ridge_pass() -> void:
    var scene := await _scene("res://content/maps/old_passage.json", WEEK2_ARMY)
    check(scene._guarded(RIDGE_GUARD), "Ridge Pass starts guarded")
    var beyond: Dictionary = JSON.parse_string(scene.adventure.preview(-4, 2))
    check(beyond.path.is_empty(), "No route through the guarded pass")
    check(scene.travel_to(RIDGE_GUARD), "Player can order an attack on the pass guards")
    while scene.hero.moving: await process_frame
    check(is_instance_valid(scene.battle), "Reaching the guards starts a battle")
    check(scene.screens.depth() == 1, "Battle is pushed on the screen stack")
    check(scene.battle.heading.text == "RIDGE PASS GUARD", "Battle is titled after the guards")
    check(not scene.get_node("HUD").visible, "Adventure HUD is hidden under the battle")
    var result := await _auto_battle(scene)
    check(result == "victory", "A week-two army beats the pass guards")
    check(scene.screens.depth() == 0 and scene.get_node("HUD").visible, "Battle pops back to the adventure")
    check(not scene._guarded(RIDGE_GUARD), "Beaten guards no longer hold the pass")
    check(scene.hero.cell == RIDGE_GUARD, "Hero steps into the pass after victory")
    check(not scene.map_view.anchors[RIDGE_GUARD].visible, "Guard camp disappears from the map")
    check(int(scene.state.treasury.Gold) >= 2750, "Guard reward gold is paid")
    check(scene.inventory.any(func(item): return item.id == "scarab_amulet"), "Guard item drop joins the inventory")
    beyond = JSON.parse_string(scene.adventure.preview(-4, 2))
    check(not beyond.path.is_empty(), "The pass is open")
    scene.queue_free()
    await process_frame

## A two-hex map: the only old mine must hide the passage.
func _tiny_map(name: String, mine_guards: Array, with_camp := false) -> String:
    var tiles := []
    for q in range(-2, 3):
        for r in range(-2, 3):
            if absi(q + r) <= 2:
                tiles.append({"q": q, "r": r, "terrain": "sand", "passable": true, "moveCost": 1.0, "variant": 0})
    var map := {"version": 1, "name": "Test", "radius": 2, "tiles": tiles, "objects": [
        {"q": -1, "r": 0, "type": "town", "name": "Camp", "factionId": 1},
        {"q": 1, "r": 0, "type": "old_mine", "name": "Last Mine"}]}
    if with_camp:
        map.objects.append({"q": 0, "r": 1, "type": "quest_giver", "name": "Camp of Scholars"})
        map.objects.append({"q": 1, "r": -2, "type": "old_mine", "name": "Other Mine"})
        FileAccess.open("user://%s.triggers.json" % name, FileAccess.WRITE).store_string(JSON.stringify({
            "offers": [{"id": "clue", "label": "Buy a clue (100 gold)", "at": "Camp of Scholars", "cost": {"Gold": 100}, "then": [{"clue": true}]}],
            "triggers": [{"id": "meet", "when": {"event": "visit", "name": "Camp of Scholars"}, "do": [{"offer": "clue"}]}]}))
    var path := "user://%s.json" % name
    FileAccess.open(path, FileAccess.WRITE).store_string(JSON.stringify(map))
    FileAccess.open("user://%s.encounters.json" % name, FileAccess.WRITE).store_string(
        JSON.stringify({"objects": {"Last Mine": {"guards": mine_guards}}}))
    return path

func _passage_wins() -> void:
    var scene := await _scene(_tiny_map("demo_win", [{"id": "skeleton_warrior", "count": 3}]), WEEK2_ARMY)
    check(scene.travel_to(Vector2i(1, 0)), "Player attacks the last old mine")
    while scene.hero.moving: await process_frame
    check(is_instance_valid(scene.battle), "The old mine is guarded")
    check(await _auto_battle(scene) == "victory", "The mine guards fall")
    check(scene.state.won, "Clearing the passage mine wins the scenario")
    var top: Control = scene.screens.top()
    check(top != null and top.victory, "Victory screen is shown")
    top.find_child("NewExpedition", true, false).pressed.emit()
    await process_frame
    check(scene.screens.depth() == 0 and not scene.state.won and scene.state.day == 1, "New expedition restarts the scenario")
    check(scene._guarded(Vector2i(1, 0)), "Restart restores the guards")
    scene.queue_free()
    await process_frame

func _defeat_ends() -> void:
    var scene := await _scene(_tiny_map("demo_loss", [{"id": "ancient_guardian", "count": 3}]),
        [{"id": "levy_spearman", "count": 2}])
    scene.travel_to(Vector2i(1, 0))
    while scene.hero.moving: await process_frame
    check(await _auto_battle(scene) == "defeat", "A token army loses")
    var top: Control = scene.screens.top()
    check(top != null and not top.victory, "Defeat screen is shown when the army falls")
    check(scene._guarded(Vector2i(1, 0)), "Guards still hold the mine after a defeat")
    scene.queue_free()
    await process_frame

func _turn_done(scene: Node2D) -> void:
    await process_frame
    while scene.turn_busy: await process_frame
    await process_frame

## Tiny map with a Shariw town: the war-band rides out, steals a mine, and
## its visible moves animate; a weak hero gets ambushed on screen.
func _rival_raids() -> void:
    var tiles := []
    for q in range(-5, 6):
        for r in range(-5, 6):
            if absi(q + r) <= 5:
                tiles.append({"q": q, "r": r, "terrain": "sand", "passable": true, "moveCost": 1.0, "variant": 0})
    var map := {"version": 1, "name": "Raid", "radius": 5, "tiles": tiles, "objects": [
        {"q": -4, "r": 0, "type": "town", "name": "Camp", "factionId": 1},
        {"q": 0, "r": 0, "type": "gold_mine", "name": "Border Mine", "factionId": 1},
        {"q": 4, "r": 0, "type": "town", "name": "Ain Sharu", "factionId": 2}]}
    FileAccess.open("user://demo_raid.json", FileAccess.WRITE).store_string(JSON.stringify(map))
    var scene := await _scene("user://demo_raid.json", [{"id": "levy_spearman", "count": 1}])
    scene.dialogue.skip_all()
    var stolen := false
    for day in 4:
        scene.end_day()
        await _turn_done(scene)
        if is_instance_valid(scene.battle): break
        stolen = stolen or scene.state.owners.any(func(o): return o[0] == 0 and o[1] == 0 and o[2] == 2)
    check(stolen or is_instance_valid(scene.battle), "The war-band raids the border mine or attacks")
    for i in 4:
        if is_instance_valid(scene.battle): break
        scene.end_day()
        await _turn_done(scene)
    check(is_instance_valid(scene.battle), "A strong war-band ambushes a one-levy army")
    if is_instance_valid(scene.battle):
        check(await _auto_battle(scene) == "defeat", "The ambush is lost")
        var top: Control = scene.screens.top()
        check(top != null and not top.victory, "Losing the army to an ambush ends the expedition")
    scene.queue_free()
    await process_frame
