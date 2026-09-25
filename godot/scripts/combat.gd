extends Control

signal finished(result: Dictionary)
const Board = preload("res://scripts/combat_board.gd")
const TEXT := Color("e8d8b3")
const DIM := Color("a8977a")
const FRIEND := Color("9fd4b4")
const FOE := Color("ff9f86")
const GOLD := Color("f7d580")
var bridge: RefCounted
var state: Dictionary = {}
var units: Dictionary = {}   # key -> unit dictionary of the current state
var busy := true
var animation_speed := 1.0
var auto_battle := false
var _returned := false
var _ai_scheduled := false
var board: Control
var heading: Label
var turn_label: Label
var initiative: HBoxContainer
var status: RichTextLabel
## Red line at the top of the board: which companions the enemy can reach.
var warning: RichTextLabel
var inspection: RichTextLabel
var log_view: RichTextLabel
var defend: Button
var retreat: Button
var auto_button: CheckButton
var return_button: Button
var confirm_retreat: ConfirmationDialog
## Plain-text combat log (newest last); the RichTextLabel shows it coloured.
var history: Array[String] = []
var _pinned_key := ""
var _hover_key := ""
var _hover_cell: Array = []
var _last_round := 1
var _pointer := Vector2.ZERO
## Move-and-attack option picked for the hovered target (standing hex, path, forecast).
var _stand: Dictionary = {}
## Ctrl/Shift+click waypoints: the active stack walks through them in order.
var waypoints: Array = []
## Pointer over the Defend button: the status line explains the stance.
var _defend_hover := false
var begin_button: Button
var _fieldwork_box: VBoxContainer
var _fieldwork_kind := "barricade"
var _orders: Array = []   # Tactics: keys of your stacks that act first, in order

func _ready() -> void:
    mouse_filter = Control.MOUSE_FILTER_STOP
    size = Vector2(1280, 800)
    var background := ColorRect.new()
    background.color = Color("191610")
    background.size = size
    background.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(background)
    _panel(Rect2(24, 10, 1232, 142))
    _panel(Rect2(24, 160, 750, 456))
    _panel(Rect2(24, 622, 750, 40))
    _panel(Rect2(808, 160, 448, 502))
    _panel(Rect2(24, 670, 1232, 120))
    heading = _label(Vector2(40, 16), Vector2(560, 34), 24)
    heading.text = "THE SEALED TOMB"
    turn_label = _label(Vector2(600, 18), Vector2(640, 30), 18)
    turn_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    var order_caption := _label(Vector2(40, 54), Vector2(200, 20), 12)
    order_caption.text = "TURN ORDER  (left acts first)"
    order_caption.add_theme_color_override("font_color", DIM)
    initiative = HBoxContainer.new()
    initiative.position = Vector2(40, 74)
    initiative.size = Vector2(1200, 74)
    initiative.add_theme_constant_override("separation", 6)
    add_child(initiative)
    board = Board.new()
    board.position = Vector2(28, 162)
    board.size = Vector2(750, 450)
    add_child(board)
    board.cell_clicked.connect(_cell_clicked)
    board.cell_right_clicked.connect(_cell_right_clicked)
    board.cell_hovered.connect(_cell_hovered)
    board.pointer_moved.connect(_pointer_moved)
    status = _rich(Vector2(36, 630), Vector2(730, 28), 15)
    status.scroll_active = false
    warning = _rich(Vector2(36, 164), Vector2(730, 24), 14)
    warning.scroll_active = false
    inspection = _rich(Vector2(826, 170), Vector2(415, 262), 16)
    inspection.scroll_active = false
    defend = _button("Defend · D   (+25% defence until its next turn)", Vector2(830, 440), _defend)
    defend.mouse_entered.connect(func():
        _defend_hover = true
        _update_status())
    defend.mouse_exited.connect(func():
        _defend_hover = false
        _update_status())
    _fieldwork_box = VBoxContainer.new()
    _fieldwork_box.position = Vector2(830, 341)
    _fieldwork_box.size = Vector2(400, 84)
    _fieldwork_box.add_theme_constant_override("separation", 8)
    add_child(_fieldwork_box)
    for kind in ["barricade", "stakes"]:
        var button := Button.new()
        button.name = kind
        button.custom_minimum_size.y = 36
        button.pressed.connect(func():
            _fieldwork_kind = kind
            _refresh())
        _fieldwork_box.add_child(button)
    _fieldwork_box.hide()
    begin_button = _button("Begin the battle · Enter", Vector2(830, 440), give_orders)
    begin_button.hide()
    retreat = _button("Retreat…", Vector2(830, 494), func(): confirm_retreat.popup_centered())
    auto_button = CheckButton.new()
    auto_button.text = "Auto-battle (AI commands your stacks)"
    auto_button.position = Vector2(830, 548)
    auto_button.size = Vector2(400, 40)
    auto_button.add_theme_color_override("font_color", TEXT)
    auto_button.toggled.connect(func(enabled: bool):
        auto_battle = enabled
        _refresh()
        _schedule_ai())
    add_child(auto_button)
    return_button = _button("Return to the desert", Vector2(830, 598), _return)
    return_button.hide()
    log_view = _rich(Vector2(40, 676), Vector2(1200, 108), 15)
    log_view.scroll_following = true
    confirm_retreat = ConfirmationDialog.new()
    confirm_retreat.title = "Retreat from battle?"
    confirm_retreat.dialog_text = "Keep your surviving units, but leave the dungeon uncleared.\nYou receive no loot. Casualties are permanent for this expedition."
    confirm_retreat.confirmed.connect(func(): issue("retreat"))
    confirm_retreat.canceled.connect(_schedule_ai)
    add_child(confirm_retreat)
    get_viewport().size_changed.connect(_resize)
    _resize()

func _resize() -> void:
    var viewport_size := get_viewport_rect().size
    scale = Vector2.ONE * minf(viewport_size.x / 1280.0, viewport_size.y / 800.0)
    position = (viewport_size - size * scale) / 2.0

