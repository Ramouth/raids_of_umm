extends Node2D
## Fog of war and ownership banners, drawn above the map's scenery.
## Unexplored hexes are black; explored-but-out-of-sight hexes are dimmed.
## State comes from the native AdventureSession snapshot.

const OWNER_COLORS := {1: Color("e8dcc0"), 2: Color("b8402c")}  # Ivory Compact, Shariw
const SHROUD := Color(0.07, 0.05, 0.03, 0.5)
const VOID := Color(0.07, 0.05, 0.03, 1.0)

var data: UmmMapData
var explored: Dictionary = {}
var visible_cells: Dictionary = {}
var owners: Dictionary = {}

func apply(state: Dictionary) -> void:
    explored.clear()
    visible_cells.clear()
    owners.clear()
    for c in state.get("explored", []): explored[Vector2i(c[0], c[1])] = true
    for c in state.get("visible", []): visible_cells[Vector2i(c[0], c[1])] = true
    for o in state.get("owners", []): owners[Vector2i(o[0], o[1])] = int(o[2])
    queue_redraw()

## Progressive reveal while the hero walks: newly seen cells become visible.
func reveal(cells: Array) -> void:
    for c in cells:
        var cell := Vector2i(c[0], c[1])
        explored[cell] = true
        visible_cells[cell] = true
    queue_redraw()

func is_explored(cell: Vector2i) -> bool:
    return explored.has(cell)

func _draw() -> void:
    if data == null:
        return
    for cell: Vector2i in owners:
        if OWNER_COLORS.has(owners[cell]) and explored.has(cell):
            _draw_banner(UmmMapData.cell_to_world(cell), OWNER_COLORS[owners[cell]])
    for cell: Vector2i in data.tiles:
        if visible_cells.has(cell):
            continue
        var center := UmmMapData.cell_to_world(cell)
        if explored.has(cell):
            draw_colored_polygon(UmmMapData.hexagon(center), SHROUD)
        else:
            # Slight overlap hides seams between opaque hexes.
            draw_colored_polygon(UmmMapData.hexagon(center, -0.6), VOID)
            _cover_rim(cell, center)

## The sand foundation is the convex hull of the map, so it pokes out past
## rim hexes. Extrude each outward-facing edge of an unexplored rim hex.
func _cover_rim(cell: Vector2i, center: Vector2) -> void:
    var corners := UmmMapData.hexagon(center, -0.6)
    for direction: Vector2i in UmmMapData.DIRECTIONS:
        if data.tiles.has(cell + direction):
            continue
        var outward := (UmmMapData.cell_to_world(cell + direction) - center).normalized()
        var best := 0
        for i in 6:
            var mid := (corners[i] + corners[(i + 1) % 6]) * 0.5 - center
            if mid.dot(outward) > ((corners[best] + corners[(best + 1) % 6]) * 0.5 - center).dot(outward):
                best = i
        var a := corners[best]
        var b := corners[(best + 1) % 6]
        draw_colored_polygon(PackedVector2Array([a, b, b + outward * 60.0, a + outward * 60.0]), VOID)

func _draw_banner(center: Vector2, color: Color) -> void:
    var pole := center + Vector2(26, -8)
    draw_line(pole, pole + Vector2(0, -40), Color("3a2412"), 3.0)
    var cloth := PackedVector2Array([pole + Vector2(1, -40), pole + Vector2(22, -33), pole + Vector2(1, -26)])
    draw_colored_polygon(cloth, color)
    cloth.append(cloth[0])
    draw_polyline(cloth, Color("3a2412"), 1.5)
