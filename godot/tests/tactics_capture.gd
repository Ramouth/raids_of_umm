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
    root.size = Vector2i(1280, 800)
    var view: Control = CombatView.new()
    root.add_child(view)
    await process_frame
    view.begin([{"id": "levy_spearman", "count": 20}, {"id": "desert_archer", "count": 10}, {"id": "armoured_warrior", "count": 4},
                {"id": "maerwen", "count": 1, "level": 2, "companion": true}],
               {"guards": [{"id": "grey_wolf", "count": 16}, {"id": "brigand", "count": 8}], "reward": "", "tactics": 2}, "Hermit's Wolves")
    while view.busy: await process_frame
    view._toggle_order(view.units["p2"].cell)
    view._toggle_order(view.units["p0"].cell)
    await _shot("tactics_orders")
    view.queue_free()
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    scene.open_tree()
    await _shot("tactics_tree")
    quit()
