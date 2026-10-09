extends SceneTree

var failures := 0

func _initialize() -> void:
    create_timer(120).timeout.connect(func(): push_error("Story effects timed out"); quit(1))
    _run.call_deferred()

func check(ok: bool, message: String) -> void:
    if not ok:
        failures += 1
        push_error(message)

func _run() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    var day: int = scene.state.day
    scene.end_day()
    check(scene.turn_busy, "Calendar transition locks repeated turns")
    scene.end_day()
    check(int(scene.state.day) == day + 1, "Repeated End Day cannot advance twice")
    check(scene.get_node("HUD").has_node("DayTransition"), "New day has a visual announcement")
    while scene.turn_busy: await process_frame
    await process_frame
    check(not scene.get_node("HUD").has_node("DayTransition"), "Announcement cleans itself up")
    scene.state.day_of_week = 1
    scene.state.week = 2
    scene._day_transition()
    var card: ColorRect = scene.get_node("HUD/DayTransition")
    check(card.get_child(0).text.begins_with("WEEK 2"), "New week has a distinct announcement")
    await create_timer(1.1).timeout
    scene.queue_free()
    await process_frame
    var source: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://content/maps/old_passage.triggers.json"))
    var arrival: Dictionary
    for trigger in source.triggers:
        if trigger.id == "ushari_arrives":
            for action in trigger["do"]:
                if action.has("choice"): arrival = action.choice
    var conversation: Control = load("res://scripts/chapter_scene.gd").new()
    conversation.choice = arrival
    root.add_child(conversation)
    await process_frame
    check(conversation._speaker.text == "USHARI", "Ushari opens the night exchange")
    check(conversation._portrait.texture == Art.texture("portrait.ushari"), "Ushari's conversation shows Ushari")
    # Other conversations show their own speaker, or a silhouette: never Ushari.
    var ChapterSceneScript: GDScript = load("res://scripts/chapter_scene.gd")
    for case in [["Northern Warrior", Art.texture("portrait.northern_warrior")], ["Captain", ChapterSceneScript.silhouette()], ["Hooded Man", ChapterSceneScript.silhouette()]]:
        var talk: Control = ChapterSceneScript.new()
        talk.choice = {"id": "t", "title": "Test", "text": "?", "presentation": "conversation",
            "lines": [[case[0], "Words."], ["Commander", "Answer."]], "options": [{"id": "a", "label": "A"}]}
        root.add_child(talk)
        await process_frame
        check(talk._portrait.texture == case[1] and talk._portrait.texture != Art.texture("portrait.ushari"), "%s's conversation shows %s, not Ushari" % [case[0], "their portrait" if case[0] == "Northern Warrior" else "a silhouette"])
        talk._advance_conversation()
        check(talk._portrait.texture == case[1], "The speaker stays on screen (dimmed) while the commander answers")
        talk.queue_free()
    check(Art.has_art("unit.northern_warrior") and Art.texture("unit.northern_warrior") != Art.texture("unit.teulu"), "The Northern Warrior has its own (snow-washed) battle art, not the Teulu's")
    check(not conversation._buttons.get_child(1).visible, "Responses wait for her question")
    await create_timer(0.1).timeout
    check(conversation._conversation_index == 0, "Conversation never advances automatically")
    for line in arrival.lines: conversation._advance_conversation()
    check(conversation._buttons.get_child(1).visible, "Question reveals responses")
    if "--capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
        await process_frame
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("/tmp/umm_arrival.png")
    var results: Array = []
    conversation.finished.connect(func(result): results.append(result))
    conversation.select("dismiss_aldren")
    conversation.select("stand_by_aldren")
    check(results.size() == 1 and results[0].choice == "dismiss_aldren", "A response commits only once")
    conversation.queue_free()
    await process_frame
    await _mid_map_scene()
    check(_scene_checked, "The mid-map scene test ran to its end")
    await _story_order()
    check(_order_checked, "The story order test ran to its end")
    await _party_choice()
    check(_party_checked, "The party choice test ran to its end")
    print("Story effects: ", "PASS" if failures == 0 else "FAIL")
    quit(failures)

const MUD_CAMP := Vector2i(2, 0)
var _scene_checked := false

