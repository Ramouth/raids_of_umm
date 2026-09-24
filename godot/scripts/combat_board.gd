extends Control

signal cell_clicked(cell: Vector2i)
signal cell_right_clicked(cell: Vector2i)
## Emitted when the pointer enters a new hex; inside is false when it leaves the grid.
signal cell_hovered(cell: Vector2i, inside: bool)
## Every pointer motion over the board (board-local), for picking the side of attack.
signal pointer_moved(point: Vector2)
const HEX := Vector2(80, 76)
const ORIGIN := Vector2(70, 70)
const ART := {"dune_stalker": "enemy_scout"}  # units sharing another unit's sprite
const PLAYER_COLOR := Color("789b88")
const ENEMY_COLOR := Color("bc7660")
const COMPANION_COLOR := Color("f7d580")
const DANGER_COLOR := Color("ff5a3c")
var state: Dictionary = {}
var actors: Dictionary = {}
var locked := true
## True when the last left click held Shift or Ctrl (set route waypoints).
var modified_click := false
## Hover feedback set by the combat screen: the hex under the pointer, what a
## click there would do ("attack", "move", "blocked", ""), and a short
## forecast drawn above an attack target.
var hover_cell: Array = []
var hover_kind := ""
var hover_label := ""
## A stack highlighted from outside the board (initiative bar hover).
var highlight_key := ""
## Move-and-attack preview: the hex the active stack would strike from, and
## the walk there (cells after the start, ending on stand_cell).
var stand_cell: Array = []
var walk_path: Array = []
## Shot preview: [from, to] cells of the active shooter's line, and whether a
## stack blocks it (half damage).
var shot_line: Array = []
var shot_blocked := false
## Player-chosen route: waypoint cells (in order), and when set, the hexes
## still reachable after them (replaces the plain reachable area).
var waypoints: Array = []
var route_reach: Array = []
## Route drawn for a plain move hover (cells after the start).
var move_path: Array = []
var _forecast: Label

static func unit_texture(id: String) -> Texture2D:
    var filename: String = ART.get(id, id)
    var path := "res://content/textures/units/%s.png" % filename
    if not ResourceLoader.exists(path): path = "res://content/textures/units/armoured_warrior.png"
    return load(path)

static func cell_point(cell: Array) -> Vector2:
    return ORIGIN + Vector2(float(cell[0]) * 60.0, (float(cell[1]) + float(cell[0]) * 0.5) * 76.0)

static func hex_points(center: Vector2) -> PackedVector2Array:
    var points := PackedVector2Array()
    for corner in range(6):
        var angle := float(corner) * TAU / 6.0
        points.append(center + Vector2(cos(angle) * HEX.x / 2, sin(angle) * HEX.y / sqrt(3.0)))
    return points

func _ready() -> void:
    mouse_default_cursor_shape = Control.CURSOR_ARROW
    gui_input.connect(_clicked)
    mouse_exited.connect(func():
        if not hover_cell.is_empty():
            hover_cell = []
            cell_hovered.emit(Vector2i.ZERO, false))

static func cell_at(point: Vector2) -> Array:
    for col in range(11):
        for row in range(5):
            var cell := [col, row - (col - (col & 1)) / 2]
            if Geometry2D.is_point_in_polygon(point, hex_points(cell_point(cell))):
                return cell
    return []

