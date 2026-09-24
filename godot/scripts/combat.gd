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
    status = _rich(Vector2(36, 630), Vector2(730, 28), 15)
    status.scroll_active = false
    inspection = _rich(Vector2(826, 170), Vector2(415, 262), 16)
    inspection.scroll_active = false
    defend = _button("Defend · D", Vector2(830, 440), _defend)
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
    _log("Battle begins. Each turn a stack either MOVES or ATTACKS — moving ends its turn, so melee stacks strike from where they stand. Archers shoot anyone, with no retaliation.", DIM)
    _consume(reply)
    return true

func issue(action: String, cell := Vector2i.ZERO) -> bool:
    if busy or state.is_empty() or state.result != "ongoing": return false
    var reply := _decode_reply(bridge.act(action, cell.x, cell.y))
    if not reply.get("ok", false):
        _log(str(reply.get("error", "Action rejected.")), FOE)
        return false
    _consume(reply)
    return true

func _decode_reply(raw: String) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(raw)
    if reply.get("ok", false):
        # Godot's JSON parser makes every number a float. Array equality is
        # type-sensitive, so normalize hex coordinates at this boundary.
        var cells: Array = reply.state.reachable + reply.state.attackable
        for unit in reply.state.units: cells.append(unit.cell)
        for preview in reply.state.get("previews", []): cells.append(preview.cell)
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
            "defend": _log("%s defends: +25%% defence until its next turn." % _unit_label(event.unit), DIM, event.unit)
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
    if strike.is_empty() or strike.target != victim:
        _log("%s takes %s." % [_unit_label(victim), outcome], TEXT, victim)
    elif strike.retaliation:
        _log("   » retaliation: %s strikes back at %s: %s." % [_unit_label(strike.unit), _unit_label(victim), outcome], TEXT, strike.unit)
    else:
        var verb := "shoots" if strike.get("ranged", false) else "attacks"
        var pinned := " PINNED (+50%, no retaliation)" if strike.flanked else ""
        _log("%s %s %s%s: %s." % [_unit_label(strike.unit), verb, _unit_label(victim), pinned, outcome], TEXT, strike.unit)

func _unit_name(key: String) -> String:
    return units[key].name if units.has(key) else key

func _unit_label(key: String) -> String:
    if not units.has(key): return key
    return "%s ×%d" % [units[key].name, units[key].count]

func _refresh() -> void:
    board.locked = busy or auto_battle
    board.queue_redraw()
    var ongoing: bool = state.get("result", "") == "ongoing"
    if ongoing:
        var player_turn: bool = state.get("player_turn", false)
        var whose := "YOUR TURN" if player_turn and not auto_battle else ("AUTO-BATTLE" if player_turn else "ENEMY TURN")
        if busy: whose = "resolving…"
        turn_label.text = "ROUND %d   ·   %s   ·   %s" % [state.get("round", 1), _unit_label(state.get("active", "")), whose]
        turn_label.add_theme_color_override("font_color", FRIEND if player_turn else FOE)
    _build_initiative()
    defend.disabled = busy or auto_battle or not state.get("player_turn", false) or not ongoing
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
    count.text = "×%d" % unit.count
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
    if state.get("result", "") != "ongoing" and not state.is_empty(): return
    for key in [_hover_key, _pinned_key, state.get("active", "")]:
        if units.has(key) and int(units[key].count) > 0:
            _inspect(units[key])
            return

