extends Control
## The commander's path (K). At level 2 the commander chooses one of the
## faction's three paths for good; each later level grants that path's skill
## for the level. The Veined is the wildcard the story may offer later.

signal finished(result: Dictionary)

var host: Node   # desert.gd: provides state, choose_path()
var _columns: HBoxContainer
var _status: Label
var _message: Label

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme
    var panel := Panel.new()
    panel.position = Vector2(60, 40)
    panel.size = Vector2(1160, 720)
    var style := StyleBoxFlat.new()
    style.bg_color = Color("211c14")
    style.border_color = Color("665032")
    style.set_border_width_all(1)
    panel.add_theme_stylebox_override("panel", style)
    add_child(panel)
    var heading := _label("THE COMMANDER'S PATH", 24, Color("f3dfb0"))
    heading.position = Vector2(90, 56)
    add_child(heading)
    _status = _label("", 16, Color("f7d580"))
    _status.position = Vector2(90, 92)
    add_child(_status)
    _columns = HBoxContainer.new()
    _columns.position = Vector2(90, 126)
    _columns.add_theme_constant_override("separation", 14)
    add_child(_columns)
    _message = _label("", 15, Color("f0c870"))
    _message.position = Vector2(90, 712)
    add_child(_message)
    var done := Button.new()
    done.name = "Done"
    done.text = "Done   Esc"
    done.position = Vector2(1030, 704)
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
    var tree: Dictionary = host.state.get("tree", {})
    var level := int(progress.get("level", 1))
    var chosen := str(tree.get("chosen", ""))
    if tree.get("needs_path", false): _status.text = "Level %d  ·  Choose your path. The choice is for good." % level
    elif chosen.is_empty(): _status.text = "Level %d of %d  ·  Your path opens at level 2." % [level, int(progress.get("max", 10))]
    else: _status.text = "Level %d of %d  ·  Each level brings the next skill of your path." % [level, int(progress.get("max", 10))]
    for path in tree.get("paths", []):
        _columns.add_child(_path_column(path, chosen, tree.get("needs_path", false), level))
    if not tree.get("wildcard", {}).get("nodes", []).is_empty():
        _columns.add_child(_path_column(tree.wildcard, chosen, false, level, true))

func _path_column(path: Dictionary, chosen: String, choosing: bool, level: int, wildcard := false) -> Control:
    var column := VBoxContainer.new()
    column.name = str(path.id)
    column.custom_minimum_size = Vector2(265, 0)
    column.add_theme_constant_override("separation", 4)
    var mine: bool = path.id == chosen
    var faded := wildcard or (not chosen.is_empty() and not mine)
    var title := str(path.name).to_upper() + ("   ✓" if mine else "")
    column.add_child(_label(title, 18, Color("7d6c55") if faded else Color("f3dfb0")))
    var about := _label(str(path.text) + ("\n(offered by the story)" if wildcard else ""), 13, Color("7d6c55") if faded else Color("c8b08a"))
    about.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    about.custom_minimum_size = Vector2(265, 56)
    column.add_child(about)
    if choosing and not wildcard:
        var pick := Button.new()
        pick.name = "Choose"
        pick.text = "Choose the %s" % path.name
        pick.pressed.connect(func(): choose(str(path.id)))
        column.add_child(pick)
    for node in path.nodes:
        column.add_child(_skill(node, faded, level))
    return column

func _skill(node: Dictionary, faded: bool, level: int) -> Control:
    var line := _label("", 13, Color("c8b08a"))
    line.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    line.custom_minimum_size = Vector2(265, 0)
    var at := ("L%d  " % int(node.level)) if int(node.get("level", 0)) > 0 else ""
    var mark := "✓ " if node.learned else ""
    var later := "" if node.get("live", false) or node.get("level", 0) == 0 else "  (later)"
    line.text = "%s%s%s: %s%s" % [mark, at, node.name, node.text, later]
    if node.learned: line.add_theme_color_override("font_color", Color("b8e0b0"))
    elif faded or not node.get("live", false): line.add_theme_color_override("font_color", Color("6f604c"))
    elif int(node.level) > level: line.add_theme_color_override("font_color", Color("a8977a"))
    return line

## Chooses `id` as the commander's path; returns false with the reason shown.
func choose(id: String) -> bool:
    var reply: Dictionary = host.choose_path(id)
    if not reply.get("ok", false):
        _message.text = str(reply.get("error", "You cannot choose that path now."))
        return false
    _message.text = "Your path is chosen."
    refresh()
    return true

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