func _clicked(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        pointer_moved.emit(event.position)
        var cell := cell_at(event.position)
        if cell != hover_cell:
            hover_cell = cell
            cell_hovered.emit(Vector2i(cell[0], cell[1]) if not cell.is_empty() else Vector2i.ZERO, not cell.is_empty())
        return
    if event is InputEventMouseButton and event.pressed and event.button_index in [MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT]:
        var cell := cell_at(event.position)
        if cell.is_empty(): return
        if event.button_index == MOUSE_BUTTON_LEFT:
            modified_click = event.shift_pressed or event.ctrl_pressed or event.meta_pressed
            cell_clicked.emit(Vector2i(cell[0], cell[1]))
            modified_click = false
        else: cell_right_clicked.emit(Vector2i(cell[0], cell[1]))
        accept_event()

## Updates hover feedback and the pointer shape (sword-like cross for attacks).
func set_hover(kind: String, label: String) -> void:
    hover_kind = kind
    hover_label = label
    mouse_default_cursor_shape = {"attack": Control.CURSOR_CROSS, "move": Control.CURSOR_POINTING_HAND,
        "blocked": Control.CURSOR_FORBIDDEN}.get(kind, Control.CURSOR_ARROW)
    queue_redraw()

func _draw() -> void:
    for col in range(11):
        for row in range(5):
            var cell := [col, row - (col - (col & 1)) / 2]
            var points := hex_points(cell_point(cell))
            var fill := Color("55402a") if (col + row) % 2 else Color("5e472f")
            if not locked and state.get("player_turn", false):
                var reach: Array = route_reach if not waypoints.is_empty() else state.get("reachable", [])
                if cell in reach: fill = Color("405347")
                if cell in state.get("attackable", []): fill = Color("854637")
            draw_colored_polygon(points, fill)
            points.append(points[0])
            draw_polyline(points, Color("947044"), 1.0, true)
    _draw_companions()
    var playing: bool = not locked and state.get("player_turn", false)
    if not hover_cell.is_empty():
        var points := hex_points(cell_point(hover_cell))
        if playing and hover_kind == "move": draw_colored_polygon(points, Color(0.55, 0.85, 0.6, 0.28))
        points.append(points[0])
        var outline: Color = {"attack": Color("ff9d7a"), "move": Color("c9f0d2"), "blocked": Color("8a7a66")}.get(hover_kind, Color("e8d8b3"))
        draw_polyline(points, outline, 3.0 if hover_kind == "attack" else 2.0, true)
    if hover_kind == "attack" and not stand_cell.is_empty():
        _draw_walk()
    if hover_kind == "move" and not move_path.is_empty():
        _draw_route(move_path)
    _draw_waypoints()
    if hover_kind == "attack" and shot_line.size() == 2:
        _draw_shot()
    for unit in state.get("units", []):
        if unit.count <= 0: continue
        var ring := ""
        if unit.key == state.get("active", ""): ring = "active"
        elif unit.key == highlight_key: ring = "highlight"
        if ring.is_empty(): continue
        var points := hex_points(cell_point(unit.cell))
        points.append(points[0])
        draw_polyline(points, Color("f7d580") if ring == "active" else Color("ffffff"), 3.0, true)
    _draw_danger()
    _place_forecast()

static func _hex_distance(a: Array, b: Array) -> int:
    var dq: int = int(a[0]) - int(b[0])
    var dr: int = int(a[1]) - int(b[1])
    return (absi(dq) + absi(dr) + absi(dq + dr)) / 2

## Companion auras (tinted hexes), bodyguard links, and a red ring on a
## companion the enemy can reach next turn.
func _draw_companions() -> void:
    var by_key := {}
    for unit in state.get("units", []): by_key[unit.key] = unit
    for unit in state.get("units", []):
        if int(unit.count) <= 0 or int(unit.get("aura_radius", 0)) <= 0: continue
        var tint := Color(0.97, 0.84, 0.5, 0.2) if unit.player else Color(1.0, 0.5, 0.4, 0.18)
        for col in range(11):
            for row in range(5):
                var cell := [col, row - (col - (col & 1)) / 2]
                var d := _hex_distance(cell, unit.cell)
                if d == 0 or d > int(unit.aura_radius): continue
                var points := hex_points(cell_point(cell))
                draw_colored_polygon(points, tint)
                points.append(points[0])
                draw_polyline(points, Color(COMPANION_COLOR, 0.35), 1.5, true)

## Red inner ring on every companion the enemy can reach next turn (drawn
## after the active/highlight rings so it is never hidden).
func _draw_danger() -> void:
    for unit in state.get("units", []):
        if int(unit.count) <= 0 or not unit.get("companion", false) or unit.get("threats", []).is_empty(): continue
        var centre := cell_point(unit.cell)
        var points := PackedVector2Array()
        for p in hex_points(centre): points.append(centre + (p - centre) * 0.84)
        points.append(points[0])
        draw_polyline(points, DANGER_COLOR, 3.0, true)

func _active_cell() -> Array:
    for unit in state.get("units", []):
        if unit.key == state.get("active", ""): return unit.cell
    return []

## Dotted route from the active stack along `path`.
func _draw_route(path: Array) -> void:
    var start := _active_cell()
    if start.is_empty(): return
    var line := PackedVector2Array([cell_point(start)])
    for cell in path: line.append(cell_point(cell))
    draw_polyline(line, Color(0.75, 1.0, 0.8, 0.85), 3.0, true)
    for k in range(1, line.size()): draw_circle(line[k], 4.0, Color("c9f0d2"))

## Numbered gold markers on the chosen waypoints.
func _draw_waypoints() -> void:
    var font := get_theme_default_font()
    for i in waypoints.size():
        var at := cell_point(waypoints[i])
        draw_circle(at, 13.0, Color("16100a"))
        draw_circle(at, 11.0, COMPANION_COLOR)
        draw_string(font, at + Vector2(-5, 6), str(i + 1), HORIZONTAL_ALIGNMENT_CENTER, -1, 16, Color("16100a"))

## Line of fire from the active shooter: gold if clear, red and dashed if a stack is in the way.
func _draw_shot() -> void:
    var a := cell_point(shot_line[0]) + Vector2(0, -24)
    var b := cell_point(shot_line[1]) + Vector2(0, -24)
    if shot_blocked:
        draw_dashed_line(a, b, DANGER_COLOR, 3.0, 10.0, true)
    else:
        draw_line(a, b, Color(COMPANION_COLOR, 0.85), 3.0, true)

## Standing hex (sword side) and the dotted walk leading to it.
func _draw_walk() -> void:
    var active := {}
    for unit in state.get("units", []):
        if unit.key == state.get("active", ""): active = unit
    var points := hex_points(cell_point(stand_cell))
    draw_colored_polygon(points, Color(1.0, 0.72, 0.35, 0.35))
    points.append(points[0])
    draw_polyline(points, Color("ffd08a"), 3.0, true)
    if active.is_empty() or walk_path.is_empty(): return
    var line := PackedVector2Array([cell_point(active.cell)])
    for cell in walk_path: line.append(cell_point(cell))
    draw_polyline(line, Color(1.0, 0.85, 0.55, 0.85), 3.0, true)
    for k in range(1, line.size()):
        draw_circle(line[k], 5.0, Color("ffd08a"))
    # Arrowhead from the standing hex toward the target.
    var target := cell_point(hover_cell)
    var tip := cell_point(stand_cell).lerp(target, 0.42)
    var dir := (target - cell_point(stand_cell)).normalized()
    draw_colored_polygon(PackedVector2Array([tip + dir * 12, tip + dir.orthogonal() * 8, tip - dir.orthogonal() * 8]), Color("ffd08a"))

## Attack forecast box, kept above every sprite.
func _place_forecast() -> void:
    if _forecast == null:
        _forecast = Label.new()
        _forecast.z_index = 20
        _forecast.mouse_filter = Control.MOUSE_FILTER_IGNORE
        _forecast.add_theme_font_size_override("font_size", 15)
        _forecast.add_theme_color_override("font_color", Color("ffe3c4"))
        var style := StyleBoxFlat.new()
        style.bg_color = Color(0.08, 0.05, 0.03, 0.94)
        style.border_color = Color("ff9d7a")
        style.set_border_width_all(1)
        style.set_content_margin_all(5)
        _forecast.add_theme_stylebox_override("normal", style)
        add_child(_forecast)
    _forecast.visible = hover_kind == "attack" and not hover_label.is_empty() and not hover_cell.is_empty()
    if not _forecast.visible: return
    _forecast.text = hover_label
    _forecast.size = Vector2.ZERO
    var box := _forecast.get_combined_minimum_size()
    var at := cell_point(hover_cell) + Vector2(-box.x / 2, -84)
    # Keep the standing hex visible: if it lies above the target, show the box below.
    if not stand_cell.is_empty() and cell_point(stand_cell).y < cell_point(hover_cell).y - 10:
        at.y = cell_point(hover_cell).y + 46
    _forecast.position = Vector2(clampf(at.x, 2, size.x - box.x - 2), clampf(at.y, 2, size.y - box.y - 2))

func _add_shadow(actor: Node2D, team_color: Color) -> void:
    var points := PackedVector2Array()
    for step in range(24):
        var angle := float(step) * TAU / 24.0
        points.append(Vector2(cos(angle) * 24, sin(angle) * 9))
    var shadow := Polygon2D.new()
    shadow.polygon = points
    shadow.color = Color(0.09, 0.07, 0.04, 0.7)
    actor.add_child(shadow)
    points.append(points[0])
    var ring := Line2D.new()
    ring.points = points
    ring.default_color = team_color
    ring.width = 2.0
    actor.add_child(ring)

func sync(snapshot: Dictionary) -> void:
    state = snapshot
    for unit in state.units:
        if not actors.has(unit.key):
            var actor := Node2D.new()
            add_child(actor)
            actors[unit.key] = actor
            _add_shadow(actor, COMPANION_COLOR if unit.get("companion", false) else (PLAYER_COLOR if unit.player else ENEMY_COLOR))
            var sprite := Sprite2D.new()
            sprite.name = "Sprite"
            sprite.texture = unit_texture(unit.id)
            sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
            var art_size := sprite.texture.get_size()
            sprite.scale = Vector2.ONE * minf(66.0 / art_size.x, 72.0 / art_size.y)
            sprite.position.y = -18
            sprite.flip_h = not unit.player
            actor.add_child(sprite)
            var badge := Label.new()
            badge.name = "Count"
            badge.mouse_filter = Control.MOUSE_FILTER_IGNORE
            badge.position = Vector2(-35, 14)
            badge.size = Vector2(70, 24)
            badge.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
            badge.add_theme_color_override("font_color", Color("b9e0cc") if unit.player else Color("ffb6a0"))
            badge.add_theme_color_override("font_outline_color", Color("16100a"))
            badge.add_theme_constant_override("outline_size", 6)
            actor.add_child(badge)
            var shield := Polygon2D.new()
            shield.name = "Shield"
            shield.polygon = PackedVector2Array([Vector2(-8, -9), Vector2(8, -9), Vector2(8, 1), Vector2(0, 10), Vector2(-8, 1)])
            shield.color = COMPANION_COLOR
            shield.position = Vector2(30, -6)
            shield.z_index = 5   # above neighbouring sprites and badges
            var rim := Line2D.new()
            rim.points = PackedVector2Array([Vector2(-8, -9), Vector2(8, -9), Vector2(8, 1), Vector2(0, 10), Vector2(-8, 1), Vector2(-8, -9)])
            rim.width = 2.0
            rim.default_color = Color("5a4424")
            shield.add_child(rim)
            shield.visible = false
            actor.add_child(shield)
        var node: Node2D = actors[unit.key]
        node.position = cell_point(unit.cell)
        node.visible = unit.count > 0
        node.modulate = Color.WHITE
        node.set_meta("hp", int(unit.hp))
        node.set_meta("unit_hp", int(unit.unit_hp))
        node.set_meta("companion", unit.get("companion", false))
        node.get_node("Count").text = _badge(unit.get("companion", false), int(unit.hp), int(unit.unit_hp), int(unit.count))
        if unit.get("companion", false): node.get_node("Count").add_theme_color_override("font_color", COMPANION_COLOR)
        node.get_node("Shield").visible = false
    # A gold shield marks each stack guarding a companion.
    for unit in state.units:
        var guard: String = unit.get("bodyguard", "")
        if int(unit.count) > 0 and actors.has(guard): actors[guard].get_node("Shield").visible = true
    queue_redraw()

## Troops show their head count; a companion (one figure) shows its health.
static func _badge(companion: bool, hp: int, unit_hp: int, count: int) -> String:
    return "♥ %d/%d" % [hp, unit_hp] if companion else "× %d" % count

func float_text(actor: Node2D, message: String, color: Color, duration: float) -> void:
    var label := Label.new()
    label.mouse_filter = Control.MOUSE_FILTER_IGNORE
    label.text = message
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.size = Vector2(140, 44)
    label.position = actor.position + Vector2(-70, -86)
    label.add_theme_font_size_override("font_size", 19)
    label.add_theme_color_override("font_color", color)
    label.add_theme_color_override("font_outline_color", Color("16100a"))
    label.add_theme_constant_override("outline_size", 6)
    add_child(label)
    var tween := create_tween().set_parallel()
    tween.tween_property(label, "position:y", label.position.y - 32, duration)
    tween.tween_property(label, "modulate:a", 0.0, duration)
    await tween.finished
    label.queue_free()

func animate(event: Dictionary, speed: float) -> void:
    var actor: Node2D = actors.get(event.unit)
    var target: Node2D = actors.get(event.target)
    var duration := 0.22 * speed
    match event.type:
        "move":
            if actor:
                for cell in event.path:
                    var tween := create_tween()
                    tween.tween_property(actor, "position", cell_point(cell), 0.12 * speed)
                    await tween.finished
        "attack":
            if actor and target:
                if event.flanked: float_text(target, "PINNED", Color("f7d580"), duration * 2)
                if event.get("blocked", false): float_text(target, "BLOCKED SHOT ½", Color("ffb08a"), duration * 3)
                var start := actor.position
                if start.distance_to(target.position) > 100:
                    var projectile := Polygon2D.new()
                    projectile.polygon = PackedVector2Array([Vector2(-12, -2), Vector2(8, -2), Vector2(12, 0), Vector2(8, 2), Vector2(-12, 2)])
                    projectile.color = Color("f7d580")
                    projectile.position = start + Vector2(0, -24)
                    projectile.rotation = (target.position - start).angle()
                    add_child(projectile)
                    var tween := create_tween()
                    tween.tween_property(projectile, "position", target.position + Vector2(0, -24), duration)
                    await tween.finished
                    projectile.queue_free()
                else:
                    var tween := create_tween()
                    tween.tween_property(actor, "position", start.lerp(target.position, 0.35), duration * 0.5)
                    tween.tween_property(actor, "position", start, duration * 0.5)
                    await tween.finished
        "damage":
            # Damage events identify the damaged stack as `unit`, not `target`.
            if actor:
                var hp := maxi(0, int(actor.get_meta("hp")) - int(event.damage))
                actor.set_meta("hp", hp)
                var unit_hp := int(actor.get_meta("unit_hp"))
                actor.get_node("Count").text = _badge(actor.get_meta("companion", false), hp, unit_hp, ceili(float(hp) / float(unit_hp)))
                var kills := int(event.get("kills", 0))
                var guarding := "\nshields" if event.get("bodyguard", false) else ""
                float_text(actor, "−%d%s%s" % [event.damage, ("\n%d slain" % kills) if kills > 0 else "", guarding], Color("ffbc91"), duration * 3)
                actor.modulate = Color(2.0, 0.45, 0.3)
                var tween := create_tween()
                tween.tween_property(actor, "modulate", Color.WHITE, duration)
                await tween.finished
        "death":
            if actor:
                var tween := create_tween()
                tween.tween_property(actor, "modulate:a", 0.0, duration)
                await tween.finished
                actor.hide()
        "defend":
            if actor: await float_text(actor, "DEFEND", Color("b9e0cc"), duration)
        _:
            await get_tree().process_frame
