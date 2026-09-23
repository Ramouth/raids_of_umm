extends Control
## HoMM3-style town screen: town art, recruit cards for the owner's roster,
## treasury, and the hero's army. Recruiting goes through the host's native
## adventure session; leaving emits finished({}).

signal finished(result: Dictionary)

const RESOURCES := ["Gold", "Wood", "Stone", "Obsidian", "Crystal"]
const CARD := Vector2(170, 360)

var host: Node        # desert.gd: provides state, unit_defs, recruit()
var cell := Vector2i.ZERO
var _treasury: Label
var _army: HBoxContainer
var _cards: HBoxContainer
var _message: Label
var _amounts: Dictionary = {}  # unit id -> SpinBox

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme  # same fonts and buttons as the adventure HUD
    var town := _town()
    _panel(Rect2(24, 18, 1232, 96))
    var title := _label(str(town.get("name", "Town")).to_upper(), 30, Color("f3dfb0"))
    title.position = Vector2(46, 26)
    add_child(title)
    var eyebrow := _label("IVORY COMPACT  ·  TOWN", 12, Color("ad854a"))
    eyebrow.position = Vector2(48, 74)
    add_child(eyebrow)
    _treasury = _label("", 15, Color("e8d8b8"))
    _treasury.position = Vector2(560, 50)
    _treasury.size = Vector2(680, 24)
    _treasury.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    add_child(_treasury)

    # Town view: the castle art large, over a warm dusk backdrop.
    _panel(Rect2(24, 128, 300, 520))
    var sky := ColorRect.new()
    sky.color = Color("3a2a1a")
    sky.position = Vector2(36, 140)
    sky.size = Vector2(276, 496)
    add_child(sky)
    var art := TextureRect.new()
    var view := "res://content/textures/screens/town_khemret.png"
    art.texture = load(view if ResourceLoader.exists(view) else "res://content/textures/objects/castle.png")
    art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
    art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    art.clip_contents = true
    art.position = Vector2(36, 140)
    art.size = Vector2(276, 420)
    add_child(art)
    var growth := _label("Recruits gather every 7th day.\nMines and towns pay income each dawn.", 13, Color("c8b08a"))
    growth.position = Vector2(48, 572)
    growth.size = Vector2(252, 80)
    growth.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    add_child(growth)

    _panel(Rect2(338, 128, 918, 520))
    var heading := _label("RECRUIT", 13, Color("ad854a"))
    heading.position = Vector2(358, 140)
    add_child(heading)
    var scroll := ScrollContainer.new()
    scroll.position = Vector2(350, 164)
    scroll.size = Vector2(894, 372)
    scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
    add_child(scroll)
    _cards = HBoxContainer.new()
    _cards.add_theme_constant_override("separation", 8)
    scroll.add_child(_cards)
    _message = _label("", 15, Color("f0c870"))
    _message.position = Vector2(358, 596)
    _message.size = Vector2(880, 40)
    add_child(_message)

    _panel(Rect2(24, 662, 1232, 120))
    var army_heading := _label("YOUR ARMY", 13, Color("ad854a"))
    army_heading.position = Vector2(46, 672)
    add_child(army_heading)
    _army = HBoxContainer.new()
    _army.position = Vector2(46, 694)
    _army.add_theme_constant_override("separation", 10)
    add_child(_army)
    var leave := Button.new()
    leave.name = "Leave"
    leave.text = "Leave town   Esc"
    leave.position = Vector2(1036, 712)
    leave.size = Vector2(200, 46)
    leave.pressed.connect(func(): finished.emit({}))
    add_child(leave)
    _build_cards()
    refresh()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
        finished.emit({})
        get_viewport().set_input_as_handled()

func _town() -> Dictionary:
    for town in host.state.get("towns", []):
        if town.cell[0] == cell.x and town.cell[1] == cell.y: return town
    return {}

func _build_cards() -> void:
    var pool: Dictionary = _town().get("pool", {})
    var ids := pool.keys()
    ids.sort_custom(func(a, b): return host.unit_defs[a].tier < host.unit_defs[b].tier)
    for id in ids:
        _cards.add_child(_card(host.unit_defs[id]))

