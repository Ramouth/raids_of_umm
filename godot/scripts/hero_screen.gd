extends Control
## Hero (H): the commander's paper doll (HoMM3). Seven slots around the
## commander; what is worn helps every troop stack in the army. Unworn items
## wait in the backpack. Click a backpack item to wear it, a worn one to take
## it off; cursed items will not come off.

signal finished(result: Dictionary)

const SLOTS := {   # slot -> [label, position on the doll]
    "helm": ["Helm", Vector2(250, 0)],
    "amulet": ["Amulet", Vector2(420, 70)],
    "weapon": ["Weapon", Vector2(40, 150)],
    "armor": ["Armour", Vector2(420, 170)],
    "trinket1": ["Trinket", Vector2(40, 260)],
    "trinket2": ["Trinket", Vector2(420, 270)],
    "boots": ["Boots", Vector2(250, 370)],
}
const TEXT := Color("e8d8b8")
const DIM := Color("9a8468")
const GOLD := Color("f0c870")
const CURSED := Color("e0806a")

var host: Node   # desert.gd: state, item_defs, equip(), unequip()
var _doll: Control
var _pack: VBoxContainer
var _bonus: Label
var _message: Label
var _detail: RichTextLabel

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme
    _panel(Rect2(60, 40, 1160, 720))
    var heading := _label("THE COMMANDER", 24, Color("f3dfb0"))
    heading.position = Vector2(90, 58)
    add_child(heading)
    var progress: Dictionary = host.state.get("hero_progress", {})
    var sub := _label("Level %d  ·  %d XP  ·  what the commander wears, the whole army feels" % [int(progress.get("level", 1)), int(progress.get("xp", 0))], 14, Color("c8b08a"))
    sub.position = Vector2(90, 96)
    add_child(sub)

    _doll = Control.new()
    _doll.position = Vector2(90, 140)
    _doll.size = Vector2(600, 460)
    add_child(_doll)
    var figure := TextureRect.new()
    figure.texture = load("res://content/textures/units/armoured_warrior.png")
    figure.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    figure.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    figure.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    figure.position = Vector2(200, 80)
    figure.size = Vector2(200, 280)
    _doll.add_child(figure)

    _bonus = _label("", 16, GOLD)
    _bonus.position = Vector2(90, 612)
    _bonus.size = Vector2(600, 24)
    add_child(_bonus)

    var pack_title := _label("BACKPACK", 13, Color("ad854a"))
    pack_title.position = Vector2(730, 140)
    add_child(pack_title)
    var scroll := ScrollContainer.new()
    scroll.position = Vector2(730, 164)
    scroll.size = Vector2(460, 300)
    scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
    add_child(scroll)
    _pack = VBoxContainer.new()
    _pack.add_theme_constant_override("separation", 6)
    scroll.add_child(_pack)
    _detail = RichTextLabel.new()
    _detail.bbcode_enabled = true
    _detail.position = Vector2(730, 476)
    _detail.size = Vector2(460, 150)
    _detail.add_theme_font_size_override("normal_font_size", 14)
    _detail.add_theme_color_override("default_color", TEXT)
    add_child(_detail)

    _message = _label("", 15, GOLD)
    _message.position = Vector2(90, 690)
    add_child(_message)
    var done := Button.new()
    done.name = "Done"
    done.text = "Done   Esc"
    done.position = Vector2(1020, 694)
    done.size = Vector2(170, 44)
    done.pressed.connect(func(): finished.emit({}))
    add_child(done)
    refresh()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and event.keycode in [KEY_ESCAPE, KEY_H]:
        finished.emit({})
        get_viewport().set_input_as_handled()

func refresh() -> void:
    var state: Dictionary = host.state
    for child in _doll.get_children():
        if child is Button: child.queue_free()
    var worn: Dictionary = state.get("equipped", {})
    for slot in SLOTS:
        var id = worn.get(slot)
        var button := Button.new()
        button.name = slot
        button.position = SLOTS[slot][1]
        button.size = Vector2(150, 74)
        button.clip_text = true
        if id == null:
            button.text = "%s\n—" % SLOTS[slot][0]
            button.modulate = Color(1, 1, 1, 0.55)
        else:
            var item: Dictionary = host.item_defs.get(id, {"name": id})
            button.text = "%s\n%s" % [SLOTS[slot][0], item.name]
            if item.get("cursed", false): button.add_theme_color_override("font_color", CURSED)
            button.pressed.connect(take_off.bind(slot))
            button.mouse_entered.connect(_describe.bind(str(id)))
        _doll.add_child(button)
    for child in _pack.get_children(): child.queue_free()
    var pack: Array = state.get("backpack", [])
    if pack.is_empty(): _pack.add_child(_label("Nothing unworn. Items found on the road land here.", 14, DIM))
    for id in pack:
        var item: Dictionary = host.item_defs.get(id, {"name": id, "slot": "?"})
        var row := HBoxContainer.new()
        row.name = str(id)
        row.add_theme_constant_override("separation", 10)
        var name := _label("%s  ·  %s" % [item.name, _slot_name(str(item.get("slot", "")))], 14, CURSED if item.get("cursed", false) else TEXT)
        name.custom_minimum_size = Vector2(300, 0)
        name.mouse_filter = Control.MOUSE_FILTER_PASS
        name.mouse_entered.connect(_describe.bind(str(id)))
        row.add_child(name)
        var wear := Button.new()
        wear.name = "Wear"
        wear.text = "Wear"
        wear.pressed.connect(put_on.bind(str(id)))
        wear.mouse_entered.connect(_describe.bind(str(id)))
        row.add_child(wear)
        _pack.add_child(row)
    var bonus: Dictionary = state.get("army_bonus", {})
    _bonus.text = "Every troop stack:  %+d attack   %+d defence   %+d speed" % [int(bonus.get("attack", 0)), int(bonus.get("defense", 0)), int(bonus.get("speed", 0))]

func _slot_name(slot: String) -> String:
    return {"helm": "helm", "amulet": "amulet", "armor": "armour", "weapon": "weapon", "boots": "boots", "trinket": "trinket"}.get(slot, slot)

func _effects(item: Dictionary) -> String:
    var parts: Array[String] = []
    for effect in item.get("passiveEffects", []):
        parts.append("%+d %s" % [int(effect.amount), {"defense": "defence"}.get(str(effect.stat), str(effect.stat))])
    return ", ".join(parts)

func _describe(id: String) -> void:
    var item: Dictionary = host.item_defs.get(id, {})
    if item.is_empty(): return
    var curse := "\n[color=#%s]Cursed: once worn, it will not come off.[/color]" % CURSED.to_html(false) if item.get("cursed", false) else ""
    _detail.text = "[b][color=#%s]%s[/color][/b]  (%s)\n%s\n[color=#%s]For every troop stack: %s[/color]%s" % [
        GOLD.to_html(false), item.name, _slot_name(str(item.get("slot", ""))), item.get("description", ""),
        GOLD.to_html(false), _effects(item), curse]

func put_on(id: String) -> bool:
    var reply: Dictionary = host.equip(id)
    _message.text = str(reply.get("error", "")) if not reply.get("ok", false) else "The commander wears the %s." % host.item_defs.get(id, {"name": id}).name
    refresh()
    return reply.get("ok", false)

func take_off(slot: String) -> bool:
    var reply: Dictionary = host.unequip(slot)
    _message.text = str(reply.get("error", "")) if not reply.get("ok", false) else "Back in the backpack."
    refresh()
    return reply.get("ok", false)

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
