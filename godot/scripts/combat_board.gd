extends Control

signal cell_clicked(cell: Vector2i)
const HEX := Vector2(80, 76)
const ORIGIN := Vector2(70, 70)
const ART := {"dune_stalker": "enemy_scout"}  # units sharing another unit's sprite
var state: Dictionary = {}
var actors: Dictionary = {}
var locked := true

static func cell_point(cell: Array) -> Vector2:
    return ORIGIN + Vector2(float(cell[0]) * 60.0, (float(cell[1]) + float(cell[0]) * 0.5) * 76.0)

static func hex_points(center: Vector2) -> PackedVector2Array:
    var points := PackedVector2Array()
    for corner in range(6):
        var angle := float(corner) * TAU / 6.0
        points.append(center + Vector2(cos(angle) * HEX.x / 2, sin(angle) * HEX.y / sqrt(3.0)))
    return points

func _ready() -> void:
    mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
    gui_input.connect(_clicked)

func _clicked(event: InputEvent) -> void:
    if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
        for col in range(11):
            for row in range(5):
                var cell := [col, row - (col - (col & 1)) / 2]
                if Geometry2D.is_point_in_polygon(event.position, hex_points(cell_point(cell))):
                    cell_clicked.emit(Vector2i(cell[0], cell[1]))
                    accept_event()
                    return

func _draw() -> void:
    for col in range(11):
        for row in range(5):
            var cell := [col, row - (col - (col & 1)) / 2]
            var points := hex_points(cell_point(cell))
            var fill := Color("55402a") if (col + row) % 2 else Color("5e472f")
            if not locked and state.get("player_turn", false):
                if cell in state.get("reachable", []): fill = Color("405347")
                if cell in state.get("attackable", []): fill = Color("854637")
            draw_colored_polygon(points, fill)
            points.append(points[0])
            draw_polyline(points, Color("947044"), 1.0, true)
    for unit in state.get("units", []):
        if unit.count > 0 and unit.key == state.get("active", ""):
            var points := hex_points(cell_point(unit.cell))
            points.append(points[0])
            draw_polyline(points, Color("f7d580"), 3.0, true)

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
            _add_shadow(actor, Color("789b88") if unit.player else Color("bc7660"))
            var sprite := Sprite2D.new()
            sprite.name = "Sprite"
            var filename: String = ART.get(unit.id, unit.id)
            var path := "res://content/textures/units/%s.png" % filename
            if not ResourceLoader.exists(path): path = "res://content/textures/units/armoured_warrior.png"
            sprite.texture = load(path)
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
        var node: Node2D = actors[unit.key]
        node.position = cell_point(unit.cell)
        node.visible = unit.count > 0
        node.modulate = Color.WHITE
        node.set_meta("hp", int(unit.hp))
        node.set_meta("unit_hp", int(unit.unit_hp))
        node.get_node("Count").text = "× %d" % unit.count
    queue_redraw()

func float_text(actor: Node2D, message: String, color: Color, duration: float) -> void:
    var label := Label.new()
    label.mouse_filter = Control.MOUSE_FILTER_IGNORE
    label.text = message
    label.position = actor.position + Vector2(-24, -60)
    label.add_theme_color_override("font_color", color)
    label.add_theme_constant_override("outline_size", 5)
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
                actor.get_node("Count").text = "× %d" % ceili(float(hp) / float(actor.get_meta("unit_hp")))
                float_text(actor, "−%d%s" % [event.damage, " FLANK" if event.flanked else ""], Color("ffbc91"), duration * 2)
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
