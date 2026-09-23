extends PanelContainer
## Quest log (Q): main quest first, then Kharim's optional tasks.
## Completed quests are dimmed and marked done.

var _list: VBoxContainer

func _ready() -> void:
    visible = false
    custom_minimum_size = Vector2(460, 0)
    var style := StyleBoxFlat.new()
    style.bg_color = Color(0.11, 0.08, 0.05, 0.96)
    style.border_color = Color("a07a44")
    style.set_border_width_all(2)
    style.set_content_margin_all(16)
    add_theme_stylebox_override("panel", style)
    _list = VBoxContainer.new()
    _list.add_theme_constant_override("separation", 10)
    add_child(_list)

func show_quests(quests: Array) -> void:
    for child in _list.get_children(): child.queue_free()
    _list.add_child(_label("QUEST LOG   ·   Q to close", 12, Color("ad854a")))
    var ordered := quests.duplicate()
    ordered.sort_custom(func(a, b): return a.main and not b.main)
    if ordered.is_empty():
        _list.add_child(_label("No quests yet.", 14, Color("c8b08a")))
    for quest in ordered:
        var kind := "MAIN QUEST" if quest.main else "OPTIONAL"
        var title := "%s   ·   %s%s" % [quest.title, kind, "   ·   DONE" if quest.done else ""]
        _list.add_child(_label(title, 16, Color("7c7060") if quest.done else Color("f0c870")))
        _list.add_child(_label(str(quest.text), 14, Color("7c7060") if quest.done else Color("e8d8b8")))

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.custom_minimum_size = Vector2(428, 0)
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