func _inspect(unit: Dictionary) -> void:
    var colour := FRIEND if unit.player else FOE
    var lines: Array[String] = []
    var tag := "ACTING NOW" if unit.key == state.get("active", "") else ("kept on screen · right-click empty ground to release" if unit.key == _pinned_key else "")
    lines.append("[font_size=21][b][color=#%s]%s ×%d[/color][/b][/font_size]" % [colour.to_html(false), unit.name, unit.count])
    lines.append("[color=#%s]%s%s[/color]" % [DIM.to_html(false), "Your expedition" if unit.player else "Enemy", ("  ·  " + tag) if not tag.is_empty() else ""])
    lines.append("Health  %d / %d on the top creature  ·  %d total" % [unit.get("hp_left", unit.unit_hp), unit.unit_hp, unit.hp])
    lines.append("Attack %d  ·  Defence %d  ·  Damage %d–%d each" % [unit.attack, unit.defense, unit.get("min_damage", 0), unit.get("max_damage", 0)])
    lines.append("Speed %d  ·  Moves %d hexes" % [unit.speed, unit.get("move", 0)])
    if unit.ranged: lines.append("Ranged  ·  %d / %d shots  ·  no retaliation when shooting" % [unit.shots, unit.get("shots_max", unit.shots)])
    else: lines.append("Melee  ·  attacks adjacent stacks only")
    var notes: Array[String] = []
    if unit.defending: notes.append("defending (+25% defence)")
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
    _update_inspection()
    _update_status()

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
    elif not my_turn:
        if auto_battle: text = "Auto-battle: the AI commands your stacks. Untick Auto-battle to take over."
        elif busy: text = "Resolving…"
        else: text = "Enemy turn — %s is deciding." % _unit_label(state.get("active", ""))
        if not unit.is_empty(): text = "%s (%s) — right-click to keep its details on screen." % [_unit_label(unit.key), "yours" if unit.player else "enemy"]
    elif _hover_cell.is_empty():
        var how := "shoot or strike" if active.get("ranged", false) and int(active.get("shots", 0)) > 0 else "attack an adjacent enemy"
        text = "%s: click a red enemy to %s · a green hex to move (ends the turn) · D to defend · right-click a stack for details." % [_unit_label(state.active), how]
    elif not unit.is_empty() and not unit.player:
        var preview := {}
        for candidate in state.get("previews", []):
            if candidate.key == unit.key: preview = candidate
        if preview.is_empty():
            kind = "blocked"
            text = "[color=#%s]%s is out of reach.[/color] Melee stacks strike only from an adjacent hex: move next to it first (moving ends your turn; it may strike first)." % [FOE.to_html(false), _unit_label(unit.key)]
        else:
            kind = "attack"
            var damage := _range_text(int(preview.damage_min), int(preview.damage_max))
            var kills := _range_text(int(preview.kills_min), int(preview.kills_max))
            text = "[b]%s %s[/b]: %s damage, kills %s of %d" % ["Shoot" if preview.ranged else "Attack", _unit_label(unit.key), damage, kills, unit.count]
            label = "%s dmg · %s slain" % [damage, kills]
            if preview.pinned: text += " · [color=#%s]PINNED: +50%%, no retaliation[/color]" % GOLD.to_html(false)
            if preview.ranged: text += " · ranged: no retaliation, no range penalty"
            elif preview.retaliation:
                text += " · [color=#%s]they strike back for %s (kills %s)[/color]" % [FOE.to_html(false), _range_text(int(preview.retaliation_min), int(preview.retaliation_max)), _range_text(int(preview.retaliation_kills_min), int(preview.retaliation_kills_max))]
            elif int(preview.kills_min) >= int(unit.count): text += " · wipes out the stack"
            elif unit.get("retaliated", false): text += " · no retaliation: already used this round"
            else: text += " · no retaliation"
    elif not unit.is_empty():
        text = "%s — %s." % [_unit_label(unit.key), "acting now" if unit.key == state.active else "your stack; right-click for details"]
    elif _hover_cell in state.get("reachable", []):
        kind = "move"
        text = "Move here — this ends %s's turn." % _unit_name(state.active)
        var near: Array[String] = []
        for other in state.get("units", []):
            if not other.player and int(other.count) > 0 and _hex_distance(other.cell, _hover_cell) == 1: near.append(_unit_label(other.key))
        if not near.is_empty():
            text += " [color=#%s]Next to %s: it can strike you before your next turn.[/color]" % [FOE.to_html(false), ", ".join(near)]
    else:
        kind = "blocked"
        text = "Out of reach — %s moves up to %d hexes, and cannot pass through stacks." % [_unit_name(state.active), active.get("move", 0)]
    status.text = text
    board.set_hover(kind if my_turn else "", label if my_turn else "")

static func _hex_distance(a: Array, b: Array) -> int:
    var dq: int = int(a[0]) - int(b[0])
    var dr: int = int(a[1]) - int(b[1])
    return (absi(dq) + absi(dr) + absi(dq + dr)) / 2

# ── Input ─────────────────────────────────────────────────────────────────────

func _cell_clicked(cell: Vector2i) -> void:
    var coordinates := [cell.x, cell.y]
    if busy or auto_battle or not state.get("player_turn", false): return
    if coordinates in state.attackable: issue("attack", cell)
    elif coordinates in state.reachable: issue("move", cell)

func _cell_right_clicked(cell: Vector2i) -> void:
    var unit := _unit_at([cell.x, cell.y])
    _pinned_key = unit.key if not unit.is_empty() else ""
    _update_inspection()

func _defend() -> void:
    if not defend.disabled: issue("defend")

func _unhandled_key_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_D:
        _defend()
        get_viewport().set_input_as_handled()

func _schedule_ai() -> void:
    if _ai_scheduled or busy or state.is_empty() or state.result != "ongoing": return
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
    var text := "[b]SURVIVORS[/b]\n" + (", ".join(survivors) if not survivors.is_empty() else "None") + "\n\n[b]LOOT[/b]\n"
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
