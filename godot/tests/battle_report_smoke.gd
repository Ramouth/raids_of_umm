extends SceneTree
## After every battle on the map: the result screen (Victory / Defeat / Retreat,
## both sides' losses, XP, the biggest hit and the damage dealt), then after a
## victory that found anything the spoils screen (gold and items, a keepsake's
## words), then the story the fight set off, then the map. Keepsakes are listed
## on the Hero screen (H) and open their text when clicked.
## Run: godot --headless --audio-driver Dummy --path godot --script res://tests/battle_report_smoke.gd
## With a display, `-- <dir>` saves result_victory.png, spoils.png and keepsakes.png there.

const DIR := "user://battle_report_smoke"
const TOWN := Vector2i(0, 0)
const GUARD := Vector2i(3, -1)        # Tag Dogs: 150 gold, the Dog Tag and a Scarab Amulet
const BattleReport = preload("res://scripts/battle_report.gd")
const ChapterScene = preload("res://scripts/chapter_scene.gd")
const EndScreen = preload("res://scripts/end_screen.gd")
const HeroScreen = preload("res://scripts/hero_screen.gd")
const KEEPSAKE_TEXT := "Beaten tin on a studded collar. Marks are cut into it, not letters you know."
var failures := 0
var shots := ""

func check(ok: bool, message: String) -> void:
    if not ok:
        failures += 1
        push_error("FAIL: " + message)

func _initialize() -> void:
    create_timer(240).timeout.connect(func(): push_error("Battle report smoke timed out"); quit(1))
    _run.call_deferred()

func _run() -> void:
    var args := OS.get_cmdline_user_args()
    if not args.is_empty() and DisplayServer.get_name() != "headless":
        shots = args[0]
        DirAccess.make_dir_recursive_absolute(shots)
    _write_fixture()
    await _victory()
    await _defeat()
    await _retreat()
    print("Battle report: ", "PASS" if failures == 0 else "FAIL (%d)" % failures)
    quit(failures)

## A small vale: your town, and the Tag Dogs three hexes east, guarding gold,
## a keepsake and a commander item; beating them plays a scene and a line.
func _write_fixture() -> void:
    DirAccess.make_dir_recursive_absolute(DIR.path_join("data"))
    for file in DirAccess.get_files_at("res://content/data"):
        if file.ends_with(".json"): DirAccess.copy_absolute("res://content/data".path_join(file), DIR.path_join("data").path_join(file))
    var items: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(DIR.path_join("data/items.json")))
    items["items"] = items["items"].filter(func(item): return item.id != "dog_tag")
    items["items"].append({"id": "dog_tag", "name": "Dog Tag", "kind": "keepsake", "text": KEEPSAKE_TEXT})
    _write(DIR.path_join("data/items.json"), items)
    var tiles: Array = []
    for q in range(-4, 5):
        for r in range(maxi(-4, -q - 4), mini(4, -q + 4) + 1):
            tiles.append({"q": q, "r": r, "terrain": "grass", "passable": true, "moveCost": 1.0})
    _write(DIR.path_join("vale.json"), {"version": 1, "name": "The Tag Vale", "ground": "grass", "tiles": tiles, "objects": [
        {"q": TOWN.x, "r": TOWN.y, "type": "town", "name": "Oakhall", "factionId": 1},
        {"q": GUARD.x, "r": GUARD.y, "type": "guard", "name": "Tag Dogs"}]})
    _write(DIR.path_join("vale.encounters.json"), {"defaults": {}, "objects": {
        "Tag Dogs": {"guards": [{"id": "feral_dog", "count": 4}], "reward": {"Gold": 150, "items": ["dog_tag"]}, "item": "scarab_amulet"}}})
    _write(DIR.path_join("vale.triggers.json"), {"triggers": [
        {"id": "tag", "when": {"event": "encounter_won", "name": "Tag Dogs"}, "do": [
            {"scene": {"frames": [{"scene": "road", "text": "Among the dead dogs, a studded collar."}]}},
            {"say": [["Scout", "Somebody fed these dogs once."]]}]}]})

func _write(path: String, value: Variant) -> void:
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_string(JSON.stringify(value, "  "))
    file.close()

func _scene(army: Array, guards: Array = []) -> Node2D:
    if not guards.is_empty():
        var table: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(DIR.path_join("vale.encounters.json")))
        table.objects["Tag Dogs"].guards = guards
        _write(DIR.path_join("vale.encounters.json"), table)
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = DIR.path_join("vale.json")
    scene.data_dir = DIR.path_join("data")
    scene.passage_seed = 1
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    scene.army = army
    return scene

## Walks to the guards as a player would and waits for the battle screen.
func _engage(scene: Node2D) -> Control:
    check(scene.travel_to(GUARD), "The hero sets off toward the Tag Dogs")
    for i in 600:
        if is_instance_valid(scene.battle): break
        if is_instance_valid(scene.popup): scene.popup.choose(0)
        if scene.dialogue.is_speaking(): scene.dialogue.skip_all()
        await process_frame
    check(is_instance_valid(scene.battle), "Reaching the Tag Dogs opens the battle")
    var battle: Control = scene.battle
    if battle != null: battle.animation_speed = 0.02
    return battle