## A tiny map whose one guard camp, once beaten, plays a full-screen scene
## (the dog tag, then an id no one has drawn) and then a line from Aldren.
func _write_scene_map(dir: String) -> String:
    DirAccess.make_dir_recursive_absolute(dir)
    var tiles: Array = []
    for q in range(-3, 4):
        for r in range(maxi(-3, -q - 3), mini(3, -q + 3) + 1):
            tiles.append({"q": q, "r": r, "terrain": "grass", "passable": true, "moveCost": 1.0})
    var path := dir.path_join("mud.json")
    _write(path, {"version": 1, "name": "The Mud Field", "ground": "grass", "start": {"q": 0, "r": 0},
        "tiles": tiles, "objects": [{"q": MUD_CAMP.x, "r": MUD_CAMP.y, "type": "guard", "name": "Mud Dogs"}]})
    _write(dir.path_join("mud.encounters.json"), {"defaults": {}, "objects": {
        "Mud Dogs": {"guards": [{"id": "feral_dog", "count": 1}], "reward": {}}}})
    _write(dir.path_join("mud.triggers.json"), {"start_companions": [], "triggers": [
        {"id": "tag", "when": {"event": "encounter_won", "name": "Mud Dogs"}, "do": [
            {"scene": {"frames": [
                {"scene": "dog_tag", "text": "Among the dead dogs, a studded collar."},
                {"scene": "dog_tag", "text": "Marks are cut into it."},
                {"scene": "no_such_scene", "text": "Nobody drew this one."}]}},
            {"say": [["Aldren", "Dogs don't fight like that."]]}]}]})
    return path

func _write(path: String, value: Variant) -> void:
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_string(JSON.stringify(value))
    file.close()

func _click(at: Vector2) -> void:
    for pressed in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT
        event.pressed = pressed
        event.position = at
        event.global_position = at
        root.push_input(event, true)
    await process_frame

func _key(code: Key) -> void:
    for pressed in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code
        event.physical_keycode = code
        event.pressed = pressed
        root.push_input(event)
    await process_frame

## Beating a guard camp whose victory carries a scene: the scene fills the
## screen, turns one frame per click / key, falls back on an unknown id, and
## the map comes back as it was, with Aldren's line after the discovery.
func _mid_map_scene() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = _write_scene_map("user://scene_smoke")
    scene.passage_seed = 1
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    scene.army = [{"id": "painted_blade", "count": 30}]
    check(scene.travel_to(MUD_CAMP), "The hero marches on the Mud Dogs")
    var started := Time.get_ticks_msec()
    while not is_instance_valid(scene.battle) and Time.get_ticks_msec() - started < 10000: await process_frame
    check(is_instance_valid(scene.battle), "The Mud Dogs fight")
    if not is_instance_valid(scene.battle):
        scene.queue_free()
        return
    var battle: Control = scene.battle
    battle.animation_speed = 0.02
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
    battle.auto_button.button_pressed = true
    while battle.busy or battle.state.result == "ongoing": await process_frame
    check(battle.state.result == "victory", "The Mud Dogs are beaten")
    battle.return_button.pressed.emit()
    await process_frame
    var report: Control = scene.screens.top()
    check(report != null and report.get_script() == load("res://scripts/battle_report.gd"), "The result screen comes before the victory's scene")
    while scene.screens.top() != null and scene.screens.top().get_script() == load("res://scripts/battle_report.gd"):
        scene.screens.top().continue_button.pressed.emit()
        await process_frame
    var view: Control = scene.screens.top()
    check(view != null and view.get_script() == load("res://scripts/chapter_scene.gd"), "Victory opens the scene full-screen")
    if view == null or view.get_script() != load("res://scripts/chapter_scene.gd"):
        scene.queue_free()
        return
    var cell: Vector2i = scene.hero.cell
    var day: int = int(scene.state.day)
    check(view.get_global_rect().size == root.get_visible_rect().size, "The scene covers the whole window")
    check(not scene.get_node("HUD").visible, "The map's HUD is hidden behind the scene")
    check(view._scene == "dog_tag" and view._words.text == "Among the dead dogs, a studded collar.", "The first frame is the dog tag and its caption")
    check(view._heading.text == "MUD DOGS", "The heading names where it happens: " + view._heading.text)
    check(view._continue.text == "Continue", "Frames still to come say Continue")
    check(not scene.dialogue.is_speaking(), "Aldren waits until the discovery has been shown")
    await _click(root.get_visible_rect().size * Vector2(0.5, 0.3))
    check(view.frame == 1 and view._words.text == "Marks are cut into it.", "A click on the picture turns to the next frame")
    await _key(KEY_SPACE)
    check(view.frame == 2 and view._scene == "no_such_scene", "Space turns to the next frame")
    check(load("res://scripts/scene_stills.gd").texture("no_such_scene") == null, "An unknown scene id has no still")
    check(Art.has_art("item.dog_tag") and load("res://scripts/scene_stills.gd").item_texture("dog_tag") != null, "The dog tag is also an item picture (item.dog_tag)")
    await process_frame   # the neutral backdrop is drawn without error
    check(scene.screens.top() == view, "An unknown scene id still shows its frame (neutral backdrop)")
    check(view._continue.text == "Back to the map", "The last frame offers the way back to the map")
    view._continue.pressed.emit()
    await process_frame
    check(scene.screens.depth() == 0, "The scene closes after its last frame")
    check(scene.get_node("HUD").visible, "The map's HUD is back")
    check(scene.hero.cell == cell and int(scene.state.day) == day, "The map is as it was: same hex, same day")
    check(scene.dialogue.is_speaking(), "Aldren speaks after the scene")
    check(not scene.is_showing_scene(), "No scene is left queued")
    _scene_checked = true
    scene.queue_free()
    await process_frame

