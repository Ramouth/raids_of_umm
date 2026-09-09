extends Node2D

const NO_CELL := Vector2i(9999, 9999)
@onready var map_view: Node2D = $Map
@onready var overlay: Node2D = $RouteOverlay
@onready var hero: Node2D = $Hero
@onready var camera: Camera2D = $Camera
@onready var sidebar: VBoxContainer = $HUD/Layout/Sidebar/Content
@onready var notice: Label = $HUD/Layout/Notice

var data: UmmMapData
var _dragging := false
var _hover := NO_CELL
var _preview: Array[Vector2i] = []
var _zoom_index := 1
var _zoom_levels := [0.5, 0.75, 1.0, 1.5, 2.0]
const STARTING_ARMY := [{"id": "desert_archer", "count": 10}, {"id": "mummy", "count": 3}]
const CombatView = preload("res://scripts/combat.gd")
var army: Array = STARTING_ARMY.duplicate(true)
var inventory: Array = []
var cleared_dungeons: Dictionary = {}
var battle: Control
var _battle_layer: CanvasLayer
var _battle_cell := NO_CELL
var _enter_button: Button
var _army_label: Label
var _restart_button: Button
var encounter: Dictionary = {}

func _ready() -> void:
    data = map_view.data
    if not data.error.is_empty():
        notice.text = data.error
        set_process(false)
        set_process_unhandled_input(false)
        return
    overlay.data = data
    overlay.reparent(map_view)
    map_view.move_child(overlay, map_view.get_node("Scenery").get_index())
    # Reparent into the native Y-sorted scenery layer: feet decide occlusion.
    hero.reparent(map_view.get_node("Scenery"))
    hero.place_at(data.spawn)
    hero.entered_cell.connect(_entered_cell)
    hero.journey_finished.connect(_journey_finished)
    sidebar.get_node("Grid").toggled.connect(_toggle_grid)
    sidebar.get_node("Center").pressed.connect(center_hero)
    sidebar.get_node("Overview").pressed.connect(fit_map)
    sidebar.get_node("Portrait").texture = load("res://content/textures/units/armoured_warrior.png")
    encounter = JSON.parse_string(FileAccess.get_file_as_string("res://content_source/dungeon_encounter.json"))
    _build_expedition_controls()
    $HUD/Layout/Header/Text/Title.text = data.title
    get_viewport().size_changed.connect(_resize_hud)
    _resize_hud()
    _entered_cell(hero.cell)
    fit_map()

func _resize_hud() -> void:
    var width := get_viewport_rect().size.x
    $HUD/Layout/Header.offset_right = width - 324.0
    $HUD/Layout/Footer.offset_right = width - 324.0
    $HUD/Layout/Notice.offset_right = width - 324.0

func _process(delta: float) -> void:
    var direction := Vector2(
        float(Input.is_physical_key_pressed(KEY_D)) - float(Input.is_physical_key_pressed(KEY_A)),
        float(Input.is_physical_key_pressed(KEY_S)) - float(Input.is_physical_key_pressed(KEY_W)))
    if not direction.is_zero_approx():
        camera.position += direction.normalized() * 500.0 * delta / camera.zoom.x
        _clamp_camera()
    if _dragging and not (Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) or Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE)):
        _dragging = false
    var screen := get_viewport().get_mouse_position()
    var cell := UmmMapData.world_to_cell(get_global_mouse_position()) if _over_map(screen) else NO_CELL
    if cell != _hover and not hero.moving:
        _inspect(cell)

func _over_map(point: Vector2) -> bool:
    var size := get_viewport_rect().size
    return point.x >= 0 and point.x < size.x - 310 and point.y > 155 and point.y < size.y - 55

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseButton:
        if event.button_index in [MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE]:
            _dragging = event.pressed and _over_map(event.position)
        if not event.pressed or not _over_map(event.position):
            return
        if event.button_index == MOUSE_BUTTON_WHEEL_UP:
            _zoom(1)
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            _zoom(-1)
        elif event.button_index == MOUSE_BUTTON_LEFT:
            var world_point: Vector2 = get_canvas_transform().affine_inverse() * event.position
            travel_to(UmmMapData.world_to_cell(world_point))
    elif event is InputEventMouseMotion and _dragging:
        camera.position -= event.relative / camera.zoom
        _clamp_camera()
    elif event is InputEventKey and event.pressed and not event.echo:
        match event.keycode:
            KEY_G:
                sidebar.get_node("Grid").button_pressed = not overlay.show_grid
            KEY_SPACE:
                center_hero()
            KEY_HOME:
                fit_map()
            KEY_ESCAPE:
                if not hero.moving:
                    _inspect(NO_CELL)

