extends SceneTree
## Renders each town's screen for a visual check. Not part of CI.
## Run (with a display): godot --path godot --position 4000,4000 --script res://tests/town_capture.gd

const TownScreen = preload("res://scripts/town_screen.gd")

func _initialize() -> void:
    _run.call_deferred()

func _shot(name: String) -> void:
    for i in 4: await process_frame
    await RenderingServer.frame_post_draw
    var folder := ProjectSettings.globalize_path("res://artifacts")
    DirAccess.make_dir_recursive_absolute(folder)
    root.get_texture().get_image().save_png(folder.path_join(name + ".png"))

func _run() -> void:
    DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    for i in 3: await process_frame
    scene.dialogue.skip_all()
    var towns := {}
    for town in scene.state.towns:
        if int(town.owner) == 1: towns[town.name] = Vector2i(town.cell[0], town.cell[1])
    print("player towns: ", towns)
    for name in towns:
        var screen := TownScreen.new()
        screen.host = scene
        screen.cell = towns[name]
        scene.screens.push(screen, func(_r): pass)
        if name == "Varenhold": await _shot("varenhold_home")
        screen.show_tab("build")
        if name == "Varenhold": screen.build("town_hall")
        print(name, ": ", screen._town().get("buildings"), " today=", screen._town().get("built_today"), " title=", screen.get_child(1).text if screen.get_child_count() > 1 else "")
        await _shot("town_" + name.to_lower())
        screen.finished.emit({})
        for i in 2: await process_frame
    quit(0)
