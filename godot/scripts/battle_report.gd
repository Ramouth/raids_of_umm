extends Control
## HoMM3-style screens after a battle, pushed on the ScreenStack by desert.gd
## before the map (and before any story the fight set off).
##   kind "result": Victory / Defeat / Retreat. Both sides' losses with unit
##     pictures, experience gained (victory), the biggest single hit and the
##     damage each side dealt. `record` is the battle record (resolve_encounter's
##     "battle", or the combat snapshot's for a dungeon).
##   kind "spoils": what the victory turned up: resources and items, each item
##     with its picture and its words (a keepsake's text is read here first).
## One button closes it; Enter / Space / Esc do the same. The button never takes
## focus: a focused button would act on the key's release as well (two closes).

signal finished(result: Dictionary)

const TEXT := Color("e8d8b8")
const DIM := Color("9a8468")
const GOLD := Color("f0c870")
const EYEBROW := Color("ad854a")
const HEAD_WON := Color("f3dfb0")
const HEAD_LOST := Color("d98a6a")
const PANEL := Color("211c14")
const FRAME := Color("665032")

var kind := "result"
var record: Dictionary = {}
var spoils: Dictionary = {}
var title := ""                 # the fight's name ("Thicket Dogs"); "" for none
var unit_names: Dictionary = {} # unit id -> display name (desert.unit_names)
var wounded_until: Dictionary = {} # companion id -> the day the wound heals (the session's specials)
var continue_button: Button
var _closed := false

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    mouse_filter = Control.MOUSE_FILTER_STOP
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, 150)
    margin.add_theme_constant_override("margin_top", 36)
    margin.add_theme_constant_override("margin_bottom", 32)
    add_child(margin)
    var column := VBoxContainer.new()
    column.name = "Column"
    column.add_theme_constant_override("separation", 12)
    margin.add_child(column)
    if kind == "spoils": _build_spoils(column)
    else: _build_result(column)
    if kind != "spoils" or spoils.get("items", []).is_empty():   # the item list takes the room itself
        var spacer := Control.new()
        spacer.size_flags_vertical = Control.SIZE_EXPAND_FILL
        column.add_child(spacer)
    var row := HBoxContainer.new()
    row.alignment = BoxContainer.ALIGNMENT_CENTER
    column.add_child(row)
    continue_button = Button.new()
    continue_button.name = "Continue"
    continue_button.text = ("Take it all" if kind == "spoils" else "Continue") + "   Enter"
    continue_button.custom_minimum_size = Vector2(240, 46)
    continue_button.focus_mode = Control.FOCUS_NONE
    continue_button.pressed.connect(close)
    row.add_child(continue_button)

func close() -> void:
    if _closed: return
    _closed = true
    finished.emit({})

func _unhandled_key_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo \
            and event.keycode in [KEY_ENTER, KEY_KP_ENTER, KEY_SPACE, KEY_ESCAPE]:
        close()
        get_viewport().set_input_as_handled()

# --- Result ---------------------------------------------------------------

func _build_result(column: VBoxContainer) -> void:
    var result := str(record.get("result", "victory"))
    var won := result == "victory"
    var eyebrow := "BATTLE" if title.is_empty() else title.to_upper() + "  ·  BATTLE"
    column.add_child(_label(eyebrow, 13, EYEBROW, true))
    var heading := _label({"victory": "Victory", "defeat": "Defeat", "retreat": "Retreat"}.get(result, result.capitalize()), 36, HEAD_WON if won else HEAD_LOST, true)
    heading.name = "Heading"
    column.add_child(heading)
    column.add_child(_label({
        "victory": "The field is yours.",
        "defeat": "Your army is broken. Those who fell stay on the field.",
        "retreat": "Your survivors fall back. The enemy still holds the field, and remembers who ran.",
    }.get(result, ""), 16, TEXT, true))
    var sides := HBoxContainer.new()
    sides.name = "Losses"
    sides.alignment = BoxContainer.ALIGNMENT_CENTER
    sides.add_theme_constant_override("separation", 20)
    column.add_child(sides)
    var losses: Dictionary = record.get("losses", {})
    sides.add_child(_losses_panel("YOUR LOSSES", losses.get("player", []), result, "PlayerLosses"))
    sides.add_child(_losses_panel("ENEMY LOSSES", losses.get("enemy", []), result, "EnemyLosses"))
    var xp := int(record.get("xp", 0))
    if won and xp > 0:
        var gained := _label("Experience gained:  +%d XP" % xp, 20, GOLD, true)
        gained.name = "Experience"
        column.add_child(gained)
    column.add_child(_stats())

func _losses_panel(heading: String, stacks: Array, result: String, node_name: String) -> Control:
    var panel := _panel(Vector2(480, 236))
    panel.name = node_name
    var box := VBoxContainer.new()
    box.add_theme_constant_override("separation", 8)
    panel.add_child(box)
    box.add_child(_label(heading, 13, EYEBROW, true))
    var grid := GridContainer.new()
    grid.name = "Units"
    grid.columns = 4
    grid.add_theme_constant_override("h_separation", 10)
    grid.add_theme_constant_override("v_separation", 6)
    box.add_child(grid)
    if stacks.is_empty():
        var none := _label("None", 18, DIM, true)
        none.name = "None"
        none.custom_minimum_size = Vector2(440, 150)
        none.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
        box.add_child(none)
        return panel
    for stack in stacks:
        var cell := VBoxContainer.new()
        cell.name = "Loss_" + str(stack.get("id", ""))
        cell.custom_minimum_size = Vector2(102, 0)
        cell.add_theme_constant_override("separation", 0)
        var picture := TextureRect.new()
        picture.name = "Picture"
        picture.texture = Art.texture("unit." + str(stack.get("id", "")))
        picture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
        picture.custom_minimum_size = Vector2(102, 72)
        picture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
        picture.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
        cell.add_child(picture)
        var count := _label(str(int(stack.get("count", 0))), 18, HEAD_WON, true)
        count.name = "Count"
        cell.add_child(count)
        var name := str(stack.get("name", unit_names.get(str(stack.get("id", "")), str(stack.get("id", "")))))
        if bool(stack.get("companion", false)):
            var heals := int(wounded_until.get(str(stack.get("id", "")), 0))
            name += "\n(lost)" if result == "defeat" else ("\n(wounded until day %d)" % heals if heals > 0 else "\n(wounded)")
        var caption := _label(name, 12, DIM, true)
        caption.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
        caption.custom_minimum_size = Vector2(102, 0)
        cell.add_child(caption)
        grid.add_child(cell)
    return panel