# ── Story order and the party choice ─────────────────────────────────────────

const CAIRN := Vector2i(2, 0)
const CAMP := Vector2i(-2, 0)
var _order_checked := false
var _party_checked := false

## Where captures go (`-- --capture`, with a display): build/screens/.
func _shots_dir() -> String:
    if not "--capture" in OS.get_cmdline_user_args() or DisplayServer.get_name() == "headless": return ""
    var dir := ProjectSettings.globalize_path("res://").path_join("../build/screens").simplify_path()
    DirAccess.make_dir_recursive_absolute(dir)
    return dir

func _shot(name: String) -> void:
    var dir := _shots_dir()
    if dir.is_empty(): return
    for i in 4: await process_frame
    await RenderingServer.frame_post_draw
    root.get_texture().get_image().save_png(dir.path_join(name))

## A small vale with no town: the Old Cairn (east) says a line, plays a scene,
## says another line and hands over an item, in that order; the Wayside Camp
## (west) is where Maerwen joins Drest and Aldren, one too many to ride.
func _write_story_map(dir: String) -> String:
    DirAccess.make_dir_recursive_absolute(dir)
    var tiles: Array = []
    for q in range(-3, 4):
        for r in range(maxi(-3, -q - 3), mini(3, -q + 3) + 1):
            tiles.append({"q": q, "r": r, "terrain": "grass", "passable": true, "moveCost": 1.0})
    var path := dir.path_join("vale.json")
    _write(path, {"version": 1, "name": "The Telling Vale", "ground": "grass", "start": {"q": 0, "r": 0}, "tiles": tiles, "objects": [
        {"q": CAIRN.x, "r": CAIRN.y, "type": "quest_giver", "name": "Old Cairn"},
        {"q": CAMP.x, "r": CAMP.y, "type": "quest_giver", "name": "Wayside Camp"}]})
    _write(dir.path_join("vale.triggers.json"), {"start_companions": ["drest", "aldren"], "triggers": [
        {"id": "cairn", "when": {"event": "visit", "name": "Old Cairn"}, "do": [
            {"say": [["Aldren", "Someone piled these stones with care."]]},
            {"scene": {"frames": [{"scene": "road", "text": "Under the top stone, a cloak folded small."}]}},
            {"say": [["Drest", "Then it is ours now. Take it."]]},
            {"item": "bearhide_cloak"}]},
        {"id": "camp", "when": {"event": "visit", "name": "Wayside Camp"}, "do": [
            {"say": [["Maerwen Hale", "I will ride with you, if you will have me."]]},
            {"join": "maerwen"}]}]})
    return path

func _arrive(scene: Node2D, cell: Vector2i) -> void:
    check(scene.travel_to(cell), "The hero sets off for %s" % [cell])
    var started := Time.get_ticks_msec()
    while scene.hero.moving and Time.get_ticks_msec() - started < 8000: await process_frame
    await process_frame

## "say → scene → say → item" plays in exactly that order: the scene waits for
## the first line to be heard, the second line follows the scene, the item
## notice follows the second line.
func _story_order() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = _write_story_map("user://story_order_smoke")
    scene.passage_seed = 1
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    await _arrive(scene, CAIRN)
    var seen: Array[String] = []
    var ChapterSceneScript: GDScript = load("res://scripts/chapter_scene.gd")
    for step in 12:
        await process_frame
        var top: Control = scene.screens.top()
        if top != null and top.get_script() == ChapterSceneScript:
            seen.append("scene:" + str(top._words.text))
            if seen.size() == 2: await _shot("story_order_scene.png")
            top._continue.pressed.emit()
            continue
        if scene.dialogue.is_speaking():
            var line: Dictionary = scene.dialogue.current
            seen.append("%s:%s" % [str(line.get("speaker", "")) if not str(line.get("speaker", "")).is_empty() else str(line.get("title", "")), str(line.get("text", ""))])
            check(top == null, "A line never plays under a scene")
            if seen.size() == 3: await _shot("story_order.png")
            if seen.size() == 4: await _shot("story_item_notice.png")
            await _click(scene.dialogue.get_global_rect().get_center())   # the player clicks the message on
            continue
        if not scene.is_playing_story(): break
    var expected: Array[String] = ["Aldren:Someone piled these stones with care.",
        "scene:Under the top stone, a cloak folded small.",
        "Drest:Then it is ours now. Take it.",
        "Found:You found: Bearhide Cloak. Wear it from the Hero screen (H)."]
    check(seen == expected, "The Old Cairn's story plays in order: line, scene, line, item (%s)" % [seen])
    check(not scene.is_playing_story() and not scene.dialogue.is_speaking(), "Nothing is left queued")
    check("bearhide_cloak" in scene.state.get("items", []) and "Bearhide Cloak" in scene._army_label.text, "The cloak is carried, and the sidebar names it")
    _order_checked = true
    scene.queue_free()
    await process_frame

