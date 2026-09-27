extends SceneTree
## Tactics in a real battle screen: opening orders come first, nobody moves
## until they are given, and the chosen stacks open the battle in order.
## Run with --headless --script res://tests/tactics_smoke.gd

const CombatView = preload("res://scripts/combat.gd")
var failures := 0

func _initialize() -> void:
    create_timer(60).timeout.connect(func(): push_error("Tactics smoke timed out"); quit(1))
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
    var usual_first: String = view.state.active
    check(not view.give_orders() and int(view.state.opening) == 2, "Enter without a choice does not silently skip the skill")
    check(view._skip_orders(), "Skip explicitly accepts normal speed order")
    while view.busy: await process_frame
    check(int(view.state.opening) == 0 and view.state.active == usual_first, "Skipping preserves the engine's speed order")
    check(not view._marshal_panel.visible and not view._orders_scroll.visible and not view.skip_orders_button.visible, "Setup controls disappear after beginning")
    print("Tactics smoke: %s" % ("PASS" if failures == 0 else "%d failure(s)" % failures))
    quit(0 if failures == 0 else 1)