func _label(at: Vector2, dimensions: Vector2, font_size: int) -> Label:
    var label := Label.new()
    label.position = at
    label.size = dimensions
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.mouse_filter = Control.MOUSE_FILTER_IGNORE
    label.add_theme_font_size_override("font_size", font_size)
    label.add_theme_color_override("font_color", TEXT)
    add_child(label)
    return label

func _rich(at: Vector2, dimensions: Vector2, font_size: int) -> RichTextLabel:
    var label := RichTextLabel.new()
    label.position = at
    label.size = dimensions
    label.bbcode_enabled = true
    label.mouse_filter = Control.MOUSE_FILTER_IGNORE
    for font in ["normal_font_size", "bold_font_size"]:
        label.add_theme_font_size_override(font, font_size)
    label.add_theme_color_override("default_color", TEXT)
    add_child(label)
    return label

func _button(text: String, at: Vector2, action: Callable) -> Button:
    var button := Button.new()
    button.text = text
    button.position = at
    button.size = Vector2(400, 46)
    var style := StyleBoxFlat.new()
    style.bg_color = Color("392b1c")
    style.border_color = Color("806037")
    style.set_border_width_all(1)
    button.add_theme_stylebox_override("normal", style)
    var hover := style.duplicate() as StyleBoxFlat
    hover.bg_color = Color("51402b")
    button.add_theme_stylebox_override("hover", hover)
    button.add_theme_color_override("font_color", TEXT)
    button.pressed.connect(action)
    add_child(button)
    return button

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

func begin(army: Array, encounter: Dictionary, title: String) -> bool:
    heading.text = title.to_upper()
    if not ClassDB.class_exists("UmmCombat"):
        _log("Native combat is missing. Run scripts/build_godot_combat.sh and restart Godot.")
        return false
    bridge = ClassDB.instantiate("UmmCombat")
    var reply := _decode_reply(bridge.begin_battle(ProjectSettings.globalize_path("res://content/data"), JSON.stringify(army), JSON.stringify(encounter)))
    if not reply.get("ok", false):
        _log(str(reply.get("error", "Could not start combat.")))
        return false
    _set_state(reply.state)
    board.sync(state)
    _log("Battle begins. Click an enemy to walk up and strike it in one turn — where your cursor sits around the target picks the side you attack from (strike opposite an ally to PIN: +50%, no retaliation). Clicking a green hex only moves. Archers shoot anyone, with no retaliation, but a stack in the line of fire halves the shot, and an archer with an enemy next to it cannot shoot: it must fight hand to hand, at half damage. Companions (gold ring) hit hard and fall fast: their aura shields nearby troops, and a stack beside them takes half of every melee blow.", DIM)
    _consume(reply)
    return true

func issue(action: String, cell := Vector2i.ZERO, from := Vector2i.ZERO) -> bool:
    if busy or state.is_empty() or state.result != "ongoing": return false
    var raw: String = bridge.strike(cell.x, cell.y, from.x, from.y) if action == "strike" else bridge.act(action, cell.x, cell.y)
    var reply := _decode_reply(raw)
    if not reply.get("ok", false):
        _log(str(reply.get("error", "Action rejected.")), FOE)
        return false
    _clear_route()
    _consume(reply)
    return true

## Walk a chosen route; "strike" then attacks the enemy on `cell` from its end.
func issue_route(action: String, route: Array, cell := Vector2i.ZERO) -> bool:
    if busy or state.is_empty() or state.result != "ongoing": return false
    var reply := _decode_reply(bridge.route(action, JSON.stringify(route), cell.x, cell.y))
    if not reply.get("ok", false):
        _log(str(reply.get("error", "Route rejected.")), FOE)
        return false
    _clear_route()
    _consume(reply)
    return true

func _decode_reply(raw: String) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(raw)
    if reply.get("ok", false):
        # Godot's JSON parser makes every number a float. Array equality is
        # type-sensitive, so normalize hex coordinates at this boundary.
        var cells: Array = reply.state.reachable + reply.state.attackable
        for entry in reply.state.get("reaction_fire", []): cells.append(entry.cell)
        for unit in reply.state.units: cells.append(unit.cell)
        for work in reply.state.get("fieldworks", []): cells.append(work.cell)
        for preview in reply.state.get("previews", []):
            cells.append(preview.cell)
            if preview.has("best"): cells.append(preview.best)
            for option in preview.get("options", []):
                cells.append(option.from)
                cells.append_array(option.path)
        for cell in cells:
            cell[0] = int(cell[0])
            cell[1] = int(cell[1])
    return reply

func _set_state(next: Dictionary) -> void:
    state = next
    units.clear()
    for unit in state.get("units", []): units[unit.key] = unit

func _consume(reply: Dictionary) -> void:
    busy = true
    board.set_hover("", "")
    _refresh()
    var strike := {}   # the attack event whose damage comes next
    for event in reply.events:
        match event.type:
            "attack": strike = event
            "damage": _log_damage(event, strike)
            "move": _log("%s moves." % _unit_label(event.unit), DIM, event.unit)
            "defend": _log("%s defends: +25%% defence until its next turn (the stance carries into the next round)." % _unit_label(event.unit), DIM, event.unit)
        await board.animate(event, animation_speed)
    _set_state(reply.state)
    board.sync(state)
    if not bridge.acknowledge(int(reply.ticket)):
        _log("Combat synchronization failed. Orders remain locked.", FOE)
        return
    busy = false
    if state.get("round", 1) > _last_round and state.result == "ongoing":
        _last_round = state.round
        _log("— Round %d —" % _last_round, GOLD)
    _refresh()
    if state.result != "ongoing":
        auto_battle = false
        auto_button.set_pressed_no_signal(false)
        _show_result()
    else:
        _schedule_ai()

