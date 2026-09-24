extends Control
## HoMM3-style town screen: town art, recruit cards for the owner's roster,
## treasury, and the hero's army. Recruiting goes through the host's native
## adventure session; leaving emits finished({}).

signal finished(result: Dictionary)

const RESOURCES := ["Gold", "Wood", "Stone", "Obsidian", "Crystal"]
const CARD := Vector2(170, 360)

var host: Node        # desert.gd: provides state, unit_defs, recruit()
var cell := Vector2i.ZERO
var view_art := "res://content/textures/screens/town_khemret.png"   # dwellings pass their map sprite
var eyebrow_text := "IVORY COMPACT  ·  TOWN"
var can_build := true   # dwellings on the map only recruit
var tab := "recruit"
var _tabs: Dictionary = {}       # name -> Button
var _pages: Dictionary = {}      # name -> Control
var _buildings: GridContainer
var _build_note: Label
var _give: OptionButton
var _get: OptionButton
var _trade_amount: SpinBox
var _quote: Label
var _treasury: Label
var _army: HBoxContainer
var _cards: HBoxContainer
var _message: Label
var _amounts: Dictionary = {}  # unit id -> SpinBox

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    theme = host.get_node("HUD/Layout").theme  # same fonts and buttons as the adventure HUD
    var town := _town()
    if can_build and not str(town.get("title", "")).is_empty(): eyebrow_text = "IVORY COMPACT  ·  " + str(town.title).to_upper()
    if can_build and not str(town.get("art", "")).is_empty(): view_art = "res://content/textures/" + str(town.art)
    _panel(Rect2(24, 18, 1232, 96))
    var title := _label(str(town.get("name", "Town")).to_upper(), 30, Color("f3dfb0"))
    title.position = Vector2(46, 26)
    add_child(title)
    var eyebrow := _label(eyebrow_text, 12, Color("ad854a"))
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
    var view := view_art
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
    var tabs := HBoxContainer.new()
    tabs.position = Vector2(352, 136)
    tabs.add_theme_constant_override("separation", 6)
    add_child(tabs)
    for name in (["recruit", "build", "market"] if can_build else ["recruit"]):
        var button := Button.new()
        button.name = name.capitalize()
        button.text = {"recruit": "Recruit   R", "build": "Build   B", "market": "Market   M"}[name]
        button.toggle_mode = true
        button.custom_minimum_size = Vector2(130, 30)
        button.pressed.connect(show_tab.bind(name))
        tabs.add_child(button)
        _tabs[name] = button
    var page := Control.new()
    page.position = Vector2(350, 190)
    add_child(page)
    _pages["recruit"] = page
    var scroll := ScrollContainer.new()
    scroll.size = Vector2(894, 372)
    scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
    page.add_child(scroll)
    _cards = HBoxContainer.new()
    _cards.add_theme_constant_override("separation", 8)
    scroll.add_child(_cards)
    if can_build:
        _pages["build"] = _build_page()
        _pages["market"] = _market_page()
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
    show_tab(tab)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
        finished.emit({})
        get_viewport().set_input_as_handled()
    elif event is InputEventKey and event.pressed and not event.echo and can_build:
        var key: String = {KEY_R: "recruit", KEY_B: "build", KEY_M: "market"}.get(event.keycode, "")
        if not key.is_empty():
            show_tab(key)
            get_viewport().set_input_as_handled()

func show_tab(name: String) -> void:
    if not _pages.has(name): return
    tab = name
    for key in _pages:
        _pages[key].visible = key == name
        if _tabs.has(key): _tabs[key].set_pressed_no_signal(key == name)
    _message.text = ""
    refresh()

# ── Build ─────────────────────────────────────────────────────────────────────

