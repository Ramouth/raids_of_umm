class_name UmmMapData
extends RefCounted
## Presentation adapter for the existing version-1 C++ map format.
## Routing is temporary preview logic; gameplay authority will move to GDExtension.

const DIRECTIONS := [Vector2i(1, 0), Vector2i(1, -1), Vector2i(0, -1),
    Vector2i(-1, 0), Vector2i(-1, 1), Vector2i(0, 1)]
const HEX_SIZE := Vector2(112.0, 80.0)

var title := ""
var tiles: Dictionary = {}
var objects: Dictionary = {}
var spawn := Vector2i.ZERO
var error := ""

func read(path: String) -> bool:
    tiles.clear()
    objects.clear()
    error = ""
    if not FileAccess.file_exists(path):
        error = "Map missing. Run ./scripts/run_godot.sh --prepare first."
        return false
    var parser := JSON.new()
    if parser.parse(FileAccess.get_file_as_string(path)) != OK:
        error = "Invalid map JSON: " + parser.get_error_message()
        return false
    var document: Variant = parser.data
    if not document is Dictionary or document.get("version", 0) != 1:
        error = "Expected a version-1 Umm map."
        return false
    if not document.get("tiles") is Array or not document.get("objects", []) is Array:
        error = "Map tiles and objects must be arrays."
        return false
    title = str(document.get("name", "The Sands of Umm'Natur"))
    for entry: Variant in document.tiles:
        if not entry is Dictionary or not entry.has_all(["q", "r", "terrain"]):
            error = "A map tile is missing its coordinates or terrain."
            return false
        var cell := Vector2i(int(entry.q), int(entry.r))
        if tiles.has(cell):
            error = "Duplicate map coordinate: " + str(cell)
            return false
        tiles[cell] = entry
    for entry: Variant in document.get("objects", []):
        if not entry is Dictionary or not entry.has_all(["q", "r", "type"]):
            error = "A map object is missing its coordinates or type."
            return false
        var cell := Vector2i(int(entry.q), int(entry.r))
        if not tiles.has(cell):
            error = "An object lies outside the map: " + str(cell)
            return false
        objects[cell] = entry
    var found_spawn := false
    for cell: Vector2i in objects:
        if objects[cell].type == "town" and is_passable(cell):
            spawn = cell
            found_spawn = true
            break
    if not found_spawn:
        for cell: Vector2i in tiles:
            if is_passable(cell):
                spawn = cell
                found_spawn = true
                break
    if not found_spawn:
        error = "Map has no passable starting tile."
    return found_spawn

static func cell_to_world(cell: Vector2i) -> Vector2:
    return Vector2(cell.x * HEX_SIZE.x * 0.75, (cell.y + cell.x * 0.5) * HEX_SIZE.y)

static func world_to_cell(point: Vector2) -> Vector2i:
    var q := point.x / (HEX_SIZE.x * 0.75)
    var r := point.y / HEX_SIZE.y - q * 0.5
    var s := -q - r
    var rq := roundi(q)
    var rr := roundi(r)
    var rs := roundi(s)
    var delta := Vector3(absf(rq - q), absf(rr - r), absf(rs - s))
    if delta.x > delta.y and delta.x > delta.z:
        rq = -rr - rs
    elif delta.y > delta.z:
        rr = -rq - rs
    return Vector2i(rq, rr)

static func hexagon(center: Vector2, inset: float = 0.0) -> PackedVector2Array:
    var points := PackedVector2Array()
    for corner in range(6):
        var angle := corner * TAU / 6.0
        points.append(center + Vector2(cos(angle) * (HEX_SIZE.x / 2.0 - inset),
            sin(angle) * (HEX_SIZE.y / sqrt(3.0) - inset)))
    return points

func is_passable(cell: Vector2i) -> bool:
    return tiles.has(cell) and bool(tiles[cell].get("passable", true))

func step_cost(cell: Vector2i) -> float:
    var tile: Dictionary = tiles[cell]
    var cost := maxf(0.01, float(tile.get("moveCost", 1.0)))
    # Match WorldMap::loadFromFile, including maps whose road cost is already 0.5.
    return minf(cost, 0.5) if tile.get("road", false) else cost

func path_between(start: Vector2i, target: Vector2i) -> Array[Vector2i]:
    # Dijkstra respects arbitrary imported terrain costs, including half-cost roads.
    # AStarGrid2D is rectangular and would add invalid neighbors to an axial hex map.
    if not is_passable(start) or not is_passable(target) or start == target:
        return []
    var frontier: Array[Vector2i] = [start]
    var costs := {start: 0.0}
    var previous: Dictionary = {}
    while not frontier.is_empty():
        var best := 0
        for i in range(1, frontier.size()):
            if costs[frontier[i]] < costs[frontier[best]]:
                best = i
        var current: Vector2i = frontier.pop_at(best)
        if current == target:
            break
        for direction: Vector2i in DIRECTIONS:
            var next := current + direction
            if not is_passable(next):
                continue
            var cost: float = costs[current] + step_cost(next)
            if not costs.has(next) or cost < costs[next]:
                costs[next] = cost
                previous[next] = current
                if not frontier.has(next):
                    frontier.append(next)
    if not previous.has(target):
        return []
    var path: Array[Vector2i] = []
    var current := target
    while current != start:
        path.push_front(current)
        current = previous[current]
    return path

func path_cost(path: Array[Vector2i]) -> float:
    var cost := 0.0
    for cell in path:
        cost += step_cost(cell)
    return cost