func _log_damage(event: Dictionary, strike: Dictionary) -> void:
    var victim: String = event.unit
    var kills := int(event.get("kills", 0))
    var left := int(event.get("remaining", 0))
    var outcome := "%d damage" % event.damage
    if kills > 0: outcome += ", %d slain" % kills
    outcome += " — stack destroyed" if left == 0 else " (%d left)" % left
    if event.get("bodyguard", false) and not strike.is_empty():
        _log("   » %s steps in front of %s and takes %s." % [_unit_label(victim), _unit_name(strike.target), outcome], TEXT, victim)
    elif strike.is_empty() or strike.target != victim:
        _log("%s takes %s." % [_unit_label(victim), outcome], TEXT, victim)
    elif strike.retaliation:
        _log("   » retaliation: %s strikes back at %s: %s." % [_unit_label(strike.unit), _unit_label(victim), outcome], TEXT, strike.unit)
    else:
        var verb := "shoots" if strike.get("ranged", false) else "attacks"
        if strike.get("reaction", false): verb = "fires at the approaching"
        if strike.get("opportunity", false):
            _log("%s strikes %s as it steps out of reach: %s." % [_unit_label(strike.unit), _unit_label(victim), outcome], TEXT, strike.unit)
            return
        var pinned := " PINNED (+50%, no retaliation)" if strike.flanked else ""
        if strike.get("blocked", false): pinned += " through the ranks (blocked shot: half damage)"
        _log("%s %s %s%s: %s." % [_unit_label(strike.unit), verb, _unit_label(victim), pinned, outcome], TEXT, strike.unit)

func _unit_name(key: String) -> String:
    return units[key].name if units.has(key) else key

func _unit_label(key: String) -> String:
    if not units.has(key): return key
    if units[key].get("companion", false): return "%s (level %d)" % [units[key].name, int(units[key].level)]
    return "%s ×%d" % [units[key].name, units[key].count]

## "Ushari can be reached by Brigands ×12, Grey Wolves ×8" for every exposed companion.
func _companion_warnings() -> String:
    var lines: Array[String] = []
    for unit in state.get("units", []):
        if not unit.player or not unit.get("companion", false) or int(unit.count) <= 0: continue
        var threats: Array = unit.get("threats", [])
        if threats.is_empty(): continue
        var names: Array[String] = []
        for key in threats: names.append(_unit_label(key))
        var guard := "" if str(unit.get("bodyguard", "")).is_empty() else " (a bodyguard takes half of melee blows)"
        lines.append("%s is exposed to %s%s" % [unit.name, ", ".join(names), guard])
    if lines.is_empty(): return ""
    return "[color=#%s]⚠ %s.[/color]" % [Board.DANGER_COLOR.to_html(false), "; ".join(lines)]

func _refresh() -> void:
    board.locked = busy or auto_battle or _opening() > 0 or _deploying()   # no move range while orders are given
    board.queue_redraw()
    var ongoing: bool = state.get("result", "") == "ongoing"
    if ongoing:
        var player_turn: bool = state.get("player_turn", false)
        var whose := "YOUR TURN" if player_turn and not auto_battle else ("AUTO-BATTLE" if player_turn else "ENEMY TURN")
        if busy: whose = "resolving…"
        if _deploying():
            turn_label.text = "BEFORE BATTLE   ·   PLACE YOUR FIELDWORKS"
            turn_label.add_theme_color_override("font_color", GOLD)
        elif _opening() > 0 and not auto_battle:
            turn_label.text = "ROUND 1   ·   OPENING ORDERS"
            turn_label.add_theme_color_override("font_color", GOLD)
        else:
            turn_label.text = "ROUND %d   ·   %s   ·   %s" % [state.get("round", 1), _unit_label(state.get("active", "")), whose]
            turn_label.add_theme_color_override("font_color", FRIEND if player_turn else FOE)
    _build_initiative()
    defend.disabled = busy or auto_battle or not state.get("player_turn", false) or not ongoing
    begin_button.visible = (_opening() > 0 or _deploying()) and ongoing
    begin_button.text = "Finish placement · Enter" if _deploying() else "Begin the battle · Enter"
    _fieldwork_box.visible = _deploying() and ongoing
    if int(state.get("fieldwork_stock", {}).get(_fieldwork_kind, 0)) == 0:
        _fieldwork_kind = "stakes" if int(state.get("fieldwork_stock", {}).get("stakes", 0)) > 0 else "barricade"
    for button in _fieldwork_box.get_children():
        var kind: String = button.name
        var stock := int(state.get("fieldwork_stock", {}).get(kind, 0))
        var used := 0
        for work in state.get("fieldworks", []):
            if work.kind == kind: used += 1
        button.text = ("› " if kind == _fieldwork_kind else "") + kind.capitalize() + "  ·  %d / %d placed" % [used, stock]
        button.disabled = busy or stock == 0
        button.tooltip_text = "Blocks movement and all arrows." if kind == "barricade" else "Blocks movement; arrows pass over it."
    defend.visible = not begin_button.visible
    begin_button.disabled = busy or auto_battle
    retreat.disabled = busy or not ongoing
    auto_button.disabled = not ongoing
    _update_inspection()
    _update_status()

# ── Turn order bar ────────────────────────────────────────────────────────────

func _build_initiative() -> void:
    for child in initiative.get_children():
        initiative.remove_child(child)
        child.queue_free()
    if state.get("result", "") != "ongoing": return
    var slots := 0
    for slot in state.get("initiative", []):
        if slot.acted or not units.has(slot.key): continue
        initiative.add_child(_portrait(slot.key, slot.key == state.active))
        slots += 1
    var next: Array = state.get("next_round", [])
    if not next.is_empty() and slots < 16:
        var divider := Label.new()
        divider.text = "ROUND\n%d" % (int(state.round) + 1)
        divider.custom_minimum_size = Vector2(54, 70)
        divider.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
        divider.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
        divider.add_theme_font_size_override("font_size", 12)
        divider.add_theme_color_override("font_color", DIM)
        initiative.add_child(divider)
        for key in next:
            if slots >= 16: break
            initiative.add_child(_portrait(key, false, true))
            slots += 1