func _begin(battle: Control) -> void:
    while battle.busy: await process_frame
    if battle._deploying():
        battle.give_orders()
        while battle.busy: await process_frame
    if battle._opening() > 0:
        for unit in battle.state.units:
            if unit.player and int(unit.count) > 0:
                battle._toggle_order(unit.cell)
                break
        battle.give_orders()
        while battle.busy: await process_frame

func _auto(battle: Control) -> void:
    await _begin(battle)
    battle.auto_button.button_pressed = true
    while battle.busy or battle.state.result == "ongoing": await process_frame

func _press_key(key: Key) -> void:
    for pressed in [true, false]:
        var event := InputEventKey.new()
        event.keycode = key
        event.physical_keycode = key
        event.pressed = pressed
        root.push_input(event)
        await process_frame

func _shot(name: String) -> void:
    if shots.is_empty(): return
    for i in 3: await process_frame
    await RenderingServer.frame_post_draw
    root.get_texture().get_image().save_png(shots.path_join(name))

func _top_is(scene: Node2D, script: Script) -> bool:
    var top: Control = scene.screens.top()
    return top != null and top.get_script() == script

func _text(node: Node) -> String:
    return node.text if node is Label else ""

func _victory() -> void:
    var scene := await _scene([{"id": "painted_blade", "count": 12}, {"id": "woad_runner", "count": 30}],
        [{"id": "feral_dog", "count": 4}])
    var xp: int = scene._xp_at(GUARD)
    var gold_before: int = int(scene.state.treasury.Gold)
    var battle := await _engage(scene)
    if battle == null:
        scene.queue_free()
        return
    await _auto(battle)
    check(battle.state.result == "victory", "The fixture army beats the Tag Dogs")
    var record: Dictionary = battle.state.get("battle", {})
    check(not record.is_empty(), "The combat snapshot carries the battle record")
    battle.return_button.pressed.emit()
    await process_frame
    check(int(scene.state.treasury.Gold) == gold_before + 150, "Victory pays the guards' 150 gold")

    # 1. The result screen comes first, over everything the fight set off.
    check(_top_is(scene, BattleReport), "Returning from the battle opens the result screen")
    var report: Control = scene.screens.top()
    if report == null or report.get_script() != BattleReport:
        scene.queue_free()
        return
    check(report.kind == "result" and _text(report.find_child("Heading", true, false)) == "Victory", "The result screen is headed Victory")
    var enemy: Control = report.find_child("EnemyLosses", true, false)
    for loss in record.losses.enemy:
        var cell: Control = enemy.find_child("Loss_" + str(loss.id), true, false)
        check(cell != null and _text(cell.get_node("Count")) == str(int(loss.count)), "Enemy losses show %d %s" % [int(loss.count), loss.id])
        check(cell != null and cell.get_node("Picture").texture != null, "Each loss shows the unit's picture")
    var mine: Control = report.find_child("PlayerLosses", true, false)
    if record.losses.player.is_empty():
        check(mine.find_child("None", true, false) != null, "No losses of your own reads None")
    else:
        check(mine.find_child("Loss_" + str(record.losses.player[0].id), true, false) != null, "Your losses are listed")
    check(xp > 0 and _text(report.find_child("Experience", true, false)) == "Experience gained:  +%d XP" % xp, "Victory shows the XP the guards were worth (%d)" % xp)
    var hit: Dictionary = record.max_hit
    check(int(hit.amount) > 0 and _text(report.find_child("MaxDamage", true, false)).begins_with("Max damage:  %d by %s" % [int(hit.amount), hit.name]), "Max damage names the biggest hit and who dealt it")
    check(_text(report.find_child("TotalDamage", true, false)) == "Damage dealt:  %d by you   ·   %d by the enemy" % [int(record.total_damage.player), int(record.total_damage.enemy)], "Total damage for each side")
    check(report.continue_button.focus_mode == Control.FOCUS_NONE, "Continue never takes focus (no double close on key release)")
    await _shot("result_victory.png")

    # 2. Enter closes the result screen once; the spoils follow.
    await _press_key(KEY_ENTER)
    check(_top_is(scene, BattleReport) and scene.screens.top().kind == "spoils", "Enter turns to the spoils screen, and only that far")
    var spoils: Control = scene.screens.top()
    if spoils == null or spoils.get_script() != BattleReport:
        scene.queue_free()
        return
    var gold: Control = spoils.find_child("Gold", true, false)
    check(gold != null and _text(gold.get_node("Amount")) == "+150 Gold", "Spoils list the 150 gold")
    var tag: Control = spoils.find_child("Item_dog_tag", true, false)
    check(tag != null and _text(tag.find_child("Name", true, false)) == "Dog Tag", "Spoils list the Dog Tag")
    check(tag != null and _text(tag.find_child("Text", true, false)) == KEEPSAKE_TEXT, "The keepsake's words are readable on the spoils screen")
    check(tag != null and tag.find_child("Picture", true, false).texture == Art.texture("item.dog_tag"), "The Dog Tag shows its picture")
    check(spoils.find_child("Item_scarab_amulet", true, false) != null, "A commander item found there is listed too")
    await _shot("spoils.png")
    spoils.continue_button.pressed.emit()
    await process_frame

    # 3. Then the scene the victory set off, then its line, then the map.
    check(_top_is(scene, ChapterScene), "The story scene plays after the spoils")
    check(not scene.dialogue.is_speaking(), "Dialogue waits for the scene")
    if _top_is(scene, ChapterScene): scene.screens.top().advance()
    await process_frame
    check(scene.screens.depth() == 0, "Back on the map after the scene")
    check(scene.dialogue.is_speaking(), "The line the victory set off plays on the map")
    check(str(scene.dialogue.current.get("text", "")) == "Somebody fed these dogs once.", "The Scout's line comes straight after the scene")
    var repeated: bool = scene.dialogue._queue.any(func(line): return "You found" in str(line.get("text", "")))
    check(not repeated, "Items the spoils screen showed are not announced again")
    scene.dialogue.skip_all()

    # 4. Keepsakes: listed apart from loot, read on the Hero screen.
    var keepsakes: Array = scene.state.get("keepsakes", [])
    check(keepsakes.size() == 1 and keepsakes[0].id == "dog_tag", "The session keeps the Dog Tag as a keepsake")
    check(not scene.state.backpack.has("dog_tag"), "A keepsake is not in the backpack")
    check(not scene.loot().any(func(item): return item.id == "dog_tag"), "A keepsake is not loot")
    check("Keepsakes: Dog Tag" in scene._army_label.text, "The sidebar names the keepsake")
    await _press_key(KEY_H)
    check(_top_is(scene, HeroScreen), "H opens the Hero screen")
    var hero_screen: Control = scene.screens.top()
    if hero_screen != null and hero_screen.get_script() == HeroScreen:
        var button: Button = hero_screen.find_child("Keepsake_dog_tag", true, false)
        check(button != null, "The Keepsakes section lists the Dog Tag")
        check(hero_screen.find_child("Keepsakes", true, false) != null, "The Hero screen has a Keepsakes section")
        check(hero_screen._pack.find_child("dog_tag", true, false) == null, "The Dog Tag cannot be worn: it is not in the backpack")
        if button != null:
            button.pressed.emit()
            await process_frame
            var detail: RichTextLabel = hero_screen.find_child("Detail", true, false)
            check(detail.get_parsed_text().contains("Dog Tag") and detail.get_parsed_text().contains(KEEPSAKE_TEXT), "Clicking the keepsake shows its name and full text")
            var picture: TextureRect = hero_screen.find_child("DetailPicture", true, false)
            check(picture.visible and picture.texture == Art.texture("item.dog_tag"), "Clicking the keepsake shows its picture")
        await _shot("keepsakes.png")
        hero_screen.finished.emit({})
    scene.queue_free()
    await process_frame