func _card(unit: Dictionary) -> Control:
    var card := Panel.new()
    card.name = str(unit.id)
    card.add_theme_stylebox_override("panel", _style(Color("2b241a"), Color("7a5c34")))
    card.custom_minimum_size = CARD
    var box := VBoxContainer.new()
    box.position = Vector2(12, 8)
    box.size = CARD - Vector2(24, 16)
    box.add_theme_constant_override("separation", 3)
    card.add_child(box)
    var sprite := TextureRect.new()
    var path := "res://content/textures/units/%s.png" % unit.id
    if ResourceLoader.exists(path): sprite.texture = load(path)
    sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    sprite.custom_minimum_size = Vector2(0, 104)
    sprite.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    sprite.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    box.add_child(sprite)
    box.add_child(_wrapped("%s  ·  T%d" % [unit.name, unit.tier], 14, Color("f3dfb0")))
    var ranged := "  ·  %d shots" % unit.shots if int(unit.shots) > 0 else ""
    box.add_child(_wrapped("ATK %d  DEF %d  HP %d\nDMG %d–%d  SPD %d%s" % [unit.attack, unit.defense, unit.hitPoints,
        unit.minDamage, unit.maxDamage, unit.speed, ranged], 12, Color("c8b08a")))
    var costs: Array[String] = []
    for r in RESOURCES:
        if int(unit.cost.get(r, 0)) > 0: costs.append("%d %s" % [unit.cost[r], r])
    box.add_child(_wrapped("Cost: " + ", ".join(costs), 12, Color("e8d8b8")))
    var available := _wrapped("", 12, Color("9fc79a"))
    available.name = "Available"
    box.add_child(available)
    var row := HBoxContainer.new()
    var amount := SpinBox.new()
    amount.name = "Amount"
    amount.min_value = 0
    amount.custom_minimum_size = Vector2(88, 0)
    row.add_child(amount)
    _amounts[unit.id] = amount
    var max_button := Button.new()
    max_button.text = "Max"
    max_button.pressed.connect(func(): amount.value = amount.max_value)
    row.add_child(max_button)
    box.add_child(row)
    var hire := Button.new()
    hire.name = "Recruit"
    hire.text = "Recruit"
    hire.pressed.connect(func(): recruit(unit.id, int(amount.value)))
    box.add_child(hire)
    return card

## Recruits through the native session; returns true on success.
func recruit(unit_id: String, count: int) -> bool:
    if count <= 0:
        _message.text = "Choose how many to recruit."
        return false
    var reply: Dictionary = host.recruit(cell, unit_id, count)
    if not reply.get("ok", false):
        _message.text = str(reply.get("error", "Could not recruit."))
        return false
    _message.text = "%d %s join the expedition." % [count, host.unit_defs[unit_id].name]
    refresh()
    return true

func refresh() -> void:
    var state: Dictionary = host.state
    var parts: Array[String] = []
    for r in RESOURCES: parts.append("%s %d" % [r, state.treasury[r]])
    _treasury.text = "    ".join(parts)
    var pool: Dictionary = _town().get("pool", {})
    for id in _amounts:
        var have := int(pool.get(id, 0))
        var unit: Dictionary = host.unit_defs[id]
        var afford := have
        for r in RESOURCES:
            var price := int(unit.cost.get(r, 0))
            if price > 0: afford = mini(afford, int(state.treasury[r]) / price)
        var card: Control = _cards.get_node(NodePath(id))
        card.find_child("Available", true, false).text = "Available %d · can afford %d" % [have, afford]
        _amounts[id].max_value = afford
        _amounts[id].value = mini(int(_amounts[id].value), afford)
    for child in _army.get_children(): child.queue_free()
    for stack in state.get("army", []):
        var slot := VBoxContainer.new()
        slot.custom_minimum_size = Vector2(120, 0)
        var icon := TextureRect.new()
        var path := "res://content/textures/units/%s.png" % stack.id
        if ResourceLoader.exists(path): icon.texture = load(path)
        icon.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
        icon.custom_minimum_size = Vector2(0, 52)
        icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
        icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
        slot.add_child(icon)
        slot.add_child(_label("%d %s" % [stack.count, host.unit_defs.get(stack.id, {}).get("name", stack.id)], 12, Color("e8d8b8")))
        _army.add_child(slot)

func _panel(rect: Rect2) -> void:
    var panel := Panel.new()
    panel.position = rect.position
    panel.size = rect.size
    panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
    panel.add_theme_stylebox_override("panel", _style(Color("211c14"), Color("665032")))
    add_child(panel)

func _style(fill: Color, border: Color) -> StyleBoxFlat:
    var style := StyleBoxFlat.new()
    style.bg_color = fill
    style.border_color = border
    style.set_border_width_all(1)
    return style

func _wrapped(text: String, size: int, color: Color) -> Label:
    var label := _label(text, size, color)
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.custom_minimum_size = Vector2(CARD.x - 24, 0)
    return label

func _label(text: String, size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
