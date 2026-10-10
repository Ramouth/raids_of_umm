extends SceneTree
## Visual check of the commander's tree and the Tactics opening orders. Not part of CI.
## Run (with a display): godot --path godot --position 4000,4000 --script res://tests/tactics_capture.gd -- <out_dir>

const CombatView = preload("res://scripts/combat.gd")

func _initialize() -> void:
    _run.call_deferred()

func _shot(name: String) -> void:
    for i in 6: await process_frame
    var dir: String = OS.get_cmdline_user_args()[0] if not OS.get_cmdline_user_args().is_empty() else OS.get_user_data_dir()
    root.get_texture().get_image().save_png(dir.path_join(name + ".png"))

func _run() -> void:
    DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    root.size = Vector2i(1920, 1080)
    var view: Control = CombatView.new()
    root.add_child(view)
    await process_frame
    view.begin([{"id": "woad_runner", "count": 20}, {"id": "cruth_slinger", "count": 10}, {"id": "painted_blade", "count": 4}, {"id": "gododdin_rider", "count": 3},
                {"id": "maerwen", "count": 1, "level": 2, "companion": true}],
               {"guards": [{"id": "grey_wolf", "count": 16}, {"id": "brigand", "count": 8}], "reward": "", "tactics": 2}, "Hermit's Wolves")
    while view.busy: await process_frame
    view._set_place_key("p0")   # a stack picked up: the green zone is where it may stand
    await _shot("deploy_field")
    view._set_place_key("")
    view.give_orders()          # Finish placement, then the Marshal's orders
    while view.busy: await process_frame
    view._toggle_order(view.units["p2"].cell)
    view._toggle_order(view.units["p0"].cell)
    await _shot("tactics_orders")
    view.queue_free()
    # The ambush lane (13 x 9): deployment, then a tree hovered for a blow.
    view = CombatView.new()
    root.add_child(view)
    await process_frame
    view.animation_speed = 0.02
    view.begin([{"id": "woad_runner", "count": 10}, {"id": "cruth_slinger", "count": 4}, {"id": "drest", "count": 1, "level": 1, "companion": true}],
               {"guards": [{"id": "feral_dog", "count": 6}], "layout": "corridor_ambush", "seed": 3}, "Thicket Dogs")
    while view.busy: await process_frame
    await _shot("ambush_deploy")
    view.give_orders()
    while view.state.result == "ongoing" and (view.busy or not view.state.player_turn): await process_frame
    var targets: Array = view.state.get("attackable_obstacles", [])
    if not targets.is_empty():
        view._cell_hovered(Vector2i(targets[0][0], targets[0][1]), true)
        view.issue("attack", Vector2i(targets[0][0], targets[0][1]))
        while view.state.result == "ongoing" and (view.busy or not view.state.player_turn): await process_frame
        targets = view.state.get("attackable_obstacles", [])
        if not targets.is_empty(): view._cell_hovered(Vector2i(targets[0][0], targets[0][1]), true)
    await _shot("ambush_tree")
    view.queue_free()
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    # Staged: level 2 reached, no path yet (the real flow is covered by demo_smoke).
    scene.state.hero_progress.level = 2
    scene.state.tree.needs_path = true
    scene._announce_levels({"level_ups": [{"level": 2, "skill": "", "text": ""}]})
    await _shot("levelup_popup")
    scene.popup.queue_free()
    scene.popup = null
    scene.open_tree()
    await _shot("tactics_tree")
    scene.screens.top().finished.emit({})
    scene.state.tree.needs_path = false
    scene.state.tree.chosen = "marshal"
    scene._announce_levels({"level_ups": [{"level": 3, "skill": "Rally", "text": "+1 attack for every troop stack."}]})
    await _shot("levelup_skill")
    quit()
