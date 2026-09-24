extends SceneTree
## Renders the northern stage for a visual check. Not part of CI.
## Run (with a display): godot --path godot --position 4000,4000 --script res://tests/north_capture.gd -- <out_dir>

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
    scene.fog.visible = false
    scene.dialogue.skip_all()
    scene.fit_map()
    await _shot("north_overview")
    scene.fog.visible = true
    scene.center_hero()
    scene._zoom(1)
    await _shot("north_start")
    scene.fog.visible = false
    scene.camera.position = UmmMapData.cell_to_world(Vector2i(-4, 1))
    await _shot("north_bridge")
    scene.camera.position = UmmMapData.cell_to_world(Vector2i(1, 1))
    await _shot("north_mere")
    scene.camera.position = UmmMapData.cell_to_world(Vector2i(8, -4))
    await _shot("north_east")
    scene.fog.visible = true
    scene.center_hero()
    scene.dialogue.say([{"speaker": "Corvin", "text": "Cousin! Word reached me that Aldren left you the keys."}])
    await _shot("north_dialogue")
    scene.dialogue.skip_all()
    scene.toggle_quest_log()
    await _shot("north_questlog")
    scene.toggle_quest_log()
    scene.fog.visible = false
    scene.camera.position = UmmMapData.cell_to_world(Vector2i(-7, 3))
    scene._inspect(Vector2i(-4, 2))
    scene._float_text("+85 XP   LEVEL 2!", Color("ffe9a0"))
    await _shot("north_xp")
    quit()