func _build_page() -> Control:
    var page := Control.new()
    page.position = Vector2(350, 190)
    add_child(page)
    _build_note = _label("", 13, Color("c8b08a"))
    _build_note.size = Vector2(894, 20)
    page.add_child(_build_note)
    var scroll := ScrollContainer.new()
    scroll.position = Vector2(0, 24)
    scroll.size = Vector2(894, 372)
    scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
    page.add_child(scroll)
    _buildings = GridContainer.new()
    _buildings.columns = 3
    _buildings.add_theme_constant_override("h_separation", 8)
    _buildings.add_theme_constant_override("v_separation", 8)
    scroll.add_child(_buildings)
    return page

## One card per building: what it does, what it costs, and built / build / why not.
func _building_card(def: Dictionary, town: Dictionary) -> Control:
    var built: bool = town.get("buildings", []).has(def.id)
    var blocker: String = town.get("blockers", {}).get(def.id, "")
    var card := Panel.new()
    card.name = str(def.id)
    card.custom_minimum_size = Vector2(290, 142)
    var waiting := blocker.begins_with("Tomorrow")
    var border := Color("7fae7a") if built else (Color("c9a24e") if blocker.is_empty() else (Color("8a7440") if waiting else Color("5e4a30")))
    card.add_theme_stylebox_override("panel", _style(Color("2b241a") if not built else Color("24301f"), border))
    var box := VBoxContainer.new()
    box.position = Vector2(10, 6)
    box.size = Vector2(270, 130)
    box.add_theme_constant_override("separation", 2)
    card.add_child(box)
    box.add_child(_wrapped_to(str(def.name), 15, Color("f3dfb0"), 270))
    box.add_child(_wrapped_to(str(def.description), 11, Color("c8b08a"), 270))
    var costs: Array[String] = []
    for r in RESOURCES:
        if int(def.cost.get(r, 0)) > 0: costs.append("%d %s" % [def.cost[r], r])
    if not built: box.add_child(_wrapped_to("Cost: " + (", ".join(costs) if not costs.is_empty() else "free"), 11, Color("e8d8b8"), 270))
    if built:
        box.add_child(_wrapped_to("✓ Built", 13, Color("9fc79a"), 270))
    elif blocker.is_empty():
        var go := Button.new()
        go.name = "Build"
        go.text = "Build"
        go.pressed.connect(func(): build(str(def.id)))
        box.add_child(go)
    else:
        box.add_child(_wrapped_to(blocker, 12, Color("d8c27a") if waiting else Color("d79a7a"), 270))
    return card

## Builds through the native session; returns true on success.
func build(building_id: String) -> bool:
    var reply: Dictionary = host.build(cell, building_id)
    if not reply.get("ok", false):
        _message.text = str(reply.get("error", "Could not build."))
        return false
    for def in host.state.get("building_defs", []):
        if def.id == building_id: _message.text = "%s is built. The town can build again tomorrow." % def.name
    _build_cards()
    refresh()
    return true

# ── Market ────────────────────────────────────────────────────────────────────

func _market_page() -> Control:
    var page := VBoxContainer.new()
    page.position = Vector2(350, 190)
    page.size = Vector2(600, 300)
    page.add_theme_constant_override("separation", 10)
    add_child(page)
    page.add_child(_label("Sell at half value, buy at one and a half (Wood/Stone 150 gold, Obsidian/Crystal 400).", 13, Color("c8b08a")))
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 8)
    page.add_child(row)
    row.add_child(_label("Give", 14, Color("e8d8b8")))
    _trade_amount = SpinBox.new()
    _trade_amount.min_value = 1
    _trade_amount.max_value = 100000
    _trade_amount.value = 5
    _trade_amount.custom_minimum_size = Vector2(110, 0)
    _trade_amount.value_changed.connect(func(_v): _update_quote())
    row.add_child(_trade_amount)
    _give = OptionButton.new()
    _get = OptionButton.new()
    for r in RESOURCES:
        _give.add_item(r)
        _get.add_item(r)
    _give.select(1)
    _get.select(0)
    _give.item_selected.connect(func(_i): _update_quote())
    _get.item_selected.connect(func(_i): _update_quote())
    row.add_child(_give)
    row.add_child(_label("for", 14, Color("e8d8b8")))
    row.add_child(_get)
    _quote = _label("", 15, Color("f0c870"))
    page.add_child(_quote)
    var go := Button.new()
    go.name = "Trade"
    go.text = "Trade"
    go.custom_minimum_size = Vector2(160, 36)
    go.pressed.connect(func(): trade(_give.get_item_text(_give.selected), _get.get_item_text(_get.selected), int(_trade_amount.value)))
    page.add_child(go)
    return page

