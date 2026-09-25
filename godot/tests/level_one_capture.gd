extends SceneTree
## Renders level 1 for a visual check: the hooded druid with his wolves and his
## transmission. Not part of CI.
## Run (with a display): godot --path godot --position 4000,4000 --script res://tests/level_one_capture.gd -- <out_dir>

func _initialize() -> void:
    _run.call_deferred()

func _shot(name: String) -> void:
    for i in 4: await process_frame
    var dir: String = OS.get_cmdline_user_args()[0] if not OS.get_cmdline_user_args().is_empty() else OS.get_user_data_dir()
    root.get_texture().get_image().save_png(dir.path_join(name + ".png"))

func _run() -> void:
    DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    root.size = Vector2i(1600, 900)
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.passage_seed = 1
    root.add_child(scene)
    await process_frame
    await _shot("level1_intro")
    scene.dialogue.skip_all()
    scene.fog.visible = false
    scene._zoom(1)
    scene.camera.position = UmmMapData.cell_to_world(Vector2i(-8, -4))
    scene.dialogue.say([{"speaker": "Hooded Druid", "text": "My forefathers did not grow the roots across the old passage to keep your kind out. They grew them thick, and deep, to keep something in."}])
    await _shot("level1_druid")
    quit()