func _defeat() -> void:
    var scene := await _scene([{"id": "woad_runner", "count": 1}], [{"id": "feral_dog", "count": 40}])
    var battle := await _engage(scene)
    if battle == null:
        scene.queue_free()
        return
    await _auto(battle)
    check(battle.state.result == "defeat", "A lone runner loses to forty dogs")
    battle.return_button.pressed.emit()
    await process_frame
    var report: Control = scene.screens.top()
    check(_top_is(scene, BattleReport) and report.kind == "result", "A defeat opens the result screen first, before the lost expedition")
    if _top_is(scene, BattleReport):
        check(_text(report.find_child("Heading", true, false)) == "Defeat", "Headed Defeat")
        check(report.find_child("Experience", true, false) == null, "A defeat earns no XP line")
        check(report.find_child("Loss_woad_runner", true, false) != null, "Your losses show the fallen runner")
        await _press_key(KEY_SPACE)
        await process_frame
    check(not (_top_is(scene, BattleReport)), "No spoils after a defeat")
    check(_top_is(scene, EndScreen), "The lost expedition follows the result screen")
    scene.queue_free()
    await process_frame

func _retreat() -> void:
    var scene := await _scene([{"id": "painted_blade", "count": 6}], [{"id": "feral_dog", "count": 6}])
    var battle := await _engage(scene)
    if battle == null:
        scene.queue_free()
        return
    await _begin(battle)
    for i in 600:
        if battle.state.get("player_turn", false) and not battle.busy: break
        await process_frame
    battle.retreat.pressed.emit()
    battle.confirm_retreat.confirmed.emit()
    battle.confirm_retreat.hide()
    while battle.busy: await process_frame
    check(battle.state.result == "retreat", "The army retreats from the dogs")
    battle.return_button.pressed.emit()
    await process_frame
    var report: Control = scene.screens.top()
    check(_top_is(scene, BattleReport) and _text(report.find_child("Heading", true, false)) == "Retreat", "A retreat opens its result screen")
    if _top_is(scene, BattleReport):
        report.continue_button.pressed.emit()
        await process_frame
    check(scene.screens.depth() == 0, "No spoils after a retreat: straight back to the map")
    check(scene._guarded(GUARD), "The dogs still hold their camp")
    scene.queue_free()
    await process_frame