func _portrait(key: String, active: bool, upcoming := false) -> Control:
    var unit: Dictionary = units[key]
    var frame := Panel.new()
    frame.custom_minimum_size = Vector2(62, 70)
    frame.tooltip_text = "%s ×%d · %s · speed %d" % [unit.name, unit.count, "yours" if unit.player else "enemy", unit.speed]
    var style := StyleBoxFlat.new()
    style.bg_color = Color("26372c") if unit.player else Color("3f2320")
    style.border_color = GOLD if active else (FRIEND if unit.player else FOE)
    style.set_border_width_all(3 if active else 1)
    frame.add_theme_stylebox_override("panel", style)
    frame.modulate = Color(1, 1, 1, 0.62) if upcoming else Color.WHITE
    var picture := TextureRect.new()
    picture.texture = Board.unit_texture(unit.id)
    picture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    picture.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    picture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    picture.position = Vector2(4, 2)
    picture.size = Vector2(54, 48)
    picture.flip_h = not unit.player
    picture.mouse_filter = Control.MOUSE_FILTER_IGNORE
    frame.add_child(picture)
    var count := Label.new()
    count.text = ("♥%d" % int(unit.hp)) if unit.get("companion", false) else "×%d" % unit.count
    count.position = Vector2(0, 48)
    count.size = Vector2(62, 20)
    count.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    count.add_theme_font_size_override("font_size", 13)
    count.add_theme_color_override("font_color", FRIEND if unit.player else FOE)
    count.mouse_filter = Control.MOUSE_FILTER_IGNORE
    frame.add_child(count)
    frame.mouse_entered.connect(func():
        board.highlight_key = key
        _hover_key = key
        board.queue_redraw()
        _update_inspection())
    frame.mouse_exited.connect(func():
        board.highlight_key = ""
        _hover_key = ""
        board.queue_redraw()
        _update_inspection())
    return frame

# ── Stack inspection ──────────────────────────────────────────────────────────

func _update_inspection() -> void:
    if _deploying():
        inspection.text = "[color=#f7d580][b]FIELD ENGINEERING[/b][/color]\n\nChoose a fieldwork, then click a highlighted hex. Right-click a placed piece to recover it.\n\nBarricades block everyone's arrows. Stakes block movement only. Equipment is reused next battle."
        return
    if state.get("result", "") != "ongoing" and not state.is_empty(): return
    for key in [_hover_key, _pinned_key, state.get("active", "")]:
        if units.has(key) and int(units[key].count) > 0:
            _inspect(units[key])
            return

func _inspect(unit: Dictionary) -> void:
    var colour := FRIEND if unit.player else FOE
    var lines: Array[String] = []
    var acting := "FIRST TO ACT, unless your orders say otherwise" if _opening() > 0 else "ACTING NOW"
    var tag := acting if unit.key == state.get("active", "") else ("kept on screen · right-click empty ground to release" if unit.key == _pinned_key else "")
    var companion: bool = unit.get("companion", false)
    if companion:
        lines.append("[font_size=21][b][color=#%s]%s[/color][/b][/font_size]" % [GOLD.to_html(false), unit.name])
        lines.append("[color=#%s]Companion · level %d%s[/color]" % [DIM.to_html(false), int(unit.level), ("  ·  " + tag) if not tag.is_empty() else ""])
        lines.append("Health  %d / %d  ·  one figure: when it falls, it is out of the fight" % [unit.hp, unit.unit_hp])
    else:
        lines.append("[font_size=21][b][color=#%s]%s ×%d[/color][/b][/font_size]" % [colour.to_html(false), unit.name, unit.count])
        lines.append("[color=#%s]%s%s[/color]" % [DIM.to_html(false), "Your expedition" if unit.player else "Enemy", ("  ·  " + tag) if not tag.is_empty() else ""])
        lines.append("Health  %d / %d on the top creature  ·  %d total" % [unit.get("hp_left", unit.unit_hp), unit.unit_hp, unit.hp])
    lines.append("Attack %d  ·  Defence %d  ·  Damage %d–%d each" % [unit.attack, unit.defense, unit.get("min_damage", 0), unit.get("max_damage", 0)])
    lines.append("Speed %d  ·  Moves %d hexes" % [unit.speed, unit.get("move", 0)])
    if unit.ranged and unit.get("engaged", false):
        lines.append("[color=#%s]ENGAGED: an enemy is next to it — it cannot shoot, only fight hand to hand (half damage)[/color]" % FOE.to_html(false))
    elif unit.ranged: lines.append("Ranged  ·  %d / %d shots  ·  no retaliation when shooting  ·  half damage hand to hand" % [unit.shots, unit.get("shots_max", unit.shots)])
    if unit.get("readied_shot", false) and int(unit.get("shots", 0)) > 0:
        lines.append("[color=#%s]Readied shot: fires once a round at an enemy moving closer[/color]" % GOLD.to_html(false))
    else: lines.append("Melee  ·  attacks adjacent stacks only")
    if int(unit.get("aura_radius", 0)) > 0:
        lines.append("[color=#%s]Aura: stacks within %d hex get +%d defence while %s stands[/color]" % [GOLD.to_html(false), int(unit.aura_radius), int(unit.aura_defense), unit.name])
    if int(unit.get("aura_bonus", 0)) > 0:
        lines.append("[color=#%s]+%d defence from a companion's aura[/color]" % [GOLD.to_html(false), int(unit.aura_bonus)])
    if companion:
        var guard: String = unit.get("bodyguard", "")
        lines.append("Bodyguard: %s" % (_unit_label(guard) + " takes half of each melee blow" if not guard.is_empty() else "none — keep a stack beside %s" % unit.name))
    var notes: Array[String] = []
    if unit.defending: notes.append("DEFENDING: +25% defence until its next turn")
    notes.append("retaliation used this round" if unit.get("retaliated", false) else "will retaliate once this round")
    var acted := false
    for slot in state.get("initiative", []):
        if slot.key == unit.key: acted = slot.acted
    if acted: notes.append("has acted")
    lines.append("[color=#%s]%s[/color]" % [DIM.to_html(false), " · ".join(notes)])
    var abilities: Array = unit.get("abilities", [])
    if not abilities.is_empty():
        var names: Array[String] = []
        for ability in abilities: names.append(str(ability).replace("_", " "))
        lines.append("[color=#%s]Traits: %s[/color]" % [DIM.to_html(false), ", ".join(names)])
    inspection.text = "\n".join(lines)

# ── Hover forecast ────────────────────────────────────────────────────────────

