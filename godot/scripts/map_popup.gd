extends Control
## HoMM3-style adventure pop-up: a small framed box with the site's picture,
## its name, a line of flavour, what you gained, and OK (or a choice of
## buttons). Covers the screen so the map cannot be clicked while it is open.
## Enter / Space picks the first button; Esc closes a box that has only OK.

signal closed(choice: int)

const FRAME := Color("a07a44")
const TEXT := Color("e8d8b8")
const GOLD := Color("f7d580")

var title := ""
var flavour := ""
var reward := ""
var picture: Texture2D
var buttons: Array[String] = ["OK"]
var _box: PanelContainer

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    mouse_filter = Control.MOUSE_FILTER_STOP
    var dim := ColorRect.new()
    dim.color = Color(0, 0, 0, 0.35)
    dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    dim.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(dim)
    _box = PanelContainer.new()
    _box.name = "Box"
    var style := StyleBoxFlat.new()
    style.bg_color = Color(0.11, 0.08, 0.05, 0.97)
    style.border_color = FRAME
    style.set_border_width_all(3)
    style.set_corner_radius_all(4)
    style.set_content_margin_all(16)
    style.shadow_color = Color(0, 0, 0, 0.5)
    style.shadow_size = 8
    _box.add_theme_stylebox_override("panel", style)
    add_child(_box)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 8)
    _box.add_child(column)
    var heading := _label(title, 20, GOLD)
    heading.name = "Title"
    heading.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    column.add_child(heading)
    if picture:
        var art := TextureRect.new()
        art.name = "Picture"
        art.texture = picture
        art.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
        art.custom_minimum_size = Vector2(0, 112)
        art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
        art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
        column.add_child(art)
    if not flavour.is_empty():
        var words := _label(flavour, 15, TEXT)
        words.name = "Flavour"
        column.add_child(words)
    if not reward.is_empty():
        var gain := _label(reward, 17, GOLD)
        gain.name = "Reward"
        gain.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
        column.add_child(gain)
    var row := HBoxContainer.new()
    row.alignment = BoxContainer.ALIGNMENT_CENTER
    row.add_theme_constant_override("separation", 10)
    column.add_child(row)
    for i in buttons.size():
        var button := Button.new()
        button.name = "Choice%d" % i
        button.text = buttons[i]
        button.custom_minimum_size = Vector2(110 if buttons.size() == 1 else 200, 38)
        button.pressed.connect(choose.bind(i))
        row.add_child(button)
    _center.call_deferred()
    (row.get_child(0) as Button).grab_focus.call_deferred()

func _center() -> void:
    _box.size = Vector2.ZERO
    _box.position = (get_viewport_rect().size - _box.get_combined_minimum_size()) * 0.5

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.custom_minimum_size = Vector2(380, 0)
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label

func choose(index: int) -> void:
    if not is_inside_tree(): return
    closed.emit(index)
    queue_free()

func _unhandled_key_input(event: InputEvent) -> void:
    if not (event is InputEventKey and event.pressed and not event.echo): return
    if event.keycode in [KEY_ENTER, KEY_KP_ENTER, KEY_SPACE]:
        choose(0)
    elif event.keycode == KEY_ESCAPE and buttons.size() == 1:
        choose(0)
    else:
        return
    get_viewport().set_input_as_handled()