func _update_quote() -> void:
    if _quote == null: return
    if not host.state.get("market", false):
        _quote.text = "Build a Marketplace (Build tab) to trade."
        return
    var give := _give.get_item_text(_give.selected)
    var get := _get.get_item_text(_get.selected)
    _quote.text = "%d %s  →  %d %s" % [int(_trade_amount.value), give, host.trade_quote(give, get, int(_trade_amount.value)), get]

## Trades through the native session; returns true on success.
func trade(give: String, get: String, amount: int) -> bool:
    var reply: Dictionary = host.trade(give, get, amount)
    if not reply.get("ok", false):
        _message.text = str(reply.get("error", "Could not trade."))
        return false
    _message.text = "Traded %d %s for %d %s." % [amount, give, int(reply.get("received", 0)), get]
    refresh()
    return true

func _town() -> Dictionary:
    for town in host.state.get("towns", []):
        if town.cell[0] == cell.x and town.cell[1] == cell.y: return town
    return {}

func _build_cards() -> void:
    for child in _cards.get_children():
        _cards.remove_child(child)
        child.queue_free()
    _amounts.clear()
    var town := _town()
    var ids: Array = town.get("pool", {}).keys()
    for id in town.get("locked", []):
        if not ids.has(id): ids.append(id)
    ids = ids.filter(func(id): return host.unit_defs.has(id))
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
    if _town().get("locked", []).has(unit.id):   # its dwelling is not built yet
        card.modulate = Color(0.62, 0.58, 0.52)
        amount.editable = false
        hire.disabled = true
        max_button.disabled = true
        var dwelling := ""
        for def in host.state.get("building_defs", []):
            if def.unlocks == unit.id: dwelling = def.name
        hire.text = "Build the %s" % dwelling if not dwelling.is_empty() else "Locked"
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
        if _town().get("locked", []).has(id):
            card.find_child("Available", true, false).text = "Not recruitable here yet"
            continue
        card.find_child("Available", true, false).text = "Available %d · can afford %d" % [have, afford]
        _amounts[id].max_value = afford
        _amounts[id].value = mini(int(_amounts[id].value), afford)
    if _buildings != null:
        for child in _buildings.get_children():
            _buildings.remove_child(child)
            child.queue_free()
        var town := _town()
        # What you can build now first, then what is locked, then what stands.
        var rank := func(def: Dictionary) -> int:
            if town.get("buildings", []).has(def.id): return 2
            return 0 if str(town.get("blockers", {}).get(def.id, "x")).is_empty() else 1
        var defs: Array = state.get("building_defs", []).duplicate()
        var order := {}
        for i in defs.size(): order[defs[i].id] = i
        defs.sort_custom(func(a, b):
            var ra: int = rank.call(a)
            var rb: int = rank.call(b)
            return ra < rb if ra != rb else order[a.id] < order[b.id])
        for def in defs: _buildings.add_child(_building_card(def, town))
        var growth := int(round(float(town.get("growth_bonus", 0.0)) * 100.0))
        var today: String = town.get("built_today_name", "")
        _build_note.text = "%s   ·   Town income %d gold a day   ·   Recruit growth +%d%%" % [
            ("Built today: %s — next building tomorrow" % today) if not today.is_empty() else "One building a day: ready",
            int(town.get("gold", 0)), growth]
    _update_quote()
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

func _wrapped_to(text: String, size: int, color: Color, width: float) -> Label:
    var label := _label(text, size, color)
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.custom_minimum_size = Vector2(width, 0)
    return label

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
