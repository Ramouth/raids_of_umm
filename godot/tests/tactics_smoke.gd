extends SceneTree
## Before the battle, in a real battle screen: every battle opens with
## deployment (place your stacks, then Finish placement); Tactics opening
## orders come next, nobody moves until they are given, and the chosen stacks
## open the battle in order. Then a corridor_ambush battle: deployment on the
## 13 x 9 lane, the enemy acting first, and a tree cut down by a click.
## Run with --headless --script res://tests/tactics_smoke.gd

const CombatView = preload("res://scripts/combat.gd")
var failures := 0

func _initialize() -> void:
    create_timer(120).timeout.connect(func(): push_error("Tactics smoke timed out"); quit(1))
    _run.call_deferred()

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        push_error("FAIL: " + message)

func click_button(button: Button) -> void:
    var event := InputEventMouseButton.new()
    event.pressed = true
    event.button_index = MOUSE_BUTTON_LEFT
    event.position = button.get_global_transform_with_canvas() * (button.size / 2)
    root.push_input(event, true)
    event = event.duplicate()
    event.pressed = false
    root.push_input(event, true)

func _run() -> void:
    var view: Control = CombatView.new()
    root.add_child(view)
    await process_frame
    view.animation_speed = 0.02
    var army := [{"id": "levy_spearman", "count": 20}, {"id": "desert_archer", "count": 10}, {"id": "armoured_warrior", "count": 4}]
    var guards := {"guards": [{"id": "grey_wolf", "count": 12}], "reward": "", "tactics": 2, "seed": 5}
    check(view.begin(army, guards, "Tactics test"), "The battle starts")
    while view.busy: await process_frame
    check(view.state.deploying and view.begin_button.visible and "Finish placement" in view.begin_button.text, "Every battle opens with deployment")
    check("DEPLOYMENT" in view.status.text, "The status line explains deployment")
    check(not view._marshal_panel.visible, "The Marshal picker waits until placement is finished")
    click_button(view.begin_button)
    while view.busy: await process_frame
    check(not view.state.deploying, "Finish placement ends deployment")
    check(int(view.state.opening) == 2, "Rank 2: two opening orders to give")
    check(view.begin_button.visible and not view.defend.visible, "The begin button replaces Defend")
    check("TACTICS" in view.status.text, "The status line explains the opening orders")
    check(view.begin_button.disabled, "Begin requires a Marshal choice; skipping is separate")
    check(view.skip_orders_button.visible and view._orders_scroll.visible, "Explicit stack selection and skip controls are visible")
    check(view._marshal_panel.visible and "CHOOSE YOUR STARTER" in view._marshal_panel.get_child(1).text, "Marshal opens a dedicated starter picker")
    check("green hexes" in view._marshal_help.text, "The picker explains when to move")
    check(view.auto_button.disabled, "Auto-battle cannot silently skip Marshal orders")
    check("MARSHAL" in view.inspection.text and "green hexes" in view.inspection.text, "The panel explains both opening selection and later movement")
    check("speed" in view.order_caption.text and "ties" in view.order_caption.text, "Initiative rules are visible")
    var initial_history: int = view.history.size()
    var initial_active: String = view.state.active
    view.auto_battle = true
    view._schedule_ai()
    for i in 20: await process_frame
    check(int(view.state.round) == 1 and view.history.size() == initial_history and view.state.active == initial_active and int(view.state.opening) == 2, "Even auto-battle waits for an explicit opening decision")
    view.auto_battle = false
    var first_active: String = view.state.active
    await process_frame
    click_button(view._orders_list.get_node("p2"))
    check(not view.begin_button.disabled, "Choosing a stack enables Begin")
    check(view.initiative.get_child(0).name == "Turn_p2", "The turn bar immediately previews the Marshal override")
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    view.initiative.get_node("Turn_p0").gui_input.emit(click)
    view._toggle_order(view.units["p1"].cell)            # a third is refused at rank 2
    check(view._orders == ["p2", "p0"], "Clicked stacks are ordered, up to the rank")
    check(view.board.order_marks.get("p2", 0) == 1 and view.board.order_marks.get("p0", 0) == 2, "Numbered badges mark the order")
    view._toggle_order(view.units["p0"].cell)            # clicking again takes it back
    check(view._orders == ["p2"], "A second click takes a stack back")
    view._toggle_order(view.units["p0"].cell)
    await process_frame
    click_button(view.begin_button)
    while view.busy: await process_frame
    check(int(view.state.opening) == 0 and not view.begin_button.visible, "The battle is under way")
    check(view.state.active == "p2", "The first chosen stack acts first (was %s)" % first_active)
    check(view.state.player_turn, "It is the player's turn")
    view.issue("defend")
    while view.busy: await process_frame
    check(view.state.active == "p0", "The second chosen stack acts next")
    view.queue_free()
    await process_frame
    view = CombatView.new()
    root.add_child(view)
    await process_frame
    view.animation_speed = 0.02
    check(view.begin(army, guards, "Explicit skip"), "A second battle starts")
    while view.busy: await process_frame
    root.push_input(_key(KEY_ENTER), true)   # Enter finishes placement
    await process_frame
    while view.busy: await process_frame
    check(not view.state.deploying, "Enter finishes placement")
    var usual_first: String = view.state.active
    check(not view.give_orders() and int(view.state.opening) == 2, "Enter without a choice does not silently skip the skill")
    check(view._skip_orders(), "Skip explicitly accepts normal speed order")
    while view.busy: await process_frame
    check(int(view.state.opening) == 0 and view.state.active == usual_first, "Skipping preserves the engine's speed order")
    check(not view._marshal_panel.visible and not view._orders_scroll.visible and not view.skip_orders_button.visible, "Setup controls disappear after beginning")
    view.queue_free()
    await process_frame
    await _ambush()
    print("Tactics smoke: %s" % ("PASS" if failures == 0 else "%d failure(s)" % failures))
    quit(0 if failures == 0 else 1)

