extends SceneTree
## Purchases use the real adventure bridge; placement uses the actual combat UI.
const CombatView = preload("res://scripts/combat.gd")
const TreeView = preload("res://scripts/tree_screen.gd")
const Intro = preload("res://scripts/intro.gd")
var failures := 0
var capture := false

class TreeHost extends Node:
    var adventure: RefCounted
    var state: Dictionary
    func _apply_state(reply: Dictionary) -> void: state = reply
    func choose_path(id: String) -> Dictionary:
        var reply: Dictionary = JSON.parse_string(adventure.choose_path(id))
        if reply.get("ok", false): state = reply
        return reply

func _initialize() -> void:
    create_timer(90).timeout.connect(func(): push_error("Fieldworks smoke timed out"); quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func _shot(name: String) -> void:
    if not capture or DisplayServer.get_name() == "headless": return
    await process_frame
    await RenderingServer.frame_post_draw
    var folder := ProjectSettings.globalize_path("res://artifacts")
    DirAccess.make_dir_recursive_absolute(folder)
    root.get_texture().get_image().save_png(folder.path_join(name + ".png"))

func _run() -> void:
    capture = "--capture" in OS.get_cmdline_user_args()
    if capture: DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    # A real session at level 4, with the standard starting purse.
    var map := {"version": 1, "name": "Workshop test", "radius": 1, "tiles": [], "objects": [{"q": 0, "r": 0, "type": "town", "name": "Home", "factionId": 1}]}
    for q in range(-1, 2):
        for r in range(-1, 2):
            if abs(q + r) <= 1: map.tiles.append({"q": q, "r": r, "terrain": "grass", "passable": true, "moveCost": 1.0})
    FileAccess.open("user://workshop.json", FileAccess.WRITE).store_string(JSON.stringify(map))
    FileAccess.open("user://workshop.triggers.json", FileAccess.WRITE).store_string(JSON.stringify({"start_companions": [], "triggers": [{"id": "level", "when": {"event": "start"}, "do": [{"xp": 450}]}]}))
    var host := TreeHost.new()
    var hud := Node.new()
    hud.name = "HUD"
    host.add_child(hud)
    var layout := Control.new()
    layout.name = "Layout"
    hud.add_child(layout)
    root.add_child(host)
    host.adventure = ClassDB.instantiate("UmmAdventure")
    host.state = JSON.parse_string(host.adventure.start(ProjectSettings.globalize_path("user://workshop.json"), ProjectSettings.globalize_path("res://content/data"), "", 1, ProjectSettings.globalize_path("user://workshop.triggers.json")))
    check(host.state.ok, "Workshop session loads")
    var tree: Control = TreeView.new()
    tree.host = host
    root.add_child(tree)
    await process_frame
    check(tree.choose("siegemaster"), "Siegemaster can be chosen")
    check(tree._columns.get_child_count() == 3, "Three path tabs remain after refresh")
    check(tree.buy("barricade"), "Buy a barricade through the tree UI")
    check(tree.buy("stakes"), "Level 4 unlocks stakes")
    check(not tree.buy("barricade"), "Cannot overfill the wagons")
    check(int(host.state.fieldwork_stock.barricade) == 1 and int(host.state.fieldwork_stock.stakes) == 1, "Purchases update native stock")
    tree.inspect("earthworks")
    await _shot("siegemaster_tree")
    # Scaling leaves the whole tree inside the viewport.
    root.size = Vector2i(1024, 640)
    await process_frame
    check(tree._frame.position.x >= -1 and tree._frame.position.y >= -1, "Tree fits a smaller window")
    await _shot("siegemaster_tree_small")
    root.size = Vector2i(1280, 800)
    tree.queue_free()
    await process_frame
    var view: Control = CombatView.new()
    root.add_child(view)
    view.animation_speed = 0.02
    check(view.begin([{"id": "desert_archer", "count": 20}], {"guards": [{"id": "skeleton_warrior", "count": 4}], "fieldworks": host.state.fieldwork_stock}, "Fieldwork deployment"), "Combat receives the purchased stock")
    while view.busy: await process_frame
    check(view.state.deploying and view.begin_button.visible, "Fieldworks are placed before combat")
    for i in 15: await process_frame
    check(view.state.round == 1 and view.state.fieldworks.is_empty(), "No enemy acts during preparation")
    view._cell_clicked(Vector2i(2, 1))
    while view.busy: await process_frame
    check(view.state.fieldworks.size() == 1, "A click places the selected barricade")
    view._fieldwork_kind = "stakes"
    view._cell_clicked(Vector2i(3, -1))
    while view.busy: await process_frame
    check(view.state.fieldworks.size() == 2, "A second type can be placed")
    await _shot("fieldworks_deployment")
    view._cell_right_clicked(Vector2i(3, -1))
    while view.busy: await process_frame
    check(view.state.fieldworks.size() == 1, "Right click recovers a piece")
    check(view.give_orders(), "Placement can be completed with unused equipment")
    while view.busy: await process_frame
    check(not view.state.deploying and not view.begin_button.visible, "Battle starts after placement")
    check(not view.issue("place_stakes", Vector2i(3, -1)), "Cannot construct after battle starts")
    check(not [2, 1] in view.state.reachable, "Barricade is excluded from movement highlights")
    check(view.state.attackable.is_empty(), "Barricade removes the shot from attack highlights")
    view.auto_button.button_pressed = true
    while view.busy or view.state.result == "ongoing": await process_frame
    check(view.state.result == "victory", "AI can finish the battle around an obstacle")
    view.queue_free()
    host.queue_free()
    await process_frame
    var intro: Control = Intro.new()
    root.add_child(intro)
    await process_frame
    intro.advance()
    intro.advance()
    await create_timer(0.1).timeout
    check(intro._front.scale == Vector2.ONE and intro._back.scale == Vector2.ONE, "Intro framing is static during crossfades")
    while not intro._title_active and not intro._done:
        intro.advance()
    check(intro._title_active and intro.find_children("Title", "Label", false, false).size() == 1, "Rapid advances create exactly one title")
    intro.advance()
    check(intro._done, "Advancing the title finishes safely")
    intro.queue_free()
    print("Fieldworks and intro smoke: ", "PASS" if failures == 0 else str(failures) + " failures")
    quit(failures)
