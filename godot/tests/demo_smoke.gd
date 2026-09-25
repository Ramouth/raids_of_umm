extends SceneTree
## Demo loop: guards block the pass, battles run through the screen stack,
## the passage mine wins, and losing the army ends the expedition.
## Run with --headless --script res://tests/demo_smoke.gd

const WEEK2_ARMY := [{"id": "levy_spearman", "count": 50}, {"id": "desert_archer", "count": 26},
    {"id": "armoured_warrior", "count": 10}, {"id": "rider_archer", "count": 4}]
const HOME := Vector2i(-11, 6)          # Varenhold
const RIDGE_GUARD := Vector2i(-4, 2)  # the Bridge Wardens on the Coldwater

var failures := 0
var capture := false

func _initialize() -> void:
    create_timer(120).timeout.connect(func(): push_error("Demo smoke timed out"); quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

## Waits out a walk, pressing OK on any site pop-up as a player would
## (the chest's choice stays open: the test answers it).
func _walk(scene: Node) -> void:
    while scene.hero.moving or (is_instance_valid(scene.popup) and scene.popup != scene._chest_panel):
        if is_instance_valid(scene.popup) and scene.popup != scene._chest_panel: scene.popup.choose(0)
        await process_frame
    # A fight waits for the speaker to finish; click through like a player.
    await process_frame
    if scene.state.get("encounter") != null and scene.dialogue.is_speaking():
        _heard.append_array(scene.dialogue._queue.map(func(l): return str(l.speaker)))
        _heard.append(scene.dialogue._speaker.text)
        scene.dialogue.skip_all()
        await process_frame

func _toward(scene: Node2D, target: Vector2i) -> Vector2i:
    var best: Vector2i = scene.hero.cell
    var best_d := 1 << 30
    for c: Vector2i in scene.data.tiles:
        if not scene.fog.is_explored(c) or not scene.data.is_passable(c) or scene._guarded(c): continue
        var dq := c.x - target.x
        var dr := c.y - target.y
        var d: int = max(abs(dq), abs(dr), abs(dq + dr))
        if d < best_d: best_d = d; best = c
    return best

var _heard: Array = []   # speakers clicked through before fights (upper-cased current + queued)

## Marches on a guard camp (scouting through the fog on the way), ending days
## as needed, and auto-wins every fight.
func _clear_camp(scene: Node2D, cell: Vector2i) -> void:
    for day in 10:
        if not scene._guarded(cell): return
        if not scene.travel_to(cell):             # still in the fog: walk to its edge
            scene.travel_to(_toward(scene, cell))
        await _walk(scene)
        while is_instance_valid(scene.battle):
            await _auto_battle(scene)
            await _walk(scene)
            if scene._guarded(cell): scene.travel_to(cell); await _walk(scene)
        if scene._guarded(cell):
            scene.dialogue.skip_all()
            scene.end_day()
            await _turn_done(scene)

func _capture(label: String) -> void:
    if DisplayServer.get_name() == "headless": return
    await process_frame
    await RenderingServer.frame_post_draw
    var folder := ProjectSettings.globalize_path("res://artifacts")
    DirAccess.make_dir_recursive_absolute(folder)
    root.get_texture().get_image().save_png(folder.path_join(label + ".png"))

func _run() -> void:
    capture = "--capture" in OS.get_cmdline_user_args()
    if capture and DisplayServer.get_name() != "headless":
        DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    await _root_scene()
    await _story()
    await _town_recruiting()
    await _hero_and_companions()
    await _ridge_pass()
    await _passage_wins()
    await _defeat_ends()
    await _rival_raids()
    await _save_load()
    await _sites()
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
    check(scene.dialogue._speaker.text == "STEWARD", "The steward opens level 1")
    check(scene.state.quests.any(func(q): return q.id == "wolves"), "Level 1: clear the vale's wolves")
    check(not scene.state.quests.any(func(q): return q.main), "The main quest waits for Ushari")
    check(scene.state.specials.is_empty(), "No companion at the start")
    check(scene.state.lore.size() >= 1, "The first lore entry is in the codex")
    scene.toggle_quest_log()
    check(scene.quest_log.visible, "Q opens the quest log")
    scene.toggle_quest_log()
    scene.dialogue.skip_all()
    check(not scene.dialogue.is_speaking(), "Dialogue can be dismissed")
    var druid := Vector2i(-7, -5)
    check(scene.map_view.anchors.has(druid), "The hooded druid stands by the Hermit's Wolves")
    for camp in [Vector2i(-8, -3), Vector2i(-10, -2), Vector2i(-7, -6)]:   # Hill, Den, Hermit's
        await _clear_camp(scene, camp)
        check(not scene._guarded(camp), "Wolf pack at %s is cleared" % camp)
    check("HOODED DRUID" in _heard or "Hooded Druid" in _heard, "The druid speaks before his pack fights")
    check(not scene.map_view.anchors.has(druid), "The druid walks off once his wolves fall")
    check(scene.state.quests.any(func(q): return q.main), "The main quest is given")
    check(scene.state.quests.any(func(q): return q.id == "aldren"), "The brother's quest is given")
    scene.dialogue.skip_all()
    check(scene.state.specials.size() == 1 and scene.state.specials[0].id == "ushari", "Ushari rides with the hero")
    # Clearing the vale is worth level 2: the pop-up asks for a path.
    check(int(scene.state.hero_progress.level) == 2, "Clearing the vale reaches level 2")
    check(scene.state.tree.needs_path, "Level 2 asks for a path")
    while is_instance_valid(scene.popup):
        scene.popup.choose(0)                            # "Choose a path" opens the path screen
        await process_frame
    await process_frame
    var tree: Control = scene.screens.top()
    check(tree != null and tree.has_method("choose"), "The path screen opens from the level-up pop-up")
    check(tree._columns.get_child_count() == 4, "Three paths and the wildcard are shown")
    check(not tree.choose("veined"), "The Veined is not chosen at level 2")
    check(tree.choose("marshal"), "The Marshal's path can be chosen")
    check(int(scene.state.tactics_rank) == 1, "First Orders comes with the Marshal's path")
    check(not scene.state.tree.needs_path, "The path is chosen for good")
    tree.find_child("Done", true, false).pressed.emit()
    await process_frame
    for day in 6:                                        # home to Varenhold, to leave her in charge
        if scene.hero.cell == HOME: break
        scene.travel_to(HOME)
        await _walk(scene)
        if scene.hero.cell != HOME:
            scene.dialogue.skip_all()
            scene.end_day()
            await _turn_done(scene)
    check(scene.hero.cell == HOME, "The hero rides home to Varenhold")
    check(scene.open_party(), "The companions screen opens")
    var party: Control = scene.screens.top()
    party._station("ushari", true)
    check(scene.state.specials[0].stationed != null, "Ushari can govern Varenhold")
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
    await _walk(scene)
    check(scene._offer_box.get_child_count() == 1, "The camp's offer appears in the sidebar")
    var gold := int(scene.state.treasury.Gold)
    check(scene.accept_offer("clue"), "Paying for a clue succeeds")
    check(int(scene.state.treasury.Gold) == gold - 100, "The clue costs its price")
    check(scene.dialogue.is_speaking(), "The clue is delivered as dialogue")
    scene.queue_free()
    await process_frame

## Hero (H): the paper doll helps the whole army; cursed items stay on.
## Companions (P): battle stats at their level.
func _hero_and_companions() -> void:
    var scene := await _scene("res://content/maps/old_passage.json", [{"id": "levy_spearman", "count": 24}])
    for id in ["greyfang_helm", "barrow_blade", "drowned_crown"]: scene._apply_state(JSON.parse_string(scene.adventure.add_item(id)))
    scene._apply_state(JSON.parse_string(scene.adventure.join_special("ushari")))   # she joins in level 1
    check(scene.open_hero(), "The hero screen opens")
    var sheet: Control = scene.screens.top()
    check(sheet._pack.get_child_count() == 3, "Three items wait in the backpack")
    check(sheet.put_on("greyfang_helm") and sheet.put_on("barrow_blade"), "The commander wears a helm and a blade")
    check(int(scene.state.army_bonus.defense) == 2 and int(scene.state.army_bonus.attack) == 2, "Worn items help every troop stack")
    check("+2 attack" in sheet._bonus.text, "The screen spells out the army-wide bonus")
    check(sheet.put_on("drowned_crown"), "The cursed crown goes on")
    check(not sheet.take_off("trinket1") and "cursed" in sheet._message.text, "The cursed crown will not come off")
    sheet._describe("drowned_crown")
    check("Cursed" in sheet._detail.get_parsed_text(), "The item details warn of the curse")
    if capture: await _capture("hero_screen")
    sheet.find_child("Done", true, false).pressed.emit()
    await process_frame
    check(scene.open_party(), "The companions screen opens")
    var party: Control = scene.screens.top()
    await process_frame
    var shown := false
    for label in party.find_children("*", "Label", true, false):
        if (label as Label).text.begins_with("In battle:"): shown = true
    check(shown, "Each companion card shows their battle stats")
    if capture: await _capture("companions_screen")
    party.find_child("Done", true, false).pressed.emit()
    await process_frame
    scene.queue_free()

func _town_recruiting() -> void:
    var scene := await _scene("res://content/maps/old_passage.json", [{"id": "woad_runner", "count": 24}])
    check(scene._town_button.visible, "Enter-town button shows in Varenhold")
    check(scene.open_town(), "The hero can enter Varenhold")
    var town: Control = scene.screens.top()
    check(town != null and town.has_method("recruit"), "Town screen is pushed")
    check(town._cards.get_child_count() == 5, "Varenhold offers the five Cruth tiers")
    var gold := int(scene.state.treasury.Gold)
    check(town.recruit("woad_runner", 6), "Recruiting levies succeeds")
    check(int(scene.state.treasury.Gold) == gold - 270, "Recruiting spends 45 gold per runner")
    check(scene.army.any(func(s): return s.id == "woad_runner" and s.count == 30), "Recruits merge into the existing stack")
    check(not town.recruit("teulu", 3), "Cannot afford three of the Teulu")
    check(not town.recruit("woad_runner", 999), "Cannot recruit past the weekly pool")
    # Buildings: tiers 3-5 wait for their dwellings; one building a day.
    var warriors: Control = town._cards.get_node("painted_blade")
    check(warriors.find_child("Recruit", true, false).disabled and "Blade Circle" in warriors.find_child("Recruit", true, false).text, "Painted Blades wait for the Blade Circle")
    town.show_tab("build")
    check(town._pages["build"].visible, "The Build tab opens")
    check(town._buildings.get_node("knights_hall").find_child("Build", true, false) == null, "The Knights' Hall is locked behind its requirements")
    if capture: await _capture("town_build")
    check(town.build("armoury"), "The Blade Circle is built")
    check("armoury" in town._town().buildings and town._town().built_today, "The town records today's building")
    check(not town.build("marketplace"), "A second building must wait for tomorrow")
    town.show_tab("recruit")
    check(not town._cards.get_node("painted_blade").find_child("Recruit", true, false).disabled, "Painted Blades can now be recruited")
    town.show_tab("market")
    check("→" in town._quote.text, "Hallowmere's Marketplace lets Varenhold trade too (quote shown)")
    town.find_child("Leave", true, false).pressed.emit()
    await process_frame
    check(scene.open_garrison(), "The hero can garrison Varenhold")
    var garrison: Control = scene.screens.top()
    check(garrison.move("woad_runner", 10, true), "Ten levies stay behind as a garrison")
    check(scene.army.any(func(s): return s.id == "woad_runner" and s.count == 20), "The garrisoned levies leave the army")
    check(not garrison.move("woad_runner", 20, true), "The hero cannot leave without an army")
    check(garrison.move("woad_runner", 5, false), "Troops can rejoin from the garrison")
    garrison.find_child("Done", true, false).pressed.emit()
    await process_frame
    check(scene.screens.depth() == 0 and scene.get_node("HUD").visible, "Leaving the town returns to the map")
    scene.queue_free()
    await process_frame

func _ridge_pass() -> void:
    var scene := await _scene("res://content/maps/old_passage.json", WEEK2_ARMY)
    check(scene._guarded(RIDGE_GUARD), "The Coldwater bridge starts guarded")
    check(scene._xp_at(RIDGE_GUARD) > 0, "The bridge guards show what victory is worth")
    scene.dialogue.skip_all()
    check(scene.travel_to(Vector2i(-7, 2)), "March down the road toward the Coldwater")
    await _walk(scene)
    check(scene.travel_to(RIDGE_GUARD), "Player can order an attack on the bridge guards")
    await _walk(scene)
    check(is_instance_valid(scene.battle), "Reaching the guards starts a battle")
    check(scene.screens.depth() == 1, "Battle is pushed on the screen stack")
    check(scene.battle.heading.text == "BRIDGE WARDENS", "Battle is titled after the guards")
    check(not scene.get_node("HUD").visible, "Adventure HUD is hidden under the battle")
    var result := await _auto_battle(scene)
    check(result == "victory", "A week-two army beats the bridge guards")
    check(scene.screens.depth() == 0 and scene.get_node("HUD").visible, "Battle pops back to the adventure")
    check(not scene._guarded(RIDGE_GUARD), "Beaten guards no longer hold the pass")
    check(scene.hero.cell == RIDGE_GUARD, "Hero steps into the pass after victory")
    check(not scene.map_view.anchors[RIDGE_GUARD].visible, "Guard camp disappears from the map")
    check(int(scene.state.treasury.Gold) >= 2750, "Guard reward gold is paid")
    check(scene.inventory.any(func(item): return item.id == "wolfpelt_boots"), "Guard item drop joins the inventory")
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
    await _walk(scene)
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
    await _walk(scene)
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
    if scene.state.get("encounter") != null and scene.dialogue.is_speaking():   # an ambush waits for its line
        scene.dialogue.skip_all()
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

func _save_load() -> void:
    var scene := await _scene("res://content/maps/old_passage.json", WEEK2_ARMY)
    scene.dialogue.skip_all()
    scene.travel_to(Vector2i(-8, 4))
    await _walk(scene)
    scene.end_day()
    await _turn_done(scene)
    var day: int = scene.state.day
    var cell: Vector2i = scene.hero.cell
    var gold: int = scene.state.treasury.Gold
    check(scene.save_game(), "Quicksave succeeds")
    scene.end_day()
    await _turn_done(scene)
    scene.travel_to(Vector2i(-10, 5))
    await _walk(scene)
    check(scene.load_game(), "Quickload succeeds")
    check(scene.state.day == day and scene.hero.cell == cell and int(scene.state.treasury.Gold) == gold, "Load restores day, hero and treasury")
    check(scene.army.size() == WEEK2_ARMY.size(), "Load restores the army")
    scene.queue_free()
    await process_frame

## Northern sites: a chest holds gold (no experience off the battlefield), a
## war journal teaches, an obelisk rules out a false mine, and a dwelling opens
## a one-creature recruit screen.
func _sites() -> void:
    var tiles := []
    for q in range(-3, 4):
        for r in range(-3, 4):
            if absi(q + r) <= 3:
                tiles.append({"q": q, "r": r, "terrain": "grass", "passable": true, "moveCost": 1.0, "variant": 0})
    var map := {"version": 1, "name": "Sites", "radius": 3, "ground": "grass", "tiles": tiles, "objects": [
        {"q": -3, "r": 0, "type": "town", "name": "Camp", "factionId": 1},
        {"q": -2, "r": 0, "type": "pickup", "name": "Strongbox", "kind": "chest"},
        {"q": -1, "r": 0, "type": "pickup", "name": "Field Book", "kind": "tome"},
        {"q": -1, "r": 2, "type": "dwelling", "name": "Wolf Den", "kind": "grey_wolf"},
        {"q": 0, "r": -2, "type": "obelisk", "name": "Stone"},
        {"q": 3, "r": -3, "type": "old_mine", "name": "Mine A"},
        {"q": 3, "r": 0, "type": "old_mine", "name": "Mine B"}]}
    FileAccess.open("user://demo_sites.json", FileAccess.WRITE).store_string(JSON.stringify(map))
    var scene := await _scene("user://demo_sites.json", WEEK2_ARMY)
    var gold := int(scene.state.treasury.Gold)
    var hero_xp := int(scene.state.hero_progress.xp)
    check(scene.travel_to(Vector2i(-2, 0)), "Walk to the chest")
    await _walk(scene)
    check(not is_instance_valid(scene._chest_panel), "A chest asks nothing: it holds gold")
    check(int(scene.state.treasury.Gold) == gold + 1000, "The chest's gold is taken")
    check(int(scene.state.hero_progress.xp) == hero_xp, "A chest gives no experience")
    check(not scene.map_view.anchors[Vector2i(-2, 0)].visible, "The opened chest leaves the map")
    check(scene._xp_bar.visible and scene._xp_title.text.contains("Level 1"), "The XP bar shows the commander's level")
    var xp: int = scene.state.specials[0].xp
    check(scene.travel_to(Vector2i(-1, 0)), "Walk to the war journal")
    while scene.hero.moving and not is_instance_valid(scene.popup): await process_frame
    await _walk(scene)
    await process_frame
    while scene.screens.depth() > 0:                     # level 2's path screen: decide later
        scene.screens.top().finished.emit({})
        await process_frame
    check(int(scene.state.hero_progress.xp) == hero_xp + 150, "The journal teaches the commander")
    check(int(scene.state.specials[0].xp) == xp + 150, "... and the companions")
    check(int(scene.state.hero_progress.level) == 2, "150 XP is level 2")
    check(scene._xp_title.text.contains("choose your path"), "An unchosen path is shown on the XP bar")
    check(scene.travel_to(Vector2i(0, -2)), "Walk to the obelisk")
    while scene.hero.moving and not is_instance_valid(scene.popup): await process_frame
    # HoMM3 pop-up: the walk waits on it; the map ignores clicks until OK.
    check(is_instance_valid(scene.popup) and scene.popup.title == "Stone", "Visiting the obelisk opens its pop-up")
    check(scene.popup.reward.contains("leads nowhere"), "The pop-up says what the obelisk revealed")
    check(not scene.travel_to(Vector2i(-3, 0)), "The map waits while the pop-up is open")
    if capture: await _capture("map_popup")
    scene.popup.choose(0)
    await _walk(scene)
    check(scene.state.ruled_out.size() == 1, "The obelisk rules out a false mine")
    check(scene.notice.text.contains("leads nowhere"), "The obelisk's reading is shown")
    scene.end_day()
    await _turn_done(scene)
    check(scene.travel_to(Vector2i(-1, 2)), "Walk to the wolf den")
    await _walk(scene)
    await process_frame
    var top: Control = scene.screens.top()
    check(top != null and top.has_method("recruit"), "An owned dwelling opens its recruit screen")
    if top != null and top.has_method("recruit"):
        check(top._cards.get_child_count() == 1, "The den offers only its wolves")
        check(top.recruit("grey_wolf", 2), "Wolves can be recruited")
        top.find_child("Leave", true, false).pressed.emit()
        await process_frame
    scene.queue_free()
    await process_frame
