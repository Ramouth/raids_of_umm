extends SceneTree
## Renders the faded margin and the campaign title card for a visual check. Not part of CI.
## Run (with a display): godot --path godot --position 4000,4000 --script res://tests/margin_capture.gd -- <out_dir>

const Fixture = preload("res://tests/campaign_fixture.gd")

func _initialize() -> void:
    _run.call_deferred()

func _shot(name: String) -> void:
    for i in 6: await process_frame
    await RenderingServer.frame_post_draw
    var dir: String = OS.get_cmdline_user_args()[0] if not OS.get_cmdline_user_args().is_empty() else OS.get_user_data_dir()
    DirAccess.make_dir_recursive_absolute(dir)
    root.get_texture().get_image().save_png(dir.path_join(name + ".png"))

func _map(path: String) -> Node2D:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    scene.get_node("Map").map_path = path
    scene.passage_seed = 1
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    return scene

func _run() -> void:
    DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    root.size = Vector2i(1600, 900)
    Fixture.write_two_maps("user://margin_capture")
    # An organic fixture map (a slanted ribbon) with its margin, fog lifted, then as played.
    var scene := await _map("user://margin_capture/b.json")
    scene.fog.visible = false
    scene.fit_map()
    await _shot("margin_ribbon_overview")
    scene.fog.visible = true
    await _shot("margin_ribbon_fogged")
    scene.queue_free()
    await process_frame
    # The old demo map: the margin around its mountain frame.
    scene = await _map("res://content/maps/old_passage.json")
    scene.fog.visible = false
    scene.fit_map()
    await _shot("margin_old_passage_overview")
    scene.camera.zoom = Vector2.ONE * 0.6
    scene.camera.position = UmmMapData.cell_to_world(Vector2i(-14, 9))
    await _shot("margin_old_passage_edge")
    scene.queue_free()
    await process_frame
    var card: Control = load("res://scripts/title_card.gd").new()
    card.chapter_title = "The Road East"
    card.map_title = "The Ribbon Road"
    card.new_chapter = true
    var layer := CanvasLayer.new()
    root.add_child(layer)
    layer.add_child(card)
    await create_timer(0.7).timeout
    await _shot("title_card")
    quit()
