extends Control
## Victory / defeat screen pushed on the ScreenStack when the scenario ends.
## finished({"action": "new"}) starts a new expedition; "quit" closes the game.

signal finished(result: Dictionary)

var heading := ""
var body := ""
var stats := ""
var victory := false

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    var box := VBoxContainer.new()
    box.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    box.custom_minimum_size = Vector2(760, 0)
    box.position = Vector2(-380, -200)
    box.add_theme_constant_override("separation", 22)
    add_child(box)
    var eyebrow := _label("THE OLD PASSAGE  ·  " + ("SCENARIO COMPLETE" if victory else "EXPEDITION LOST"), 13, Color("ad854a"))
    box.add_child(eyebrow)
    box.add_child(_label(heading, 40, Color("f3dfb0") if victory else Color("d98a6a")))
    box.add_child(_label(body, 18, Color("e8d8b8")))
    box.add_child(_label(stats, 15, Color("b8a080")))
    var row := HBoxContainer.new()
    row.alignment = BoxContainer.ALIGNMENT_CENTER
    row.add_theme_constant_override("separation", 18)
    box.add_child(row)
    var again := Button.new()
    again.name = "NewExpedition"
    again.text = "New expedition"
    again.custom_minimum_size = Vector2(220, 46)
    again.pressed.connect(func(): finished.emit({"action": "new"}))
    row.add_child(again)
    var quit := Button.new()
    quit.name = "Quit"
    quit.text = "Quit"
    quit.custom_minimum_size = Vector2(160, 46)
    quit.pressed.connect(func(): finished.emit({"action": "quit"}))
    row.add_child(quit)
    again.grab_focus.call_deferred()

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
