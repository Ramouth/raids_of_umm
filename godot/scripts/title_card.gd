extends Control
## Between two campaign maps: the chapter, the next map's name and a line of
## the prophecy, on a dark card. Click, Space or Enter (or the button) goes on.

signal finished(result: Dictionary)

var chapter_title := ""
var map_title := ""
var new_chapter := false
## The author's line for this map (campaign.json map entry "prophecy");
## until it is written the card shows a marked placeholder.
var prophecy := ""
const PROPHECY_PLACEHOLDER := "[prophecy fragment]"

var _closed := false

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    mouse_filter = Control.MOUSE_FILTER_STOP
    var black := ColorRect.new()
    black.color = Color("0b0907")
    black.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    black.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(black)
    var center := CenterContainer.new()
    center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    center.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(center)
    var column := VBoxContainer.new()
    column.custom_minimum_size = Vector2(760, 0)
    column.add_theme_constant_override("separation", 16)
    center.add_child(column)
    var eyebrow := _label(("A NEW CHAPTER  ·  " if new_chapter else "") + chapter_title.to_upper(), 15, Color("ad854a"))
    eyebrow.name = "Chapter"
    column.add_child(eyebrow)
    var rule := ColorRect.new()
    rule.color = Color("6b5230")
    rule.custom_minimum_size = Vector2(0, 1)
    column.add_child(rule)
    var title := _label(map_title, 36, Color("f3dfb0"))
    title.name = "MapTitle"
    column.add_child(title)
    var verse := _label(prophecy if not prophecy.is_empty() else PROPHECY_PLACEHOLDER, 17,
        Color("c8b08a") if not prophecy.is_empty() else Color("8a7a62"))
    verse.name = "Prophecy"
    column.add_child(verse)
    var gap := Control.new()
    gap.custom_minimum_size.y = 18
    column.add_child(gap)
    var go := Button.new()
    go.name = "Continue"
    go.text = "Set out"
    go.custom_minimum_size = Vector2(220, 46)
    go.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
    go.add_theme_font_size_override("font_size", 16)
    go.add_theme_color_override("font_color", Color("f7dea3"))
    for state in ["normal", "hover", "pressed", "focus"]:
        var style := StyleBoxFlat.new()
        style.bg_color = Color(0.2, 0.15, 0.09) if state != "hover" else Color(0.27, 0.2, 0.11)
        style.border_color = Color(0.5, 0.37, 0.18) if state != "focus" else Color("c9a24a")
        style.set_border_width_all(1)
        style.draw_center = state != "focus"
        go.add_theme_stylebox_override(state, style)
    go.pressed.connect(close)
    column.add_child(go)
    go.grab_focus.call_deferred()
    modulate.a = 0.0
    create_tween().tween_property(self, "modulate:a", 1.0, 0.5)

func close() -> void:
    if _closed: return
    _closed = true
    finished.emit({})

func _gui_input(event: InputEvent) -> void:
    if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
        close()

func _unhandled_key_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo \
            and event.keycode in [KEY_SPACE, KEY_ENTER, KEY_KP_ENTER, KEY_ESCAPE]:
        get_viewport().set_input_as_handled()
        close()

func _label(text: String, size: int, colour: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", colour)
    return label
