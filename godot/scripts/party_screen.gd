extends Control
## Party (P): special characters, their level, XP, stage abilities and upkeep.
## In an owned town a companion can be left to govern (+100 gold per level a
## day, no XP) or recalled.

signal finished(result: Dictionary)

const PORTRAITS := {"ushari": "units/ushari.png", "kharim": "portraits/kharim.png"}

var host: Node   # desert.gd: provides state, station()
var _list: VBoxContainer
var _message: Label

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme
    var panel := Panel.new()
    panel.position = Vector2(90, 60)
    panel.size = Vector2(1100, 680)
    var style := StyleBoxFlat.new()
    style.bg_color = Color("211c14")
    style.border_color = Color("665032")
    style.set_border_width_all(1)
    panel.add_theme_stylebox_override("panel", style)
    add_child(panel)
    var heading := _label("COMPANIONS", 24, Color("f3dfb0"))
    heading.position = Vector2(120, 80)
    add_child(heading)
    var sub := _label("Special characters grow stranger and stronger with every victory, and dearer to keep.", 14, Color("c8b08a"))
    sub.position = Vector2(120, 118)
    add_child(sub)
    var scroll := ScrollContainer.new()
    scroll.position = Vector2(120, 150)
    scroll.size = Vector2(1040, 500)
    add_child(scroll)
    _list = VBoxContainer.new()
    _list.add_theme_constant_override("separation", 18)
    scroll.add_child(_list)
    _message = _label("", 15, Color("f0c870"))
    _message.position = Vector2(120, 668)
    add_child(_message)
    var done := Button.new()
    done.name = "Done"
    done.text = "Done   Esc"
    done.position = Vector2(990, 680)
    done.size = Vector2(170, 44)
    done.pressed.connect(func(): finished.emit({}))
    add_child(done)
    refresh()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and event.keycode in [KEY_ESCAPE, KEY_P]:
        finished.emit({})
        get_viewport().set_input_as_handled()

func refresh() -> void:
    for child in _list.get_children(): child.queue_free()
    var specials: Array = host.state.get("specials", [])
    if specials.is_empty():
        _list.add_child(_label("No companions ride with you.", 16, Color("9a8468")))
    for sc in specials:
        _list.add_child(_card(sc))

func _card(sc: Dictionary) -> Control:
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 18)
    var portrait := TextureRect.new()
    var path := "res://content/textures/" + str(PORTRAITS.get(sc.id, "units/armoured_warrior.png"))
    if ResourceLoader.exists(path): portrait.texture = load(path)
    portrait.custom_minimum_size = Vector2(128, 128)
    portrait.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    portrait.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    row.add_child(portrait)
    var column := VBoxContainer.new()
    column.custom_minimum_size = Vector2(700, 0)
    row.add_child(column)
    var status := "travelling with the hero"
    if sc.stationed != null: status = "governing a town (+%d gold a day)" % (100 * int(sc.level))
    if int(sc.unpaid) >= 3: status = "UNPAID %d days — abilities withheld, leaves at 7" % int(sc.unpaid)
    elif int(sc.unpaid) > 0: status += "  ·  unpaid %d day(s)" % int(sc.unpaid)
    column.add_child(_label("%s  ·  %s" % [sc.name, sc.title], 18, Color("f0c870")))
    column.add_child(_label("Level %d   ·   XP %d / %d   ·   Upkeep %d gold a day   ·   %s" % [sc.level, sc.xp, sc.next, sc.upkeep, status], 13, Color("c8b08a")))
    for ability in sc.abilities:
        var mark := "✓" if ability.unlocked else "Lv %d" % int(ability.level)
        var line := _label("%s  %s — %s" % [mark, ability.name, ability.text], 14, Color("e8d8b8") if ability.unlocked else Color("7c7060"))
        line.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
        line.custom_minimum_size = Vector2(700, 0)
        column.add_child(line)
    var actions := VBoxContainer.new()
    row.add_child(actions)
    if sc.stationed == null and host.can_govern():
        var govern := Button.new()
        govern.text = "Govern this town"
        govern.pressed.connect(func(): _station(str(sc.id), true))
        actions.add_child(govern)
    elif sc.stationed != null:
        var recall := Button.new()
        recall.text = "Recall"
        recall.pressed.connect(func(): _station(str(sc.id), false))
        actions.add_child(recall)
    return row

func _station(id: String, stay: bool) -> void:
    var reply: Dictionary = host.station(id, stay)
    _message.text = "" if reply.get("ok", false) else str(reply.get("error", ""))
    refresh()

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
