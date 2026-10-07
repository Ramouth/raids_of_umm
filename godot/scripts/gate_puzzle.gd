extends Control
## Match the survey diagram's place names; legacy gate IDs preserve pending saves.
signal finished(result: Dictionary)
var choice: Dictionary = {}
var outcome: Dictionary = {}
var readings: Array = []
const RUNES := ["Esh Varen", "Tal Eren", "Orun", "Ishara"]
const MARKS := ["BRANCH", "JOINED LINES", "CIRCLE", "DOORWAY"]
const HINTS := {
    "Gate rune · Esh Varen": "Ushari: The branch names Esh Varen, the settlement beneath your castle. The journey begins in its orchards.",
    "Gate rune · Tal Eren": "Ushari: The joined lines name Tal Eren. Cross its waters on the bridge Queen Sereth built.",
    "Gate rune · Orun": "Ushari: The circle names Orun. Our communities brought their petitions to that court.",
    "Gate rune · Ishara": "Ushari: The doorway names Ishara. My mother remembered it as the house of names. That is where we are returning."
}
var wheels := [2, 0, 3, 1]
var buttons: Array[Button] = []
var marks: Array[Control] = []
var feedback: Label
var hints := 0
var solved := false

# The player matches the surviving marks to Ushari's readings.
class RuneMark extends Control:
    var rune := 0
    func _draw() -> void:
        var ink := Color("e5c487")
        var center := Vector2(size.x * 0.5, 28)
        match rune:
            0: # Branch / orchard settlement.
                draw_line(center + Vector2(0, 19), center + Vector2(0, -19), ink, 3, true)
                for side in [-1, 1]:
                    draw_line(center, center + Vector2(side * 17, -14), ink, 3, true)
                    draw_line(center + Vector2(0, 10), center + Vector2(side * 17, -2), ink, 3, true)
            1: # Two waters joining at a crossing.
                draw_line(center + Vector2(-18, -18), center, ink, 3, true)
                draw_line(center + Vector2(18, -18), center, ink, 3, true)
                draw_line(center, center + Vector2(0, 20), ink, 3, true)
                draw_line(center + Vector2(-20, 5), center + Vector2(20, 5), ink, 3, true)
            2: # Petitioners around a court.
                draw_arc(center, 16, 0, TAU, 32, ink, 3, true)
                for angle in [0.0, PI / 2, PI, PI * 1.5]:
                    draw_line(center + Vector2.from_angle(angle) * 7, center + Vector2.from_angle(angle) * 22, ink, 3, true)
            3: # Open doorway / house of names.
                draw_polyline(PackedVector2Array([center + Vector2(-17, 20), center + Vector2(-17, -18), center + Vector2(17, -18), center + Vector2(17, 20)]), ink, 3, true)
                draw_line(center + Vector2(0, -8), center + Vector2(0, 20), ink, 3, true)

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    var center := CenterContainer.new()
    center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    add_child(center)
    var panel := PanelContainer.new()
    panel.custom_minimum_size = Vector2(700, 440)
    center.add_child(panel)
    var margin := MarginContainer.new()
    for side in ["left", "right", "top", "bottom"]:
        margin.add_theme_constant_override("margin_" + side, 28)
    panel.add_child(margin)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 16)
    margin.add_child(column)
    _label(column, "THE SURVEYORS’ ROAD", 26)
    _label(column, "Four names survive on the road diagram. Match the marks to read Aldren’s route.", 18)
    _label(column, "Ushari reads: ‘From the orchards, cross the joined waters; bring your petition to the court, then return to the house of names.’", 18)
    if not readings.is_empty():
        var scroll := ScrollContainer.new()
        scroll.custom_minimum_size = Vector2(640, 110)
        scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
        column.add_child(scroll)
        var translations := VBoxContainer.new()
        translations.add_theme_constant_override("separation", 8)
        scroll.add_child(translations)
        for reading in readings:
            _label(translations, str(reading.text), 16)
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 16)
    column.add_child(row)
    for i in range(4):
        var button := Button.new()
        button.custom_minimum_size = Vector2(145, 110)
        row.add_child(button)
        buttons.append(button)
        var mark := RuneMark.new()
        mark.mouse_filter = Control.MOUSE_FILTER_IGNORE
        mark.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
        button.add_child(mark)
        marks.append(mark)
        button.pressed.connect(turn_wheel.bind(i))
    _refresh()
    feedback = _label(column, "Ushari: Trace the route of the places that brought you home. These were our roads.", 18)
    var help := Button.new()
    help.text = "Ask Ushari for help"
    help.pressed.connect(ask_ushari)
    column.add_child(help)
    var submit := Button.new()
    submit.text = "Trace the route"
    submit.pressed.connect(try_lock)
    column.add_child(submit)
    var leave := Button.new()
    leave.name = "LeaveGate"
    leave.text = "Leave the survey · Search for inscriptions"
    leave.pressed.connect(leave_gate)
    column.add_child(leave)
    buttons[0].grab_focus()

func _label(parent: Node, words: String, size: int) -> Label:
    var label := Label.new()
    label.text = words
    label.custom_minimum_size.x = 640
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.add_theme_font_size_override("font_size", size)
    parent.add_child(label)
    return label

func turn_wheel(index: int) -> void:
    if solved: return
    wheels[index] = (wheels[index] + 1) % RUNES.size()
    _refresh()

func _refresh() -> void:
    for i in range(buttons.size()):
        var rune: int = wheels[i]
        var known := readings.any(func(entry): return str(entry.title) == "Gate rune · " + RUNES[rune])
        buttons[i].text = "\n\n%s ↻" % (RUNES[rune] if known else "Untranslated")
        buttons[i].tooltip_text = MARKS[rune].capitalize() + " mark"
        marks[i].rune = rune
        marks[i].queue_redraw()

func ask_ushari() -> void:
    if readings.is_empty():
        feedback.text = "Ushari: There are inscriptions in Varenhold’s wall, at Coldwater bridge, and in the ruined court south of Hallowmere. We can go back."
    else:
        var reading: Dictionary = readings[hints % readings.size()]
        feedback.text = str(HINTS.get(str(reading.title), reading.text))
    hints += 1

func try_lock() -> bool:
    if solved: return false
    var required: Array = choice.get("options", [{}])[0].get("requires_lore", [])
    for title in required:
        if not readings.any(func(entry): return str(entry.title) == str(title)):
            feedback.text = "Ushari: We still need the old names from Varenhold, Coldwater bridge and the assembly court."
            return false
    if choice.get("options", [{}])[0].get("requires_route", false) and not choice.get("route_ready", false):
        feedback.text = "The bearings are not enough. Secure Rookwatch, clear Troll Gate, or recover the escort’s patrol orders before setting out."
        return false
    if wheels != [0, 1, 2, 3]:
        feedback.text = "The bearings do not match. Ushari: Follow the journey: settlement, crossing, court, house of names."
        return false
    solved = true
    finished.emit({"choice": "esh_varen_tal_eren_orun_ishara"})
    return true

func leave_gate() -> void:
    if solved: return
    solved = true
    finished.emit({"action": "leave_gate"})

func _unhandled_key_input(event: InputEvent) -> void:
    if event.is_action_pressed("ui_cancel"):
        get_viewport().set_input_as_handled()
        leave_gate()
