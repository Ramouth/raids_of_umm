extends Control
## Browse a connected path, inspect a skill, and equip the Siegemaster's wagons.
signal finished(result: Dictionary)

const INK := Color("101923")
const PAPER := Color("ece2cc")
const MUTED := Color("a4a798")
const GOLD := Color("d5b578")
const GREEN := Color("91b6a1")
var host: Node
var _columns: HBoxContainer
var _status: Label
var _message: Label
var _frame: Control
var _graph: Control
var _detail: VBoxContainer
var _workshop: VBoxContainer
var _view_path := ""
var _selected := ""
var _nodes: Array = []

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme
    var background := ColorRect.new()
    background.color = Color("090f17")
    background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    add_child(background)
    _frame = Control.new()
    _frame.size = Vector2(1280, 800)
    add_child(_frame)
    _panel(Rect2(38, 32, 1204, 736), Color("17212b"), Color("46504f"))
    _put(_label("COMMANDER  /  PROGRESSION", 12, GOLD), Vector2(65, 53))
    _put(_label("Choose how you lead", 32, PAPER), Vector2(65, 77))
    _status = _label("", 15, MUTED)
    _put(_status, Vector2(65, 123))
    _columns = HBoxContainer.new()
    _columns.position = Vector2(65, 161)
    _columns.size = Vector2(1150, 64)
    _columns.add_theme_constant_override("separation", 12)
    _frame.add_child(_columns)
    _panel(Rect2(65, 246, 696, 431), INK, Color("384447"))
    _panel(Rect2(785, 246, 430, 431), Color("1c272e"), Color("46504f"))
    var scroll := ScrollContainer.new()
    scroll.name = "SkillScroll"
    scroll.position = Vector2(77, 257)
    scroll.size = Vector2(672, 407)
    scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
    _frame.add_child(scroll)
    _graph = Control.new()
    _graph.custom_minimum_size = Vector2(640, 900)
    _graph.draw.connect(_draw_tree)
    scroll.add_child(_graph)
    _detail = VBoxContainer.new()
    _detail.position = Vector2(807, 266)
    _detail.size = Vector2(386, 185)
    _detail.add_theme_constant_override("separation", 12)
    _frame.add_child(_detail)
    _workshop = VBoxContainer.new()
    _workshop.position = Vector2(807, 493)
    _workshop.size = Vector2(386, 165)
    _workshop.add_theme_constant_override("separation", 7)
    _frame.add_child(_workshop)
    _message = _label("", 14, GOLD)
    _message.custom_minimum_size = Vector2(900, 24)
    _put(_message, Vector2(65, 692))
    var done := Button.new()
    done.name = "Done"
    done.text = "Return to the map   Esc"
    done.position = Vector2(968, 709)
    done.size = Vector2(247, 40)
    done.pressed.connect(func(): finished.emit({}))
    _frame.add_child(done)
    _put(_label("One path. Each new level grants its next skill.", 13, MUTED), Vector2(65, 732))
    get_viewport().size_changed.connect(_resize)
    _resize()
    refresh()

func _resize() -> void:
    var viewport := get_viewport_rect().size
    _frame.scale = Vector2.ONE * minf(viewport.x / 1280.0, viewport.y / 800.0)
    _frame.position = (viewport - _frame.size * _frame.scale) / 2.0

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo and event.keycode in [KEY_ESCAPE, KEY_K]:
        finished.emit({})
        get_viewport().set_input_as_handled()

func _clear(node: Node) -> void:
    for child in node.get_children():
        node.remove_child(child)
        child.queue_free()

