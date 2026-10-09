extends SceneTree
## Screenshots of Drest, Aldren and the commander (the hero) in conversation, for the
## playtester (needs a display); fails if any of their art is missing.
## Run: godot --audio-driver Dummy --path godot --script res://tests/drest_capture.gd -- <out dir>
const ChapterScene = preload("res://scripts/chapter_scene.gd")

func _initialize() -> void:
    create_timer(60).timeout.connect(func(): push_error("Drest capture timed out"); quit(1))
    _run.call_deferred()

func _run() -> void:
    var args := OS.get_cmdline_user_args()
    var dir: String = args[0] if not args.is_empty() and not args[0].begins_with("--") else ProjectSettings.globalize_path("res://artifacts/verify")
    DirAccess.make_dir_recursive_absolute(dir)
    if DisplayServer.get_name() != "headless":
        DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    root.size = Vector2i(1280, 720)
    var desert: Node = load("res://scenes/desert.tscn").instantiate()
    var theme: Theme = desert.get_node("HUD/Layout").theme   # the map's own UI theme
    desert.free()
    var failures := 0
    for shot in [["conversation_drest.png", "Drest", "Stand up straight, boy. Dogs smell fear before they smell blood."],
            ["conversation_aldren.png", "Aldren", "Keep up. The pack won't wait for us, and neither will I."]]:
        var view: Control = ChapterScene.new()
        view.theme = theme
        view.choice = {"id": "capture", "title": "Before the Hunt", "text": "What do you say?", "presentation": "conversation",
            "lines": [[shot[1], shot[2]], ["Commander", "I'm ready."]], "options": [{"id": "a", "label": "Ride on"}]}
        root.add_child(view)
        await create_timer(0.4).timeout
        var expected := Art.texture("portrait." + str(shot[1]).to_lower())
        if view._portrait.texture != expected or not Art.has_art("portrait." + str(shot[1]).to_lower()):
            failures += 1
            push_error("FAIL: %s's conversation does not show portrait.%s" % [shot[1], str(shot[1]).to_lower()])
        await RenderingServer.frame_post_draw
        var path: String = dir.path_join(shot[0])
        root.get_texture().get_image().save_png(path)
        print("Saved ", path)
        if shot[1] == "Drest":   # the commander answers: the hero's own portrait lights up
            view._advance_conversation()
            await create_timer(0.2).timeout
            var face: TextureRect = view._commander.get_node("Face")
            if face.texture != Art.texture("hero.portrait") or not Art.has_art("hero.portrait"):
                failures += 1
                push_error("FAIL: the commander's side does not show hero.portrait")
            await RenderingServer.frame_post_draw
            root.get_texture().get_image().save_png(dir.path_join("conversation_commander.png"))
            print("Saved ", dir.path_join("conversation_commander.png"))
        view.queue_free()
        await process_frame
    for id in ["unit.drest", "unit.aldren", "portrait.drest", "portrait.aldren", "hero.portrait", "portrait.commander"]:
        if not Art.has_art(id):
            failures += 1
            push_error("FAIL: no art for " + id)
    quit(failures)