func _stats() -> Control:
    var panel := _panel(Vector2(980, 0))
    panel.name = "Stats"
    var box := VBoxContainer.new()
    box.add_theme_constant_override("separation", 4)
    panel.add_child(box)
    box.add_child(_label("BATTLE", 13, EYEBROW, true))
    var hit: Dictionary = record.get("max_hit", {})
    if int(hit.get("amount", 0)) > 0:
        var whose := " (yours)" if str(hit.get("side", "")) == "player" else (" (the enemy's)" if str(hit.get("side", "")) == "enemy" else "")
        var line := _label("Max damage:  %d by %s%s" % [int(hit.amount), str(hit.get("name", "")), whose], 16, TEXT, true)
        line.name = "MaxDamage"
        box.add_child(line)
    var total: Dictionary = record.get("total_damage", {})
    var dealt := _label("Damage dealt:  %d by you   ·   %d by the enemy" % [int(total.get("player", 0)), int(total.get("enemy", 0))], 16, TEXT, true)
    dealt.name = "TotalDamage"
    box.add_child(dealt)
    return panel

# --- Spoils ---------------------------------------------------------------

func _build_spoils(column: VBoxContainer) -> void:
    var eyebrow := "SPOILS" if title.is_empty() else title.to_upper() + "  ·  SPOILS"
    column.add_child(_label(eyebrow, 13, EYEBROW, true))
    var heading := _label("Among the fallen, you find", 32, HEAD_WON, true)
    heading.name = "Heading"
    column.add_child(heading)
    var resources: Dictionary = spoils.get("resources", {})
    if not resources.is_empty():
        var row := HBoxContainer.new()
        row.name = "Resources"
        row.alignment = BoxContainer.ALIGNMENT_CENTER
        row.add_theme_constant_override("separation", 28)
        column.add_child(row)
        for resource in resources:
            var cell := HBoxContainer.new()
            cell.name = str(resource)
            cell.add_theme_constant_override("separation", 6)
            var icon := TextureRect.new()
            icon.texture = Art.texture("kind." + str(resource).to_lower())
            icon.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
            icon.custom_minimum_size = Vector2(48, 48)
            icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
            icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
            cell.add_child(icon)
            var amount := _label("%+d %s" % [int(resources[resource]), str(resource)], 20, GOLD)
            amount.name = "Amount"
            amount.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
            amount.custom_minimum_size.y = 48
            cell.add_child(amount)
            row.add_child(cell)
        column.add_child(_label("Added to your treasury.", 13, DIM, true))
    var items: Array = spoils.get("items", [])
    if items.is_empty(): return
    var scroll := ScrollContainer.new()
    scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
    column.add_child(scroll)
    var list := VBoxContainer.new()
    list.name = "Items"
    list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    list.add_theme_constant_override("separation", 12)
    scroll.add_child(list)
    for item in items: list.add_child(_item_card(item))

func _item_card(item: Dictionary) -> Control:
    var panel := _panel(Vector2(960, 0))
    panel.name = "Item_" + str(item.get("id", ""))
    var row := HBoxContainer.new()
    row.add_theme_constant_override("separation", 18)
    panel.add_child(row)
    var picture := TextureRect.new()
    picture.name = "Picture"
    picture.texture = Art.texture("item." + str(item.get("id", "")))
    picture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    picture.custom_minimum_size = Vector2(154, 112)
    picture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    picture.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    row.add_child(picture)
    var words := VBoxContainer.new()
    words.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    words.add_theme_constant_override("separation", 6)
    row.add_child(words)
    var name := _label(str(item.get("name", item.get("id", ""))), 20, GOLD)
    name.name = "Name"
    words.add_child(name)
    var keepsake := str(item.get("kind", "")) == "keepsake"
    var note := _label("A keepsake: it is not worn. Read it again any time under Keepsakes in the Hero screen (H)." if keepsake
        else "Commander item: wear it from the Hero screen (H) and every troop stack feels it.", 13, EYEBROW)
    note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    words.add_child(note)
    var text := _label(str(item.get("text", "")), 16, TEXT)
    text.name = "Text"
    text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    text.custom_minimum_size = Vector2(740, 0)
    words.add_child(text)
    return panel

# --- Helpers --------------------------------------------------------------

func _panel(min_size: Vector2) -> PanelContainer:
    var panel := PanelContainer.new()
    panel.custom_minimum_size = min_size
    var style := StyleBoxFlat.new()
    style.bg_color = PANEL
    style.border_color = FRAME
    style.set_border_width_all(1)
    style.set_content_margin_all(14)
    panel.add_theme_stylebox_override("panel", style)
    return panel

func _label(text: String, size: int, color: Color, centred := false) -> Label:
    var label := Label.new()
    label.text = text
    if centred:
        label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
        label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.add_theme_font_size_override("font_size", size)
    label.add_theme_color_override("font_color", color)
    return label
