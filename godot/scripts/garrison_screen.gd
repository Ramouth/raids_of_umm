extends Control
## Garrison: move stacks between the hero's army and troops left at a mine
## or town (HoMM3-style). A Shariw war-band must beat the garrison to take it.

signal finished(result: Dictionary)

var host: Node   # desert.gd: provides state, unit_defs, transfer()
var cell := Vector2i.ZERO
var title := ""
var _army: VBoxContainer
var _held: VBoxContainer
var _message: Label

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme
    _panel(Rect2(140, 90, 1000, 620))
    var heading := _label("GARRISON  ·  " + title.to_upper(), 24, Color("f3dfb0"))
    heading.position = Vector2(170, 110)
    add_child(heading)
    var hint := _label("Troops left here defend it when the Shariw come. Stronger garrisons scare raiders off.", 14, Color("c8b08a"))
    hint.position = Vector2(170, 150)
    add_child(hint)
    var left := _label("YOUR ARMY", 13, Color("ad854a"))
    left.position = Vector2(170, 196)
    add_child(left)
    var right := _label("GARRISON", 13, Color("ad854a"))
    right.position = Vector2(660, 196)
    add_child(right)
    _army = VBoxContainer.new()
    _army.position = Vector2(170, 222)
    _army.custom_minimum_size = Vector2(440, 0)
    add_child(_army)
    _held = VBoxContainer.new()
    _held.position = Vector2(660, 222)
    _held.custom_minimum_size = Vector2(440, 0)
    add_child(_held)
    _message = _label("", 15, Color("f0c870"))
    _message.position = Vector2(170, 600)
    add_child(_message)
    var done := Button.new()
    done.name = "Done"
    done.text = "Done   Esc"
    done.position = Vector2(930, 640)
    done.size = Vector2(180, 44)
    done.pressed.connect(func(): finished.emit({}))
    add_child(done)
    refresh()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
        finished.emit({})
        get_viewport().set_input_as_handled()

## Moves units; to_garrison=false brings them back to the hero.
func move(unit_id: String, count: int, to_garrison: bool) -> bool:
    var reply: Dictionary = host.transfer(cell, unit_id, count, to_garrison)
    _message.text = str(reply.get("error", "")) if not reply.get("ok", false) else ""
    refresh()
    return reply.get("ok", false)

func refresh() -> void:
    for child in _army.get_children(): child.queue_free()
    for child in _held.get_children(): child.queue_free()
    for stack in host.state.get("army", []):
        _army.add_child(_row(stack, true))
    var held: Array = []
    for g in host.state.get("garrisons", []):
        if g.cell[0] == cell.x and g.cell[1] == cell.y: held = g.army
    if held.is_empty(): _held.add_child(_label("Empty. Raiders will take it freely.", 14, Color("9a8468")))
    for stack in held:
        _held.add_child(_row(stack, false))

func _row(stack: Dictionary, in_army: bool) -> Control:
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 8)
    var icon := TextureRect.new()
    var path := "res://content/textures/units/%s.png" % stack.id
    if ResourceLoader.exists(path): icon.texture = load(path)
    icon.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    icon.custom_minimum_size = Vector2(48, 48)
    icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    row.add_child(icon)
    var name := _label("%d %s" % [stack.count, host.unit_defs.get(stack.id, {}).get("name", stack.id)], 15, Color("e8d8b8"))
    name.custom_minimum_size = Vector2(210, 0)
    row.add_child(name)
    var count := int(stack.count)
    var half := Button.new()
    half.text = "Half →" if in_army else "← Half"
    half.disabled = count < 2
    half.pressed.connect(func(): move(str(stack.id), count / 2, in_army))
    row.add_child(half)
    var all := Button.new()
    all.text = "All →" if in_army else "← All"
    all.pressed.connect(func(): move(str(stack.id), count, in_army))
    row.add_child(all)
    return row

func _panel(rect: Rect2) -> void:
    var panel := Panel.new()
    panel.position = rect.position
    panel.size = rect.size
    panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
    var style := StyleBoxFlat.new()
    style.bg_color = Color("211c14")
    style.border_color = Color("665032")
    style.set_border_width_all(1)
    panel.add_theme_stylebox_override("panel", style)
    add_child(panel)

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
