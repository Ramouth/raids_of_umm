extends SceneTree
## Renders a few moments of the intro for a visual check. Not part of CI.
## Run (with a display): godot --path godot --position 4000,4000 --script res://tests/intro_capture.gd -- <out_dir>

const Intro = preload("res://scripts/intro.gd")

func _initialize() -> void:
    _run.call_deferred()

func _shot(name: String) -> void:
    await RenderingServer.frame_post_draw
    var dir: String = OS.get_cmdline_user_args()[0] if not OS.get_cmdline_user_args().is_empty() else OS.get_user_data_dir()
    root.get_texture().get_image().save_png(dir.path_join(name + ".png"))

func _run() -> void:
    DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_NO_FOCUS, true)
    root.size = Vector2i(1920, 1080)
    var intro: Control = Intro.new()
    root.add_child(intro)
    await create_timer(3.0).timeout
    await _shot("intro_1")
    intro.advance()
    intro.advance()
    await create_timer(2.5).timeout
    intro.advance()
    intro.advance()
    await create_timer(3.0).timeout
    await _shot("intro_3")
    quit()