func _inspect(cell: Vector2i) -> void:
    _hover = cell
    overlay.hover = cell
    _preview.clear()
    if not data.tiles.has(cell):
        sidebar.get_node("Inspection").text = "Hover over the desert to inspect terrain and landmarks."
        sidebar.get_node("Route").text = "Click a reachable hex to travel."
    else:
        var tile: Dictionary = data.tiles[cell]
        var terrain := str(tile.terrain).capitalize()
        if tile.get("road", false):
            terrain += " · Road"
        var landmark: Dictionary = data.objects.get(cell, {})
        var heading := str(landmark.get("name", terrain))
        sidebar.get_node("Inspection").text = "%s\n%s · [%d, %d]" % [heading, terrain, cell.x, cell.y]
        if not data.is_passable(cell):
            sidebar.get_node("Route").text = "Impassable terrain."
        elif cell == hero.cell:
            sidebar.get_node("Route").text = "Your expedition is here."
        else:
            _preview = data.path_between(hero.cell, cell)
            sidebar.get_node("Route").text = "%d hexes · %.1f movement\nClick to travel." % [_preview.size(), data.path_cost(_preview)] if not _preview.is_empty() else "No route to this hex."
    overlay.origin = hero.cell
    overlay.path = _preview.duplicate()
    overlay.queue_redraw()

func travel_to(cell: Vector2i) -> bool:
    if hero.moving or is_instance_valid(battle):
        return false
    _inspect(cell)
    if _preview.is_empty():
        return false
    notice.text = "Crossing the sands…"
    sidebar.get_node("Route").text = "Travelling · %d hexes" % _preview.size()
    hero.follow(_preview.duplicate())
    return true

func _entered_cell(cell: Vector2i) -> void:
    var landmark: Dictionary = data.objects.get(cell, {})
    sidebar.get_node("Location").text = str(landmark.get("name", "%s · [%d, %d]" % [str(data.tiles[cell].terrain).capitalize(), cell.x, cell.y]))
    overlay.origin = cell
    if not overlay.path.is_empty() and overlay.path[0] == cell:
        overlay.path.pop_front()
    overlay.queue_redraw()
    _update_expedition()

func _journey_finished() -> void:
    var object: Dictionary = data.objects.get(hero.cell, {})
    match object.get("type", ""):
        "town": notice.text = "%s · Banners stir above the gates." % object.name
        "dungeon": notice.text = "%s · A cold wind rises from the sealed entrance." % object.name
        "artifact": notice.text = "%s · Something ancient glimmers beneath the sand." % object.name
        _: notice.text = "The expedition has arrived. Choose the next stretch of your journey."
    _hover = NO_CELL
    _inspect(hero.cell)
    _update_expedition()

func _build_expedition_controls() -> void:
    sidebar.add_theme_constant_override("separation", 10)
    sidebar.get_node("Portrait").custom_minimum_size.y = 55
    sidebar.get_node("Inspection").custom_minimum_size.y = 48
    sidebar.get_node("Route").custom_minimum_size.y = 42
    _army_label = Label.new()
    _army_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    _army_label.add_theme_font_size_override("font_size", 13)
    sidebar.add_child(_army_label)
    sidebar.move_child(_army_label, sidebar.get_node("Separator").get_index())
    _enter_button = Button.new()
    _enter_button.text = "Enter dungeon"
    _enter_button.pressed.connect(enter_dungeon)
    sidebar.add_child(_enter_button)
    sidebar.move_child(_enter_button, sidebar.get_node("Spacer").get_index())
    _restart_button = Button.new()
    _restart_button.text = "New expedition"
    _restart_button.tooltip_text = "Reset the army, collected loot, and cleared dungeons; return to Khemret."
    _restart_button.pressed.connect(new_expedition)
    sidebar.add_child(_restart_button)
    sidebar.move_child(_restart_button, sidebar.get_node("Spacer").get_index())