func _cell_hovered(cell: Vector2i, inside: bool) -> void:
    _hover_cell = [cell.x, cell.y] if inside else []
    board.hover_cell = _hover_cell
    var unit := _unit_at(_hover_cell)
    _hover_key = unit.key if not unit.is_empty() else ""
    _stand = _pick_stand()
    _update_inspection()
    _update_status()

func _pointer_moved(point: Vector2) -> void:
    _pointer = point
    if board.hover_kind == "attack" and _hover_options().size() > 1:
        var before := _stand
        _stand = _pick_stand()
        if _stand != before: _update_status()

# ── Routes (waypoints) ────────────────────────────────────────────────────────

func _occupied() -> Dictionary:
    var taken := {}
    for work in state.get("fieldworks", []): taken[Vector2i(work.cell[0], work.cell[1])] = true
    for unit in state.get("units", []):
        if int(unit.count) > 0 and unit.key != state.get("active", ""): taken[Vector2i(unit.cell[0], unit.cell[1])] = true
    return taken

static func _neighbours(cell: Vector2i) -> Array:
    var out := []
    for d in [Vector2i(1, 0), Vector2i(1, -1), Vector2i(0, -1), Vector2i(-1, 0), Vector2i(-1, 1), Vector2i(0, 1)]:
        var n: Vector2i = cell + d
        var col := n.x
        var row := n.y + (col - (col & 1)) / 2
        if col >= 0 and col < 11 and row >= 0 and row < 5: out.append(n)
    return out

## Shortest free path from `from` to `to` (cells after `from`), avoiding
## stacks and the hexes in `used`; [] when there is none.
func _segment(from: Vector2i, to: Vector2i, used: Dictionary) -> Array:
    if from == to: return []
    var blocked := _occupied()
    var previous := {from: from}
    var queue: Array[Vector2i] = [from]
    while not queue.is_empty():
        var current: Vector2i = queue.pop_front()
        if current == to: break
        for next in _neighbours(current):
            if previous.has(next) or blocked.has(next) or used.has(next): continue
            previous[next] = current
            queue.append(next)
    if not previous.has(to): return []
    var path := []
    var cell := to
    while cell != from:
        path.push_front([cell.x, cell.y])
        cell = previous[cell]
    return path

## The route through the waypoints to `dest` (cells after the active hex),
## or [] if it cannot be walked; its length may exceed the move range.
func _route_to(dest: Array) -> Array:
    var active: Dictionary = units.get(state.get("active", ""), {})
    if active.is_empty(): return []
    var at := Vector2i(active.cell[0], active.cell[1])
    var used := {at: true}
    var route := []
    for stop in waypoints + [dest]:
        var target := Vector2i(stop[0], stop[1])
        if target == at: continue
        var leg := _segment(at, target, used)
        if leg.is_empty(): return []
        for cell in leg: used[Vector2i(cell[0], cell[1])] = true
        route.append_array(leg)
        at = target
    return route

func _move_range() -> int:
    return int(units.get(state.get("active", ""), {}).get("move", 0))

## Hexes still reachable after walking through the waypoints.
func _route_reach() -> Array:
    if waypoints.is_empty(): return []
    var so_far := _route_to(waypoints[-1])
    var left := _move_range() - so_far.size()
    var used := {}
    var active: Dictionary = units.get(state.get("active", ""), {})
    used[Vector2i(active.cell[0], active.cell[1])] = true
    for cell in so_far: used[Vector2i(cell[0], cell[1])] = true
    var blocked := _occupied()
    var out := []
    var frontier: Array = [Vector2i(waypoints[-1][0], waypoints[-1][1])]
    for step in left:
        var next_frontier := []
        for cell in frontier:
            for n in _neighbours(cell):
                if used.has(n) or blocked.has(n): continue
                used[n] = true
                out.append([n.x, n.y])
                next_frontier.append(n)
        frontier = next_frontier
    return out

func _clear_route() -> void:
    waypoints.clear()
    board.waypoints = waypoints
    board.route_reach = []

## Ctrl/Shift+click: add a waypoint (or drop it and those after it if clicked again).
func _toggle_waypoint(cell: Array) -> void:
    var index := waypoints.find(cell)
    if index >= 0:
        waypoints = waypoints.slice(0, index)
    else:
        if not _unit_at(cell).is_empty(): return
        var route := _route_to(cell)
        if route.is_empty() or route.size() > _move_range():
            _log("That waypoint is out of reach.", FOE)
            return
        waypoints.append(cell)
    board.waypoints = waypoints
    board.route_reach = _route_reach()
    _stand = _pick_stand()
    _update_status()

## Options (standing hexes) for attacking the hovered enemy; empty if none.
## With waypoints, only standing hexes the route can reach, carrying that route.
func _hover_options() -> Array:
    var unit := _unit_at(_hover_cell)
    if unit.is_empty(): return []
    for preview in state.get("previews", []):
        if preview.key != unit.key: continue
        if waypoints.is_empty(): return preview.get("options", [])
        var routed := []
        for option in preview.get("options", []):
            if option.get("ranged", false): continue
            var route := _route_to(option.from)
            if route.is_empty() or route.size() > _move_range(): continue
            var copy: Dictionary = option.duplicate()
            copy.path = route
            routed.append(copy)
        return routed
    return []

## HoMM3 sword cursor: the standing hex is the target's neighbour on the side
## the pointer sits on; if that side is not legal, the closest legal side.
func _pick_stand() -> Dictionary:
    var options := _hover_options()
    if options.is_empty(): return {}
    if options.size() == 1: return options[0]
    var centre := Board.cell_point(_hover_cell)
    var toward := _pointer - centre
    var best: Dictionary = options[0]
    var best_score := -INF
    for option in options:
        var side := Board.cell_point(option.from) - centre
        var score := toward.normalized().dot(side.normalized()) if toward.length() > 3.0 else -side.length()
        if score > best_score:
            best_score = score
            best = option
    if toward.length() <= 3.0:
        # Pointer dead centre: use the engine's own pick (pin first, then shortest walk).
        for preview in state.get("previews", []):
            if preview.cell == _hover_cell:
                for option in options:
                    if option.from == preview.best: best = option
    return best

