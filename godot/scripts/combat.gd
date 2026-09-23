extends Control

signal finished(result: Dictionary)
const Board = preload("res://scripts/combat_board.gd")
var bridge: RefCounted
var state: Dictionary = {}
var busy := true
var animation_speed := 1.0
var auto_battle := false
var _returned := false
var _ai_scheduled := false
var board: Control
var heading: Label
var turn_label: Label
var initiative: Label
var inspection: Label
var log_label: Label
var defend: Button
var retreat: Button
var auto_button: CheckButton
var return_button: Button
var confirm_retreat: ConfirmationDialog
var history: Array[String] = []

func _ready() -> void:
    mouse_filter = Control.MOUSE_FILTER_STOP
    size = Vector2(1280, 800)
    var background := ColorRect.new()
    background.color = Color("191610")
    background.size = size
    background.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(background)
    _panel(Rect2(24, 18, 1232, 148))
    _panel(Rect2(24, 190, 750, 456))
    _panel(Rect2(808, 190, 448, 456))
    _panel(Rect2(24, 658, 1232, 124))
    heading = _label(Vector2(40, 26), Vector2(1200, 42), 28)
    heading.text = "THE SEALED TOMB"
    turn_label = _label(Vector2(40, 82), Vector2(1200, 28), 18)
    initiative = _label(Vector2(40, 124), Vector2(1200, 60), 15)
    board = Board.new()
    board.position = Vector2(28, 192)
    board.size = Vector2(750, 450)
    add_child(board)
    board.cell_clicked.connect(_cell_clicked)
    inspection = _label(Vector2(830, 206), Vector2(405, 190), 18)
    defend = _button("Defend · D", Vector2(830, 410), _defend)
    retreat = _button("Retreat…", Vector2(830, 468), func(): confirm_retreat.popup_centered())
    auto_button = CheckButton.new()
    auto_button.text = "Auto-battle"
    auto_button.position = Vector2(830, 532)
    auto_button.size = Vector2(250, 40)
    auto_button.toggled.connect(func(enabled: bool):
        auto_battle = enabled
        _refresh()
        _schedule_ai())
    add_child(auto_button)
    return_button = _button("Return to the desert", Vector2(830, 594), _return)
    return_button.hide()
    log_label = _label(Vector2(46, 665), Vector2(1180, 110), 15)
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
    label.add_theme_color_override("font_color", Color("e8d8b3"))
    add_child(label)
    return label

func _button(text: String, at: Vector2, action: Callable) -> Button:
    var button := Button.new()
    button.text = text
    button.position = at
    button.size = Vector2(360, 46)
    var style := StyleBoxFlat.new()
    style.bg_color = Color("392b1c")
    style.border_color = Color("806037")
    style.set_border_width_all(1)
    button.add_theme_stylebox_override("normal", style)
    var hover := style.duplicate() as StyleBoxFlat
    hover.bg_color = Color("51402b")
    button.add_theme_stylebox_override("hover", hover)
    button.add_theme_color_override("font_color", Color("e8d8b3"))
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
    state = reply.state
    board.sync(state)
    _log("Green hexes: move · Red hexes: attack · Click any stack to inspect. Moving ends the turn.")
    _consume(reply)
    return true

func issue(action: String, cell := Vector2i.ZERO) -> bool:
    if busy or state.is_empty() or state.result != "ongoing": return false
    var reply := _decode_reply(bridge.act(action, cell.x, cell.y))
    if not reply.get("ok", false):
        _log(str(reply.get("error", "Action rejected.")))
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
        for cell in cells:
            cell[0] = int(cell[0])
            cell[1] = int(cell[1])
    return reply

func _consume(reply: Dictionary) -> void:
    busy = true
    _refresh()
    for event in reply.events:
        if event.type == "attack":
            _log("%s attacks %s%s%s." % [_unit_name(event.unit), _unit_name(event.target), " · retaliation" if event.retaliation else "", " · pinned (+50%, no retaliation)" if event.flanked else ""])
        elif event.type == "damage":
            _log("%s takes %d damage%s." % [_unit_name(event.unit), event.damage, " · flanked" if event.flanked else ""])
        await board.animate(event, animation_speed)
    state = reply.state
    board.sync(state)
    if not bridge.acknowledge(int(reply.ticket)):
        _log("Combat synchronization failed. Orders remain locked.")
        return
    busy = false
    _refresh()
    if state.result != "ongoing":
        auto_battle = false
        auto_button.set_pressed_no_signal(false)
        _show_result()
    else:
        _schedule_ai()

func _unit_name(key: String) -> String:
    for unit in state.get("units", []):
        if unit.key == key: return unit.name
    return key

func _refresh() -> void:
    board.locked = busy or auto_battle
    board.queue_redraw()
    var active_name := _unit_name(state.get("active", ""))
    turn_label.text = "ROUND %d   ·   %s%s" % [state.get("round", 1), active_name, " — resolving action…" if busy else (" — your turn" if state.get("player_turn", false) else " — enemy turn")]
    var order: Array[String] = []
    for slot in state.get("initiative", []):
        order.append(("✓ " if slot.acted else ("▶ " if slot.key == state.active else "")) + _unit_name(slot.key))
    initiative.text = "INITIATIVE   " + "   →   ".join(order)
    defend.disabled = busy or auto_battle or not state.get("player_turn", false) or state.get("result", "") != "ongoing"
    retreat.disabled = busy or state.get("result", "") != "ongoing"
    auto_button.disabled = state.get("result", "") != "ongoing"
    for unit in state.get("units", []):
        if unit.key == state.get("active", ""): _inspect(unit)

func _inspect(unit: Dictionary) -> void:
    inspection.text = "%s\n%s · %d remaining\nHP %d  |  ATK %d  |  DEF %d\nSpeed %d%s%s" % [unit.name, "Expedition" if unit.player else "Enemy", unit.count, unit.hp, unit.attack, unit.defense, unit.speed, "\nShots remaining: %d" % unit.shots if unit.ranged else "\nMelee · adjacent targets only", "\nDefending" if unit.defending else ""]

func _cell_clicked(cell: Vector2i) -> void:
    var coordinates := [cell.x, cell.y]
    for unit in state.get("units", []):
        if unit.cell == coordinates and unit.count > 0: _inspect(unit)
    if busy or auto_battle or not state.get("player_turn", false): return
    if coordinates in state.attackable: issue("attack", cell)
    elif coordinates in state.reachable: issue("move", cell)

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
    var survivors: Array[String] = []
    for unit in state.survivors: survivors.append("%d %s" % [unit.count, str(unit.id).replace("_", " ")])
    inspection.text = "SURVIVORS\n" + (", ".join(survivors) if not survivors.is_empty() else "None") + "\n\nLOOT\n"
    if state.rewards.is_empty(): inspection.text += "None"
    for item in state.rewards: inspection.text += item.name + "\n"
    return_button.show()
    return_button.grab_focus()

func _return() -> void:
    if busy or _returned or state.get("result", "ongoing") == "ongoing": return
    _returned = true
    finished.emit(state.duplicate(true))

func _log(message: String) -> void:
    history.append(message)
    if history.size() > 4: history.pop_front()
    log_label.text = "\n".join(history)