## Maerwen joins Drest and Aldren: two may ride, so "Who stays behind?" comes
## up as a choice with the three companions' portraits; the one chosen waits
## (no town is held here), and the party screen shows them greyed, waiting,
## under the 2-companion limit.
func _party_choice() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = _write_story_map("user://party_choice_smoke")
    scene.passage_seed = 1
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    check(scene.state.get("specials", []).size() == 2 and int(scene.state.get("max_riding", 0)) == 2, "Drest and Aldren ride; two may ride")
    await _arrive(scene, CAMP)
    var ChapterSceneScript: GDScript = load("res://scripts/chapter_scene.gd")
    var choice: Control = null
    for i in 300:
        var top: Control = scene.screens.top()
        if top != null and top.get_script() == ChapterSceneScript and str(top.choice.get("id", "")) == "stay_behind":
            choice = top
            break
        if scene.dialogue.is_speaking(): scene.dialogue.advance()   # Maerwen's word first, then the question
        await process_frame
    check(choice != null, "A third companion brings up the choice of who stays behind")
    check(scene._story_blocks_exploration() and scene._end_day_button.disabled, "The choice holds the map until it is answered")
    if choice == null:
        scene.queue_free()
        return
    check(choice._heading.text == "WHO STAYS BEHIND?", "Headed with the question")
    for id in ["drest", "aldren", "maerwen"]:
        var card: Control = choice.find_child("Card_" + id, true, false)
        check(card != null and card.get_node("Portrait").texture == Art.texture("portrait.companion." + id), "%s's card shows their portrait" % id.capitalize())
    var newcomer: Control = choice.find_child("Card_maerwen", true, false)
    check(newcomer != null and "JUST JOINED" in newcomer.get_node("Name").text, "The newcomer is marked as just joined")
    await _shot("party_choice.png")
    var leave: Button = choice.find_child("Card_aldren", true, false).get_node("Choose")
    check(leave.text.begins_with("Aldren stays behind"), "Aldren's button says what it does: " + leave.text)
    leave.pressed.emit()
    for i in 10: await process_frame
    check(scene.state.get("story_choice", {}).is_empty(), "The answer closes the choice")
    check(scene.state.get("departed", []).any(func(sc): return sc.id == "aldren" and sc.waiting), "The engine has Aldren waiting")
    check(scene.dialogue.is_speaking() and str(scene.dialogue.current.get("title", "")) == "Companions"
        and str(scene.dialogue.current.get("text", "")).begins_with("Aldren stays behind"), "The party notice says Aldren stays behind")
    scene.dialogue.skip_all()
    await process_frame
    var press := InputEventKey.new()
    press.keycode = KEY_P
    press.physical_keycode = KEY_P
    press.pressed = true
    root.push_input(press)
    await process_frame
    await process_frame
    var party: Control = scene.screens.top()
    check(party != null and party.get_script() == load("res://scripts/party_screen.gd"), "P opens the companions screen")
    if party != null and party.get_script() == load("res://scripts/party_screen.gd"):
        var waiting: Control = party.find_child("Waiting_aldren", true, false)
        check(waiting != null and waiting.modulate.r < 0.9, "Aldren is listed, greyed out")
        check(waiting != null and "WAITING" in waiting.find_child("Status", true, false).text, "...and marked as waiting")
        var limits: Label = party.find_child("Limits", true, false)
        check(limits != null and limits.text.begins_with("Companions 2/2") and "Stacks %d/5" % scene.army.size() in limits.text, "The header gives the limits: " + (limits.text if limits != null else ""))
        await _shot("party_screen_limits.png")
        party.finished.emit({})
    _party_checked = true
    scene.queue_free()
    await process_frame