func _update_expedition() -> void:
    if not is_instance_valid(_army_label): return
    var summary: Array[String] = []
    for stack in army: summary.append("%d %s" % [stack.count, str(stack.id).replace("_", " ")])
    _army_label.text = ", ".join(summary) if not army.is_empty() else "Your army has fallen. Start a new expedition to fight again."
    if not inventory.is_empty():
        var loot: Array[String] = []
        for item in inventory: loot.append(item.name)
        _army_label.text += "\nLoot: " + ", ".join(loot)
    var at_dungeon: bool = data.objects.get(hero.cell, {}).get("type", "") == "dungeon"
    _enter_button.visible = at_dungeon
    _enter_button.disabled = hero.moving or army.is_empty() or cleared_dungeons.has(hero.cell)
    _enter_button.text = "Dungeon cleared" if cleared_dungeons.has(hero.cell) else "Enter dungeon"
    _restart_button.visible = army.is_empty()

func enter_dungeon() -> bool:
    if hero.moving or is_instance_valid(battle) or army.is_empty() or cleared_dungeons.has(hero.cell): return false
    var landmark: Dictionary = data.objects.get(hero.cell, {})
    if landmark.get("type", "") != "dungeon": return false
    _battle_cell = hero.cell
    _battle_layer = CanvasLayer.new()
    _battle_layer.layer = 10
    add_child(_battle_layer)
    var backdrop := ColorRect.new()
    backdrop.color = Color("191610")
    backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    _battle_layer.add_child(backdrop)
    battle = CombatView.new()
    _battle_layer.add_child(battle)
    battle.finished.connect(_battle_finished)
    if not battle.begin(army, encounter, str(landmark.name)):
        notice.text = "Could not start combat. Check the native build and data files."
        battle = null
        _battle_layer.queue_free()
        _battle_cell = NO_CELL
        return false
    _dragging = false
    set_process(false)
    set_process_unhandled_input(false)
    $HUD.hide()
    return true

func _battle_finished(result: Dictionary) -> void:
    if not is_instance_valid(battle) or _battle_cell == NO_CELL: return
    army = result.survivors.duplicate(true)
    if result.result == "victory" and not cleared_dungeons.has(_battle_cell):
        cleared_dungeons[_battle_cell] = true
        inventory.append_array(result.rewards.duplicate(true))
    notice.text = {"victory": "Victory! Survivors and loot have returned to the expedition.", "defeat": "The army has fallen. Start a new expedition to challenge the tomb again.", "retreat": "Your survivors return. The dungeon remains guarded."}.get(result.result, "Returned to the desert.")
    _battle_cell = NO_CELL
    battle = null
    _battle_layer.queue_free()
    $HUD.show()
    set_process(true)
    set_process_unhandled_input(true)
    _update_expedition()

func new_expedition() -> void:
    if is_instance_valid(battle) or not army.is_empty(): return
    army = STARTING_ARMY.duplicate(true)
    inventory.clear()
    cleared_dungeons.clear()
    hero.place_at(data.spawn)
    _entered_cell(hero.cell)
    center_hero()
    notice.text = "A new expedition sets out. Previous loot and dungeon progress have been reset."

func _toggle_grid(enabled: bool) -> void:
    overlay.show_grid = enabled
    overlay.queue_redraw()

func _zoom(direction: int) -> void:
    var before := get_global_mouse_position()
    _zoom_index = clampi(_zoom_index + direction, 0, _zoom_levels.size() - 1)
    camera.zoom = Vector2.ONE * _zoom_levels[_zoom_index]
    camera.force_update_scroll()
    camera.position += before - get_global_mouse_position()
    _clamp_camera()

func center_hero() -> void:
    camera.position = hero.position + Vector2(155.0 / camera.zoom.x, -30.0 / camera.zoom.x)
    camera.force_update_scroll()

func fit_map() -> void:
    var viewport := get_viewport_rect().size
    var available := viewport - Vector2(370, 210)
    var fit := minf(available.x / map_view.world_bounds.size.x, available.y / map_view.world_bounds.size.y)
    fit = clampf(fit, 0.3, 1.0)
    _zoom_levels[0] = minf(fit, 0.5)
    _zoom_index = 0
    camera.zoom = Vector2.ONE * fit
    camera.position = map_view.world_bounds.get_center() + Vector2(155.0 / fit, -50.0 / fit)
    camera.force_update_scroll()

func _clamp_camera() -> void:
    var bounds: Rect2 = map_view.world_bounds.grow(250.0)
    camera.position = camera.position.clamp(bounds.position, bounds.end)
    camera.force_update_scroll()
