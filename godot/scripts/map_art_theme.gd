extends RefCounted
## A biome package owns scenery, landmark overrides, sizing and UI colours together.
var config: Dictionary = {}

func load_for(ground: String) -> void:
    var id := "north" if ground == "grass" else "desert"
    config = JSON.parse_string(FileAccess.get_file_as_string("res://themes/%s.json" % id))

func colour(key: String) -> Color:
    return Color(str(config.palette[key]))

func apply_ui(layout: Control) -> void:
    var theme: Theme = layout.theme.duplicate(true)
    var panel: StyleBoxFlat = theme.get_stylebox("panel", "PanelContainer").duplicate()
    panel.bg_color = colour("panel")
    panel.border_color = colour("frame")
    theme.set_stylebox("panel", "PanelContainer", panel)
    var button: StyleBoxFlat = theme.get_stylebox("normal", "Button").duplicate()
    button.bg_color = colour("button")
    button.border_color = colour("frame")
    theme.set_stylebox("normal", "Button", button)
    theme.set_color("font_color", "Label", colour("text"))
    theme.set_color("font_color", "Button", colour("text"))
    layout.theme = theme
