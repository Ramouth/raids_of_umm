extends Node2D

var data: UmmMapData
var hover := Vector2i(9999, 9999)
var path: Array[Vector2i] = []
var origin := Vector2i.ZERO
var show_grid := false:
    set(value):
        show_grid = value
        set_process(value)
        queue_redraw()
var _grid_zoom := 0.0

func _ready() -> void:
    set_process(show_grid)

func _process(_delta: float) -> void:
    var zoom := get_global_transform_with_canvas().get_scale().x
    if not is_equal_approx(zoom, _grid_zoom):
        _grid_zoom = zoom
        queue_redraw()
var reachable := -1  # steps affordable today; -1 = whole route

func _draw() -> void:
    if data == null:
        return
    if show_grid:
        var pixel := 1.0 / maxf(0.1, get_global_transform_with_canvas().get_scale().x)
        for cell: Vector2i in data.tiles:
            var points := UmmMapData.hexagon(UmmMapData.cell_to_world(cell))
            points.append(points[0])
            draw_polyline(points, Color(0.05, 0.10, 0.06, 0.65), 2.8 * pixel, true)
            draw_polyline(points, Color(0.88, 0.94, 0.76, 0.65), 1.1 * pixel, true)
    if data.tiles.has(hover):
        var center := UmmMapData.cell_to_world(hover)
        var points := UmmMapData.hexagon(center, 3.0)
        var color := Color("fff0a0") if data.is_passable(hover) else Color("c05b40")
        draw_colored_polygon(points, Color(color, 0.14))
        points.append(points[0])
        draw_polyline(points, color, 2.0)
    if not path.is_empty():
        var points := PackedVector2Array([UmmMapData.cell_to_world(origin)])
        for cell in path:
            points.append(UmmMapData.cell_to_world(cell))
        # Today's stretch is gold; the rest of the route (tomorrow onward) is faded.
        var today := points.size() if reachable < 0 else reachable + 1
        draw_polyline(points, Color("4a200a"), 6.0)
        draw_polyline(points, Color("b8a080"), 2.0)
        if today > 1:
            draw_polyline(points.slice(0, today), Color("f8c840"), 2.0)
        for i in points.size():
            draw_circle(points[i], 4.0, Color("fff0a0") if i < today else Color("9a8468"))
        draw_arc(points[-1], 14, 0, TAU, 24, Color("fff0a0") if today >= points.size() else Color("c07850"), 2.0)