func _unit_at(cell: Array) -> Dictionary:
    if cell.is_empty(): return {}
    for unit in state.get("units", []):
        if unit.cell == cell and int(unit.count) > 0: return unit
    return {}

func _range_text(low: int, high: int) -> String:
    return str(low) if low == high else "%d–%d" % [low, high]

func _update_status() -> void:
    var text := ""
    var kind := ""
    var label := ""
    var ongoing: bool = state.get("result", "") == "ongoing"
    var my_turn: bool = ongoing and not busy and not auto_battle and state.get("player_turn", false)
    var unit := _unit_at(_hover_cell)
    var active: Dictionary = units.get(state.get("active", ""), {})
    if not ongoing:
        text = "The battle is over."
    elif _deploying():
        text = "[b]FIELDWORKS[/b]: click highlighted hex to place · right-click to recover · Enter to finish"
        kind = "move" if not _hover_cell.is_empty() and int(_hover_cell[0]) >= 2 and int(_hover_cell[0]) <= 4 else "blocked"
    elif _opening() > 0 and not auto_battle:
        var chosen: Array[String] = []
        for key in _orders: chosen.append(_unit_name(key))
        text = "[b]TACTICS[/b]: click up to %d stack%s to act first · %s · Enter" % [
            _opening(), "" if _opening() == 1 else "s", (" → ".join(chosen)) if not chosen.is_empty() else "none: usual order"]
    elif not my_turn:
        if auto_battle: text = "Auto-battle: the AI commands your stacks. Untick Auto-battle to take over."
        elif busy: text = "Resolving…"
        else: text = "Enemy turn — %s is deciding." % _unit_label(state.get("active", ""))
        if not unit.is_empty(): text = "%s (%s) — right-click to keep its details on screen." % [_unit_label(unit.key), "yours" if unit.player else "enemy"]
    elif _hover_cell.is_empty():
        var how := "shoot" if active.get("ranged", false) and int(active.get("shots", 0)) > 0 and not active.get("engaged", false) else "strike"
        text = "%s: click enemy = %s · green hex = move · [b]Ctrl+click green = waypoint[/b] · D = defend" % [_unit_name(state.active), how]
        if not waypoints.is_empty():
            text = "Route: %d waypoint%s · click green hex or enemy to go · Esc clears" % [waypoints.size(), "" if waypoints.size() == 1 else "s"]

    elif not unit.is_empty() and not unit.player:
        var preview := {}
        for candidate in state.get("previews", []):
            if candidate.key == unit.key: preview = candidate
        if not _stand.is_empty() and _stand_matches(preview): preview = _stand
        if not waypoints.is_empty() and not preview.is_empty() and _hover_options().is_empty() and not preview.ranged:
            kind = "blocked"
            text = "[color=#%s]Your route cannot reach a side of %s this turn[/color] — move a waypoint, or clear the route (Esc)." % [FOE.to_html(false), _unit_label(unit.key)]
        elif preview.is_empty():
            kind = "blocked"
            text = "[color=#%s]%s is out of reach this turn[/color] — %s can walk %d hexes and then strike an adjacent enemy." % [FOE.to_html(false), _unit_label(unit.key), _unit_name(state.active), active.get("move", 0)]
        else:
            kind = "attack"
            var damage := _range_text(int(preview.damage_min), int(preview.damage_max))
            var kills := _range_text(int(preview.kills_min), int(preview.kills_max))
            var verb := "Shoot" if preview.ranged else ("Walk up and attack" if not preview.get("path", []).is_empty() else "Attack")
            text = "[b]%s %s[/b]: %s damage, kills %s of %d" % [verb, _unit_label(unit.key), damage, kills, unit.count]
            label = "%s dmg · %s slain" % [damage, kills]
            var incoming: Array = preview.get("reactions", [])
            if not incoming.is_empty():
                text = _reaction_text(incoming).trim_prefix(" · ") + " · " + text
                label = "⚠ volley first · " + label
            if unit.get("defending", false):
                text += " · [color=#%s]it is defending (+25%% defence)[/color]" % FRIEND.to_html(false)
                label += " · defending"
            if preview.pinned:
                text += " · [color=#%s]PINNED: +50%%, no retaliation[/color]" % GOLD.to_html(false)
                label = "PINNED +50%  ·  " + label
            elif not preview.ranged and _pin_spots().size() > 0:
                text += " · [color=#%s]orange-marked sides would PIN it (+50%%)[/color]" % GOLD.to_html(false)
            if preview.get("guarded", false):
                text += " · [color=#%s]a bodyguard takes %s of the blow[/color]" % [GOLD.to_html(false), _range_text(int(preview.guard_min), int(preview.guard_max))]
                label += " · shielded"
            if preview.ranged and preview.get("blocked", false):
                text += " · [color=#%s]BLOCKED SHOT: half damage[/color] (a stack is in the line of fire)" % FOE.to_html(false)
                label += " · blocked ½"
            elif preview.ranged: text += " · clear line of sight: full damage, no retaliation"
            elif preview.retaliation:
                text += " · [color=#%s]they strike back for %s (kills %s)[/color]" % [FOE.to_html(false), _range_text(int(preview.retaliation_min), int(preview.retaliation_max)), _range_text(int(preview.retaliation_kills_min), int(preview.retaliation_kills_max))]
            elif int(preview.kills_min) >= int(unit.count): text += " · wipes out the stack"
            elif unit.get("retaliated", false): text += " · no retaliation: already used this round"
            else: text += " · no retaliation"
    elif not unit.is_empty():
        text = "%s — %s." % [_unit_label(unit.key), "acting now" if unit.key == state.active else "your stack; right-click for details"]
    elif (_hover_cell in state.get("reachable", [])) if waypoints.is_empty() else (_hover_cell in board.route_reach or _hover_cell == waypoints[-1]):
        kind = "move"
        text = "Move here without attacking — this ends %s's turn." % _unit_name(state.active)
        if not waypoints.is_empty():
            text = "Move along your route (%d of %d hexes, %d waypoint%s) — this ends %s's turn." % [_route_to(_hover_cell).size(), _move_range(), waypoints.size(), "" if waypoints.size() == 1 else "s", _unit_name(state.active)]
        var volley := _reactions_at(_hover_cell)
        if not volley.is_empty(): text = _reaction_text(volley).trim_prefix(" · ") + " · " + text
        var near: Array[String] = []
        for other in state.get("units", []):
            if not other.player and int(other.count) > 0 and not (other.ranged and int(other.shots) > 0) \
                    and _hex_distance(other.cell, _hover_cell) - 1 <= int(other.move):
                near.append(_unit_label(other.key))
        if not near.is_empty():
            text += " [color=#%s]In reach of %s: they can walk up and strike first.[/color]" % [FOE.to_html(false), ", ".join(near)]
    else:
        kind = "blocked"
        text = "Out of reach — %s moves up to %d hexes, and cannot pass through stacks." % [_unit_name(state.active), active.get("move", 0)]
    if _defend_hover and my_turn: text = _defend_explanation(active)
    status.text = text
    warning.text = _companion_warnings() if ongoing else ""
    var walking: bool = my_turn and kind == "attack" and not _stand.is_empty() and _stand_matches_cell()
    board.stand_cell = _stand.from if walking and not _stand.path.is_empty() else []
    board.walk_path = _stand.path if walking else []
    board.move_path = _route_to(_hover_cell) if my_turn and kind == "move" else []
    board.pin_spots = _pin_spots() if my_turn and kind == "attack" else []
    board.reaction_lines = []
    if my_turn and kind in ["move", "attack"]:
        var dest: Array = _hover_cell if kind == "move" else (_stand.get("from", []) as Array)
        var shots: Array = _reactions_at(dest) if kind == "move" else _stand.get("reactions", [])
        for shot in shots:
            if units.has(shot.key): board.reaction_lines.append([units[shot.key].cell, dest, shot.blocked])
    board.pin_stand = []
    board.pin_ally = []
    if my_turn and kind == "attack" and not _stand.is_empty() and _stand.get("pinned", false):
        # The ally on the far side: the hex mirrored through the target.
        var mirror := [2 * int(_hover_cell[0]) - int(_stand.from[0]), 2 * int(_hover_cell[1]) - int(_stand.from[1])]
        var ally := _unit_at(mirror)
        if not ally.is_empty() and ally.player:
            board.pin_stand = _stand.from
            board.pin_ally = mirror
    board.shot_line = []
    if my_turn and kind == "attack" and not _stand.is_empty() and _stand.get("ranged", false):
        board.shot_line = [active.cell, _hover_cell]
        board.shot_blocked = _stand.get("blocked", false)
    board.set_hover(kind if my_turn else "", label if my_turn else "")