func refresh() -> void:
    _clear(_columns)
    _clear(_graph)
    var progress: Dictionary = host.state.get("hero_progress", {})
    var tree: Dictionary = host.state.get("tree", {})
    var chosen := str(tree.get("chosen", ""))
    var level := int(progress.get("level", 1))
    if _view_path.is_empty(): _view_path = chosen if not chosen.is_empty() else "marshal"
    _status.text = "LEVEL %d  ·  %d XP  ·  %s" % [level, int(progress.get("xp", 0)), "Choose a permanent path" if tree.get("needs_path", false) else (chosen.capitalize() if not chosen.is_empty() else "Paths open at level 2")]
    for path in tree.get("paths", []):
        var tab := Button.new()
        tab.name = str(path.id)
        tab.text = str(path.name).to_upper() + ("  ·  YOUR PATH" if path.id == chosen else "")
        tab.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        tab.custom_minimum_size = Vector2(375, 60)
        tab.add_theme_font_size_override("font_size", 17)
        tab.add_theme_stylebox_override("normal", _style(Color("2b373b") if path.id == _view_path else INK, GOLD if path.id == _view_path else Color("384447")))
        tab.pressed.connect(browse.bind(str(path.id)))
        _columns.add_child(tab)
        if path.id == _view_path: _nodes = path.nodes
    if not _nodes.any(func(node): return node.id == _selected):
        _selected = str(_nodes[0].id) if not _nodes.is_empty() else ""
    _graph.custom_minimum_size.y = _nodes.size() * 88 + 20
    for i in _nodes.size():
        var node: Dictionary = _nodes[i]
        var learned := bool(node.get("learned", false))
        var selected: bool = node.id == _selected
        var card := Button.new()
        card.name = str(node.id)
        card.position = Vector2(86, 12 + i * 88)
        card.size = Vector2(540, 72)
        card.alignment = HORIZONTAL_ALIGNMENT_LEFT
        card.text = "   %s\n   %s" % [node.name, "LEARNED" if learned else "UNLOCKS AT LEVEL %d" % int(node.level)]
        card.add_theme_font_size_override("font_size", 17)
        card.add_theme_color_override("font_color", PAPER if learned or selected else MUTED)
        card.add_theme_stylebox_override("normal", _style(Color("293a3d") if selected else Color("1b2731"), GOLD if selected else (Color("527268") if learned else Color("34424b"))))
        card.tooltip_text = str(node.text)
        card.pressed.connect(inspect.bind(str(node.id)))
        _graph.add_child(card)
    _graph.queue_redraw()
    _show_detail()
    _show_workshop()

func browse(id: String) -> void:
    _view_path = id
    _selected = ""
    refresh()

func inspect(id: String) -> void:
    _selected = id
    refresh()

func _draw_tree() -> void:
    for i in _nodes.size():
        var at := Vector2(43, 48 + i * 88)
        var learned := bool(_nodes[i].get("learned", false))
        if i + 1 < _nodes.size():
            _graph.draw_line(at, at + Vector2(0, 88), Color("58756b") if learned else Color("35434c"), 3, true)
        _graph.draw_circle(at, 20, Color("314a43") if learned else Color("202d39"))
        _graph.draw_arc(at, 20, 0, TAU, 40, GOLD if _nodes[i].id == _selected else Color("64776f"), 1.5, true)
        _graph.draw_line(at + Vector2(20, 0), at + Vector2(43, 0), Color("64776f"), 1, true)
        _graph.draw_string(ThemeDB.fallback_font, at + Vector2(-6, 6), str(int(_nodes[i].level)), HORIZONTAL_ALIGNMENT_LEFT, -1, 17, PAPER)

func _show_detail() -> void:
    _clear(_detail)
    var found := _nodes.filter(func(node): return node.id == _selected)
    if found.is_empty(): return
    var node: Dictionary = found[0]
    _detail.add_child(_label("LEVEL %d  /  %s" % [int(node.level), "LEARNED" if node.learned else "LOCKED"], 12, GREEN if node.learned else GOLD))
    _detail.add_child(_label(str(node.name), 25, PAPER))
    var words := _label(str(node.text), 16, MUTED)
    words.custom_minimum_size = Vector2(386, 90)
    _detail.add_child(words)
    var chosen := str(host.state.tree.get("chosen", ""))
    var pick := Button.new()
    pick.name = "Choose"
    pick.custom_minimum_size.y = 38
    pick.text = "Your chosen path" if chosen == _view_path else ("Choose " + _view_path.capitalize() if chosen.is_empty() else "Committed to " + chosen.capitalize())
    pick.disabled = not host.state.tree.get("needs_path", false)
    pick.pressed.connect(func(): choose(_view_path))
    _detail.add_child(pick)

