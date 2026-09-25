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

func _run() -> void:
    var view: Control = CombatView.new()
    root.add_child(view)
    await process_frame
    view.animation_speed = 0.02
    var army := [{"id": "levy_spearman", "count": 20}, {"id": "desert_archer", "count": 10}, {"id": "armoured_warrior", "count": 4}]
    var guards := {"guards": [{"id": "grey_wolf", "count": 12}], "reward": "", "tactics": 2}
    check(view.begin(army, guards, "Tactics test"), "The battle starts")
    while view.busy: await process_frame
    check(int(view.state.opening) == 2, "Rank 2: two opening orders to give")
    check(view.begin_button.visible and not view.defend.visible, "The begin button replaces Defend")
    check("TACTICS" in view.status.text, "The status line explains the opening orders")
    for i in 20: await process_frame
    check(int(view.state.round) == 1 and view.history.size() == view.history.size(), "Nobody acts before the orders")
    var first_active: String = view.state.active
    view._toggle_order(view.units["p2"].cell)
    view._toggle_order(view.units["p0"].cell)
    view._toggle_order(view.units["p1"].cell)            # a third is refused at rank 2
    check(view._orders == ["p2", "p0"], "Clicked stacks are ordered, up to the rank")
    check(view.board.order_marks.get("p2", 0) == 1 and view.board.order_marks.get("p0", 0) == 2, "Numbered badges mark the order")
    view._toggle_order(view.units["p0"].cell)            # clicking again takes it back
    check(view._orders == ["p2"], "A second click takes a stack back")
    view._toggle_order(view.units["p0"].cell)
    check(view.give_orders(), "The orders are given")
    while view.busy: await process_frame
    check(int(view.state.opening) == 0 and not view.begin_button.visible, "The battle is under way")
    check(view.state.active == "p2", "The first chosen stack acts first (was %s)" % first_active)
    check(view.state.player_turn, "It is the player's turn")
    view.issue("defend")
    while view.busy: await process_frame
    check(view.state.active == "p0", "The second chosen stack acts next")
    print("Tactics smoke: %s" % ("PASS" if failures == 0 else "%d failure(s)" % failures))
    quit(0 if failures == 0 else 1)