## Standing hexes (of those legal for this hover) that would pin the target.
func _pin_spots() -> Array:
    var out := []
    for option in _hover_options():
        if option.get("pinned", false): out.append(option.from)
    return out

func _stand_matches(preview: Dictionary) -> bool:
    for option in preview.get("options", []):
        if option.from == _stand.get("from", []): return true
    return false

func _stand_matches_cell() -> bool:
    for preview in state.get("previews", []):
        if preview.cell == _hover_cell: return _stand_matches(preview)
    return false

## Mirrors CombatEngine::damageMultiplier (without armour bypass).
static func _damage_multiplier(attack: int, defence: int) -> float:
    var diff := attack - defence
    if diff >= 0: return 1.0 + 0.05 * mini(diff, 20)
    return maxf(0.3, 1.0 + 0.025 * diff)

## What Defend would do for `unit`, in numbers, against the hardest-hitting enemy.
## Enemy shooters that would fire as the active stack walks to `cell`.
func _reactions_at(cell: Array) -> Array:
    for entry in state.get("reaction_fire", []):
        if entry.cell[0] == cell[0] and entry.cell[1] == cell[1]: return entry.shots
    return []

## " · ⚠ Brigand ×8 fires as you close in: 12–20" for a list of reaction shots.
func _reaction_text(shots: Array) -> String:
    if shots.is_empty(): return ""
    var parts: Array[String] = []
    var low := 0
    var high := 0
    var blows: Array[String] = []   # guardians striking a stack that steps out of reach
    for shot in shots:
        if shot.get("opportunity", false):
            blows.append("%s strikes as you step away: %s" % [_unit_label(shot.key), _range_text(int(shot.damage_min), int(shot.damage_max))])
            continue
        parts.append(_unit_label(shot.key) + (" (blocked ½)" if shot.blocked else ""))
        low += int(shot.damage_min)
        high += int(shot.damage_max)
    var text := ""
    if not blows.is_empty(): text += " · [color=#%s]⚠ %s[/color]" % [FOE.to_html(false), " · ".join(blows)]
    if not parts.is_empty():
        text += " · [color=#%s]⚠ %s fire%s as you close in: %s damage first[/color]" % [FOE.to_html(false), ", ".join(parts), "" if parts.size() > 1 else "s", _range_text(low, high)]
    return text

func _defend_explanation(unit: Dictionary) -> String:
    var defence := int(unit.get("defense", 0))
    var braced := defence + defence / 4
    var worst := {}
    for other in state.get("units", []):
        if not other.player and int(other.count) > 0 and (worst.is_empty() or int(other.attack) > int(worst.attack)): worst = other
    var less := ""
    if not worst.is_empty():
        var before := _damage_multiplier(int(worst.attack), defence)
        var after := _damage_multiplier(int(worst.attack), braced)
        less = " · ~%d%% less damage from %s" % [roundi(100.0 * (1.0 - after / before)), _unit_name(worst.key)]
    return "[b]Defend[/b]: defence %d → %d%s · lasts until its next turn · retaliation stays ready" % [defence, braced, less]

static func _hex_distance(a: Array, b: Array) -> int:
    var dq: int = int(a[0]) - int(b[0])
    var dr: int = int(a[1]) - int(b[1])
    return (absi(dq) + absi(dr) + absi(dq + dr)) / 2

# ── Input ─────────────────────────────────────────────────────────────────────

## Tactics rank still to use: how many of your stacks you may order to act first.
func _deploying() -> bool:
    return bool(state.get("deploying", false))

func _opening() -> int:
    return int(state.get("opening", 0))