func _show_workshop() -> void:
    _clear(_workshop)
    if _view_path != "siegemaster":
        _workshop.add_child(_label("ON THE FIELD", 12, GOLD))
        _workshop.add_child(_label("Order your stacks before the enemy can act." if _view_path == "marshal" else "Reach farther and keep your army supplied.", 17, PAPER))
        return
    var stock: Dictionary = host.state.get("fieldwork_stock", {})
    var capacity := int(host.state.get("fieldwork_capacity", 0))
    var owned := int(stock.get("barricade", 0)) + int(stock.get("stakes", 0))
    _workshop.add_child(_label("FIELDWORKS  ·  %d / %d WAGON SLOTS" % [owned, capacity], 12, GOLD))
    _workshop.add_child(_label("Reusable. Place before battle; right-click to reposition.", 13, MUTED))
    for kind in ["barricade", "stakes"]:
        var button := Button.new()
        button.name = "Buy_" + kind
        var gold := 250 if kind == "barricade" else 150
        var wood := 3 if kind == "barricade" else 2
        button.text = "Buy %s  ·  %d gold, %d wood  ·  %d owned" % [kind, gold, wood, int(stock.get(kind, 0))]
        button.custom_minimum_size.y = 36
        button.add_theme_font_size_override("font_size", 14)
        button.tooltip_text = "Blocks movement and all shots, including your own." if kind == "barricade" else "Blocks movement. Arrows pass over stakes. Requires level 4."
        var funds: Dictionary = host.state.get("treasury", {})
        button.disabled = capacity == 0 or owned >= capacity or (kind == "stakes" and capacity < 2) or int(funds.get("Gold", 0)) < gold or int(funds.get("Wood", 0)) < wood
        button.pressed.connect(buy.bind(kind))
        _workshop.add_child(button)

func buy(kind: String) -> bool:
    var reply: Dictionary = JSON.parse_string(host.adventure.buy_fieldwork(kind))
    if not reply.get("ok", false):
        _message.text = str(reply.get("error", "Cannot buy that fieldwork."))
        return false
    host._apply_state(reply)
    _message.text = "Packed for the next battle. Place it before the first turn."
    refresh()
    return true

func choose(id: String) -> bool:
    var reply: Dictionary = host.choose_path(id)
    if not reply.get("ok", false):
        _message.text = str(reply.get("error", "You cannot choose that path now."))
        return false
    _view_path = id
    _message.text = "Your path is chosen."
    refresh()
    return true

func _put(control: Control, at: Vector2) -> void:
    control.position = at
    if control is Label: control.size = Vector2(1120, 42)
    _frame.add_child(control)

func _panel(rect: Rect2, fill: Color, edge: Color) -> void:
    var panel := Panel.new()
    panel.position = rect.position
    panel.size = rect.size
    panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
    panel.add_theme_stylebox_override("panel", _style(fill, edge))
    _frame.add_child(panel)

func _style(fill: Color, edge: Color) -> StyleBoxFlat:
    var style := StyleBoxFlat.new()
    style.bg_color = fill
    style.border_color = edge
    style.set_border_width_all(1)
    style.set_corner_radius_all(5)
    style.content_margin_left = 12
    style.content_margin_right = 12
    return style

func _label(text: String, font_size: int, colour: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.add_theme_font_size_override("font_size", font_size)
    label.add_theme_color_override("font_color", colour)
    return label
