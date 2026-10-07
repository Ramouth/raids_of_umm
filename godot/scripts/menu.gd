extends Control
## Main and in-game menus, sharing the game's parchment and brass palette.

signal finished(result: Dictionary)

var in_game := false
var can_load := false

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var shade := ColorRect.new()
	shade.color = Color(0.025, 0.02, 0.015, 0.88 if not in_game else 0.72)
	shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(shade)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(center)
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(430, 0)
	center.add_child(panel)
	var body := VBoxContainer.new()
	body.add_theme_constant_override("separation", 14)
	panel.add_child(body)
	var title := Label.new()
	title.text = "RAIDS OF UMM'NATUR" if not in_game else "THE EXPEDITION"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 29)
	title.add_theme_color_override("font_color", Color("e6c27a"))
	body.add_child(title)
	var subtitle := Label.new()
	subtitle.text = "THE DESERT KINGDOMS" if not in_game else "A pause in the journey"
	subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	subtitle.add_theme_font_size_override("font_size", 13)
	subtitle.add_theme_color_override("font_color", Color("b99762"))
	body.add_child(subtitle)
	var gap := Control.new()
	gap.custom_minimum_size.y = 10
	body.add_child(gap)
	if in_game:
		_add_button(body, "Resume", "resume", true)
		_add_button(body, "Save game", "save")
		_add_button(body, "Load game", "load", can_load)
		_add_button(body, "Main menu", "main_menu")
		_add_button(body, "Quit", "quit")
	else:
		_add_button(body, "New expedition", "new", true)
		_add_button(body, "Load game", "load", can_load)
		_add_button(body, "Quit", "quit")

func _add_button(parent: VBoxContainer, text: String, action: String, enabled := true) -> void:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(0, 48)
	button.disabled = not enabled
	button.pressed.connect(func(): finished.emit({"action": action}))
	parent.add_child(button)
	if enabled and (action == "new" or action == "resume"):
		button.grab_focus.call_deferred()