## Click your own stacks to put them in (or take them out of) the opening orders.
func _toggle_order(coordinates: Array) -> void:
    var unit := _unit_at(coordinates)
    if unit.is_empty() or not unit.player: return
    if unit.key in _orders: _orders.erase(unit.key)
    elif _orders.size() < _opening(): _orders.append(unit.key)
    else:
        _log("Your Tactics allow %d opening order%s. Click a numbered stack to take it back." % [_opening(), "" if _opening() == 1 else "s"], FOE)
        return
    board.order_marks = {}
    for i in _orders.size(): board.order_marks[_orders[i]] = i + 1
    board.queue_redraw()
    _update_status()

## Sends the opening orders (none chosen = initiative as usual) and starts round 1.
func give_orders() -> bool:
    if _deploying(): return issue("deploy_done")
    if _opening() == 0: return false
    var route: Array = []
    for key in _orders: route.append(units[key].cell)
    var names: Array[String] = []
    for key in _orders: names.append(_unit_label(key))
    if issue_route("opening", route):
        if not names.is_empty(): _log("Opening orders: %s act%s first." % [", then ".join(names), "s" if names.size() == 1 else ""], GOLD)
        _orders.clear()
        board.order_marks = {}
        return true
    return false

func _cell_clicked(cell: Vector2i) -> void:
    var coordinates := [cell.x, cell.y]
    if _deploying():
        if not busy and not auto_battle: issue("place_" + _fieldwork_kind, cell)
        return
    if _opening() > 0:
        if not busy and not auto_battle: _toggle_order(coordinates)
        return
    if busy or auto_battle or not state.get("player_turn", false): return
    if board.modified_click or Input.is_key_pressed(KEY_SHIFT) or Input.is_key_pressed(KEY_CTRL):
        _toggle_waypoint(coordinates)
        return
    if not waypoints.is_empty():
        if coordinates in state.attackable and not _stand.is_empty() and coordinates == _hover_cell and _stand_matches_cell():
            issue_route("strike", _stand.path, cell)
        elif _unit_at(coordinates).is_empty():
            var route := _route_to(coordinates)
            if not route.is_empty() and route.size() <= _move_range(): issue_route("move", route)
            else: _log("Your route cannot reach that hex this turn.", FOE)
        return
    if coordinates in state.attackable:
        if coordinates == _hover_cell and not _stand.is_empty() and _stand_matches_cell():
            issue("strike", cell, Vector2i(_stand.from[0], _stand.from[1]))
        else:
            issue("attack", cell)   # engine picks: pin first, then the shortest walk
    elif coordinates in state.reachable: issue("move", cell)

func _cell_right_clicked(cell: Vector2i) -> void:
    if _deploying():
        if not busy and not auto_battle: issue("remove_fieldwork", cell)
        return
    var unit := _unit_at([cell.x, cell.y])
    _pinned_key = unit.key if not unit.is_empty() else ""
    if unit.is_empty() and not waypoints.is_empty():
        _clear_route()
        _stand = _pick_stand()
        _update_status()
    _update_inspection()

func _defend() -> void:
    if not defend.disabled: issue("defend")

func _unhandled_key_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo and (_opening() > 0 or _deploying()) \
            and event.keycode in [KEY_ENTER, KEY_KP_ENTER, KEY_SPACE]:
        give_orders()
        get_viewport().set_input_as_handled()
    elif event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_D:
        _defend()
        get_viewport().set_input_as_handled()
    elif event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_ESCAPE and not waypoints.is_empty():
        _clear_route()
        _stand = _pick_stand()
        _update_status()
        get_viewport().set_input_as_handled()

func _schedule_ai() -> void:
    if _ai_scheduled or busy or state.is_empty() or state.result != "ongoing": return
    if _deploying():
        if auto_battle: issue("deploy_done")
        return
    if _opening() > 0:              # nobody moves before the opening orders
        if auto_battle:
            _orders.clear()
            give_orders()           # the AI gives none: initiative as usual
        return
    if state.player_turn and not auto_battle: return
    _ai_scheduled = true
    await get_tree().create_timer(0.35 * animation_speed).timeout
    _ai_scheduled = false
    if confirm_retreat.visible: return
    if not busy and state.result == "ongoing" and (auto_battle or not state.player_turn): issue("ai")

func _show_result() -> void:
    turn_label.text = {"victory": "VICTORY — the tomb is yours", "defeat": "DEFEAT — the expedition has fallen", "retreat": "RETREAT — the survivors escape"}.get(state.result, state.result)
    turn_label.add_theme_color_override("font_color", GOLD)
    _log(turn_label.text, GOLD)
    var survivors: Array[String] = []
    for unit in state.survivors: survivors.append("%d %s" % [unit.count, str(unit.id).replace("_", " ")])
    var text := "[b]SURVIVORS[/b]\n" + (", ".join(survivors) if not survivors.is_empty() else "None") + "\n\n"
    var fallen: Array = state.get("fallen", [])
    if not fallen.is_empty():
        var names: Array[String] = []
        for id in fallen: names.append(str(id).capitalize())
        var fate := "lost with the battle" if state.result == "defeat" else "carried from the field, wounded: out of battle for 3 days"
        text += "[b][color=#%s]FALLEN[/color][/b]\n%s — %s\n\n" % [FOE.to_html(false), ", ".join(names), fate]
    text += "[b]LOOT[/b]\n"
    if state.rewards.is_empty(): text += "None"
    for item in state.rewards: text += item.name + "\n"
    inspection.text = text
    status.text = "The battle is over."
    board.set_hover("", "")
    return_button.show()
    return_button.grab_focus()

func _return() -> void:
    if busy or _returned or state.get("result", "ongoing") == "ongoing": return
    _returned = true
    finished.emit(state.duplicate(true))

func _log(message: String, colour := TEXT, actor := "") -> void:
    history.append(message)
    if history.size() > 200: history.pop_front()
    if not actor.is_empty() and units.has(actor):
        colour = FRIEND if units[actor].player else FOE
    log_view.append_text("[color=#%s]%s[/color]\n" % [colour.to_html(false), message.replace("[", "[lb]")])
