extends PanelContainer
## Warcraft 3-style transmission: portrait + speaker + line, bottom-left over
## the map, while the player keeps control. Lines queue up; click, Space or
## Enter advances, and each line also advances on its own after a while.

signal drained

const PORTRAITS := {
    "Ushari": "portraits/ushari.png", "Kharim": "portraits/kharim.png",
    "Scout": "units/rider_archer.png", "Messenger": "units/levy_spearman.png",
    "Aldren": "portraits/aldren.png", "Corvin": "portraits/corvin.png",
    "Inscription": "objects/obelisk.png", "Hooded Druid": "portraits/druid.png",
    "Maerwen": "portraits/maerwen.png",
    "Steward": "units/levy_spearman.png",
}

var _queue: Array[Dictionary] = []
var _portrait: TextureRect
var _speaker: Label
var _text: Label
var _hint: Label
var _timer: Timer

func _ready() -> void:
    visible = false
    mouse_filter = Control.MOUSE_FILTER_STOP
    custom_minimum_size = Vector2(620, 118)
    var style := StyleBoxFlat.new()
    style.bg_color = Color(0.11, 0.08, 0.05, 0.94)
    style.border_color = Color("a07a44")
    style.set_border_width_all(2)
    style.set_content_margin_all(10)
    add_theme_stylebox_override("panel", style)
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 14)
    add_child(row)
    _portrait = TextureRect.new()
    _portrait.custom_minimum_size = Vector2(96, 96)
    _portrait.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    _portrait.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    _portrait.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    row.add_child(_portrait)
    var column := VBoxContainer.new()
    column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    row.add_child(column)
    _speaker = Label.new()
    _speaker.add_theme_font_size_override("font_size", 15)
    _speaker.add_theme_color_override("font_color", Color("f0c870"))
    column.add_child(_speaker)
    _text = Label.new()
    _text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    _text.custom_minimum_size = Vector2(480, 0)
    _text.add_theme_font_size_override("font_size", 15)
    _text.add_theme_color_override("font_color", Color("f3e6c8"))
    column.add_child(_text)
    _hint = Label.new()
    _hint.add_theme_font_size_override("font_size", 11)
    _hint.add_theme_color_override("font_color", Color("9a8468"))
    column.add_child(_hint)
    _timer = Timer.new()
    _timer.one_shot = true
    _timer.timeout.connect(advance)
    add_child(_timer)

func say(lines: Array) -> void:
    for line in lines: _queue.append(line)
    if not visible: advance()
    else: _update_hint()

func is_speaking() -> bool:
    return visible

func advance() -> void:
    if _queue.is_empty():
        visible = false
        _timer.stop()
        drained.emit()
        return
    var line: Dictionary = _queue.pop_front()
    _speaker.text = str(line.speaker).to_upper()
    _text.text = str(line.text)
    var path: String = "res://content/textures/" + str(PORTRAITS.get(line.speaker, "units/armoured_warrior.png"))
    _portrait.texture = load(path) if ResourceLoader.exists(path) else null
    _update_hint()
    visible = true
    _timer.start(4.0 + str(line.text).length() / 22.0)

func skip_all() -> void:
    _queue.clear()
    advance()

func _update_hint() -> void:
    _hint.text = "Click / Space: next  (%d more)" % _queue.size() if not _queue.is_empty() else "Click / Space: close"

func _gui_input(event: InputEvent) -> void:
    if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
        advance()
        accept_event()
