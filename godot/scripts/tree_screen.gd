extends Control
## The commander's tree (K): four branches side by side, learned top to
## bottom with the points each level brings. Tactics is live in the demo;
## the other branches show what is coming.

signal finished(result: Dictionary)

var host: Node   # desert.gd: provides state, learn()
var _columns: HBoxContainer
var _points: Label
var _message: Label

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme
    var panel := Panel.new()
    panel.position = Vector2(90, 60)
    panel.size = Vector2(1100, 680)
    var style := StyleBoxFlat.new()
    style.bg_color = Color("211c14")
    style.border_color = Color("665032")
    style.set_border_width_all(1)
    panel.add_theme_stylebox_override("panel", style)
    add_child(panel)
    var heading := _label("THE COMMANDER'S TREE", 24, Color("f3dfb0"))
    heading.position = Vector2(120, 80)
    add_child(heading)
    _points = _label("", 16, Color("f7d580"))
    _points.position = Vector2(120, 118)
    add_child(_points)
    _columns = HBoxContainer.new()
    _columns.position = Vector2(120, 156)
    _columns.add_theme_constant_override("separation", 16)
    add_child(_columns)
    _message = _label("", 15, Color("f0c870"))
    _message.position = Vector2(120, 668)
    add_child(_message)
    var done := Button.new()
    done.name = "Done"
    done.text = "Done   Esc"
    done.position = Vector2(990, 680)
    done.size = Vector2(170, 44)
    done.pressed.connect(func(): finished.emit({}))
    add_child(done)
    refresh()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and event.keycode in [KEY_ESCAPE, KEY_K]:
        finished.emit({})
        get_viewport().set_input_as_handled()

func refresh() -> void:
    for child in _columns.get_children(): child.queue_free()
    var progress: Dictionary = host.state.get("hero_progress", {})
    var points := int(progress.get("points", 0))
    _points.text = "Level %d  ·  %d point%s to spend" % [int(progress.get("level", 1)), points, "" if points == 1 else "s"]
    for branch in host.state.get("tree", []):
        _columns.add_child(_branch(branch))

func _branch(branch: Dictionary) -> Control:
    var column := VBoxContainer.new()
    column.custom_minimum_size = Vector2(248, 0)
    column.add_theme_constant_override("separation", 8)
    var live: bool = branch.live
    column.add_child(_label(str(branch.name).to_upper(), 18, Color("f3dfb0") if live else Color("8f7c62")))
    var about := _label(str(branch.text) + ("" if live else "\n(coming later)"), 13, Color("c8b08a") if live else Color("7d6c55"))
    about.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    about.custom_minimum_size = Vector2(248, 64)
    column.add_child(about)
    for node in branch.nodes:
        column.add_child(_node(node, live))
    return column

func _node(node: Dictionary, live: bool) -> Control:
    var button := Button.new()
    button.name = str(node.id)
    button.custom_minimum_size = Vector2(248, 92)
    button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    button.alignment = HORIZONTAL_ALIGNMENT_LEFT
    var mark := "✓ " if node.learned else ""
    button.text = "%s%s\n%s" % [mark, node.name, node.text]
    button.tooltip_text = "Learned." if node.learned else str(node.blocker) if not str(node.blocker).is_empty() else "Click to learn (1 point)."
    button.disabled = not str(node.blocker).is_empty()
    if node.learned: button.modulate = Color("b8e0b0")
    elif not live: button.modulate = Color(1, 1, 1, 0.45)
    button.pressed.connect(func(): learn(str(node.id)))
    return button

## Spends a point on `id`; returns false with the reason shown if it cannot.
func learn(id: String) -> bool:
    var reply: Dictionary = host.learn(id)
    if not reply.get("ok", false):
        _message.text = str(reply.get("error", "Cannot learn that yet."))
        return false
    _message.text = "Learned."
    refresh()
    return true

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
