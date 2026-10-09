extends SceneTree
## Screenshots of mid-map story scenes for the playtester (needs a display):
## the dog tag still with its caption, and the neutral backdrop an unknown id gets.
## Run: godot --audio-driver Dummy --path godot --script res://tests/scene_capture.gd -- <out dir> [--export-art]
## --export-art also rewrites godot/themes/stills/*.png from scripts/scene_stills.gd.
const ChapterScene = preload("res://scripts/chapter_scene.gd")

func _initialize() -> void:
    create_timer(60).timeout.connect(func(): push_error("Scene capture timed out"); quit(1))
    _run.call_deferred()

func _run() -> void:
    var args := OS.get_cmdline_user_args()
    var dir: String = args[0] if not args.is_empty() and not args[0].begins_with("--") else ProjectSettings.globalize_path("res://artifacts/verify")
    DirAccess.make_dir_recursive_absolute(dir)
    root.size = Vector2i(1280, 720)
    var shots := [
        ["dog_tag.png", [{"scene": "dog_tag", "text": "Among the dead dogs, a studded collar.\nOn it, a tag of beaten tin."}]],
        ["scene_unknown.png", [{"scene": "no_such_scene", "text": "A scene nobody has drawn yet falls back to this backdrop."}]],
    ]
    var desert: Node = load("res://scenes/desert.tscn").instantiate()
    var theme: Theme = desert.get_node("HUD/Layout").theme   # the map's own UI theme
    desert.free()
    for shot in shots:
        var view: Control = ChapterScene.new()
        view.theme = theme
        view.outcome = {"frames": shot[1]}
        view.heading = "THICKET DOGS"
        view.close_label = "Back to the map"
        root.add_child(view)
        await create_timer(0.8).timeout   # the caption fades in
        await RenderingServer.frame_post_draw
        var path: String = dir.path_join(shot[0])
        root.get_texture().get_image().save_png(path)
        print("Saved ", path)
        view.queue_free()
        await process_frame
    if "--export-art" in args:   # refresh the PNGs art.json names (scene.dog_tag, item.dog_tag)
        var stills: GDScript = load("res://scripts/scene_stills.gd")
        DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://themes/stills"))
        stills.texture("dog_tag").get_image().save_png(ProjectSettings.globalize_path("res://themes/stills/dog_tag.png"))
        stills.item_texture("dog_tag").get_image().save_png(ProjectSettings.globalize_path("res://themes/stills/item_dog_tag.png"))
        print("Exported res://themes/stills/dog_tag.png and item_dog_tag.png")
    quit(0)