func _key(code: Key) -> InputEventKey:
    var key := InputEventKey.new()
    key.keycode = code
    key.pressed = true
    return key

func click_cell(view: Control, cell: Array) -> void:
    var event := InputEventMouseButton.new()
    event.pressed = true
    event.button_index = MOUSE_BUTTON_LEFT
    event.position = view.board.get_global_transform_with_canvas() * view.board.cell_point(cell)
    root.push_input(event, true)
    event = event.duplicate()
    event.pressed = false
    root.push_input(event, true)

## corridor_ambush: a 13 x 9 lane through trees; dogs in the top and bottom rows act first.
func _ambush() -> void:
    var view: Control = CombatView.new()
    root.add_child(view)
    await process_frame
    view.animation_speed = 0.02
    var army := [{"id": "woad_runner", "count": 10}, {"id": "cruth_slinger", "count": 4}]
    check(view.begin(army, {"guards": [{"id": "feral_dog", "count": 6}], "layout": "corridor_ambush", "seed": 3}, "Thicket Dogs"), "The ambush battle starts")
    while view.busy: await process_frame
    check(view.state.get("layout", "") == "corridor_ambush" and int(view.state.get("width", 0)) == 13 and int(view.state.get("height", 0)) == 9, "The battle uses the 13 x 9 corridor layout")
    check(view.state.deploying and view.state.get("unit_placement", false), "The ambush opens with deployment")
    check(view.state.get("enemy_first", false) and "ENEMY MOVES FIRST" in view.turn_label.text, "The player is told the enemy acts first")
    var trees: Array = view.state.get("obstacles", []).filter(func(piece): return piece.kind == "tree")
    check(trees.size() > 20, "The lane is walled with trees")
    # The whole 13 x 9 board fits its panel.
    var shown: Vector2 = view.board.size * view.board.scale
    check(shown.x <= 751 and shown.y <= 451 and view.board.position.x >= 27 and view.board.position.y >= 161, "The 13 x 9 board is scaled to fit its panel")
    check(view.board.cell_at(view.board.cell_point([12, 2])) == [12, 2], "Corner hexes of the larger board are clickable")
    # Pick up a stack with a real click, then click a free hex of the zone.
    var runner: Dictionary = {}
    for unit in view.state.units:
        if unit.player and unit.id == "woad_runner": runner = unit
    click_cell(view, runner.cell)
    await process_frame
    check(view._place_key == runner.key and view.board.place_key == runner.key, "Clicking your stack picks it up")
    var outside: Array = []
    for cell in view.board.grid_cells():
        if not cell in view.state.deploy_zone and view._obstacle_at(cell).is_empty() and view._unit_at(cell).is_empty(): outside = cell
    click_cell(view, outside)
    await process_frame
    check(not view.busy and view.units[runner.key].cell == runner.cell, "A hex outside the zone is refused")
    var spot: Array = []
    for cell in view.state.deploy_zone:
        if view._unit_at(cell).is_empty() and view._obstacle_at(cell).is_empty(): spot = cell
    check(not spot.is_empty(), "The zone has a free hex")
    view._cell_hovered(Vector2i(spot[0], spot[1]), true)
    check("Stand" in view.status.text and view.board.hover_kind == "move", "Hovering a free zone hex says the stack will stand there")
    click_cell(view, spot)
    while view.busy: await process_frame
    check(view.units[runner.key].cell == spot, "place_unit moves the stack inside the zone")
    check(view.state.deploying, "Placing a stack keeps deployment open")
    # Finish placement: the dogs act first.
    click_button(view.begin_button)
    while view.busy: await process_frame
    check(not view.state.deploying, "Finish placement starts the battle")
    check(not view.units[view.state.active].player, "An enemy stack acts first in the ambush")
    var player_waited := 0
    while view.state.result == "ongoing" and (view.busy or not view.state.player_turn) and player_waited < 2000:
        player_waited += 1
        await process_frame
    check(view.history.any(func(line): return "Foaming Dog" in line), "The dogs' first moves reach the log")
    # A tree beside the lane can be attacked; its hit points drop.
    var tree_cell: Array = []
    for turn in range(6):
        if view.state.result != "ongoing": break
        var targets: Array = view.state.get("attackable_obstacles", [])
        if not targets.is_empty():
            tree_cell = targets[0]
            break
        view.issue("defend")
        while view.state.result == "ongoing" and (view.busy or not view.state.player_turn): await process_frame
    check(not tree_cell.is_empty(), "A tree is within reach of a stack")
    if not tree_cell.is_empty():
        var before := int(view._obstacle_at(tree_cell).hp)
        view._cell_hovered(Vector2i(tree_cell[0], tree_cell[1]), true)
        check(view.board.hover_kind == "attack" and "the tree" in view.status.text, "Hovering a tree forecasts the damage")
        click_cell(view, tree_cell)
        await process_frame
        check(view.busy, "Clicking the tree orders the attack")
        while view.busy: await process_frame
        var after: int = int(view._obstacle_at(tree_cell).get("hp", 0))
        check(after < before, "The tree loses hit points (%d -> %d)" % [before, after])
        check(view.history.any(func(line): return "the tree" in line), "The log records the blow on the tree")
    view.queue_free()
    await process_frame
