extends Control
## Victory / defeat screen pushed on the ScreenStack when the scenario ends.
## finished({"action": "new"}) starts a new expedition; "quit" closes the game.
## A campaign map's ending offers "continue" (on to the next map); a lost map
## offers "retry" (the same map again, with the company that rode into it).

signal finished(result: Dictionary)

var heading := ""
var body := ""
var stats := ""
var victory := false
var illustration := ""   # Art id shown above the text; "" for none
var eyebrow := ""        # e.g. "THE LAST HUNT  ·  MAP COMPLETE"; "" = the old demo's line
## [label, action] pairs, in order; the first takes focus.
var actions: Array = [["New expedition", "new"], ["Quit", "quit"]]
const BUTTON_NAMES := {"new": "NewExpedition", "quit": "Quit", "continue": "Continue", "retry": "Retry", "menu": "MainMenu"}

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    var box := VBoxContainer.new()
    box.custom_minimum_size = Vector2(900, 0)
    box.position = Vector2(190, 24)
    box.add_theme_constant_override("separation", 14)
    add_child(box)
    if not illustration.is_empty():
        var art := TextureRect.new()
        art.name = "Illustration"
        art.texture = Art.texture(illustration)
        art.custom_minimum_size = Vector2(900, 440)
        art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
        art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
        box.add_child(art)
    var line := eyebrow if not eyebrow.is_empty() else "THE OLD PASSAGE  ·  " + ("SCENARIO COMPLETE" if victory else "EXPEDITION LOST")
    box.add_child(_label(line, 13, Color("ad854a")))
    box.add_child(_label(heading, 32, Color("f3dfb0") if victory else Color("d98a6a")))
    box.add_child(_label(body, 16, Color("e8d8b8")))
    box.add_child(_label(stats, 15, Color("b8a080")))
    var row := HBoxContainer.new()
    row.alignment = BoxContainer.ALIGNMENT_CENTER
    row.add_theme_constant_override("separation", 18)
    box.add_child(row)
    var first: Button
    for entry in actions:
        var action := str(entry[1])
        var button := Button.new()
        button.name = BUTTON_NAMES.get(action, action.capitalize())
        button.text = str(entry[0])
        button.custom_minimum_size = Vector2(220 if first == null else 160, 46)
        button.pressed.connect(func(): finished.emit({"action": action}))
        row.add_child(button)
        if first == null: first = button
    if first != null: first.grab_focus.call_deferred()

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
