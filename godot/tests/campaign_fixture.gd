extends RefCounted
## Fixture campaigns written to user:// for the smoke tests, so the game flow
## can be driven before (and independently of) the real data/campaign.json.

## Two tiny organic maps in two chapters. Map "a": a start-of-map choice
## (one option locked behind a flag the player does not have) whose answer
## sets the flag "mercy", gives an item, troops and XP past the map's cap;
## visiting the Old Stone with that flag set ends the map (next: "b").
## Map "b": a start line gated on choice.oath == "spare" (and one on the
## absence of mercy, which must not play); visiting the Far Stone ends the
## campaign. Returns the campaign file's path.
static func write_two_maps(dir: String) -> String:
    DirAccess.make_dir_recursive_absolute(dir)
    _write(dir.path_join("a.json"), _map("The Vale of Oaths", _blob([Vector2i(-1, 0), Vector2i(2, -1)], 2),
        [{"q": -1, "r": 0, "type": "town", "name": "Oakhall", "factionId": 1},
         {"q": 3, "r": -1, "type": "quest_giver", "name": "Old Stone"}]))
    _write(dir.path_join("a.triggers.json"), {
        "quests": [{"id": "stone", "title": "Reach the Old Stone", "main": true,
            "text": "Walk east to the Old Stone once you have given your answer."}],
        "triggers": [
            {"id": "intro", "when": {"event": "start"}, "do": [{"quest": "stone"}, {"choice": {
                "id": "oath", "title": "The Oath", "text": "The village waits for your word.",
                "options": [
                    {"id": "spare", "label": "Spare the village", "then": [
                        {"flag": {"mercy": true}}, {"item": "bearhide_cloak"},
                        {"troops": [{"id": "woad_runner", "count": 5}]}, {"xp": 400}]},
                    {"id": "burn", "label": "Burn the village", "requires_flag": "torch", "then": []}]}}]},
            {"id": "stone", "when": {"event": "visit", "name": "Old Stone", "flag": "mercy"}, "do": [
                {"quest_done": "stone"},
                {"ending": {"id": "vale_done", "heading": "The Stone Remembers", "body": "The vale is behind you.",
                    "frames": [{"scene": "road", "text": "The road runs east."}], "next": "b"}}]}]})
    var ribbon: Array[Vector2i] = []
    for q in range(-4, 5):
        for r in range(-1, 2):
            ribbon.append(Vector2i(q, r - (q + 4) / 3))
    _write(dir.path_join("b.json"), _map("The Ribbon Road", ribbon,
        [{"q": -4, "r": 0, "type": "town", "name": "Fording Camp", "factionId": 1},
         {"q": 4, "r": -2, "type": "quest_giver", "name": "Far Stone"}]))
    _write(dir.path_join("b.triggers.json"), {
        "start_companions": [],
        "triggers": [
            {"id": "remember", "when": {"event": "start", "flag": {"choice.oath": "spare"}},
                "do": [{"say": [["Ushari", "You spared them. The vale will remember it."]]}]},
            {"id": "forgot", "when": {"event": "start", "unless_flag": "mercy"},
                "do": [{"say": [["Ushari", "FLAG LEAK: mercy was not carried."]]}]},
            {"id": "far", "when": {"event": "visit", "name": "Far Stone"}, "do": [
                {"ending": {"id": "road_done", "heading": "The Far Stone", "body": "Here the fixture campaign ends.",
                    "frames": []}}]}]})
    var path := dir.path_join("campaign.json")
    _write(path, {"chapters": [
        {"id": "one", "title": "The Vale", "maps": [
            {"id": "a", "title": "The Vale of Oaths", "map": "a.json", "triggers": "a.triggers.json", "max_level": 2}]},
        {"id": "two", "title": "The Road East", "maps": [
            {"id": "b", "title": "The Ribbon Road", "map": "b.json", "triggers": "b.triggers.json", "max_level": 4}]}]})
    return path

## A one-map campaign of the old demo map (the legacy flow tests).
static func write_old_passage(dir: String) -> String:
    DirAccess.make_dir_recursive_absolute(dir)
    var path := dir.path_join("campaign.json")
    _write(path, {"chapters": [{"id": "north", "title": "The North", "maps": [
        {"id": "old_passage", "title": "The Old Passage", "map": "maps/old_passage.json",
         "encounters": "maps/old_passage.encounters.json", "triggers": "maps/old_passage.triggers.json"}]}]})
    return path

## A one-map campaign whose map has the id `map_id` (combat tips key on it):
## a town, and guard camps east of it named in `guards` ({name: [stacks]}).
## `companions` ride from the start; `triggers` are the map's triggers.
static func write_guard_map(dir: String, map_id: String, companions: Array, guards: Dictionary, triggers: Array) -> String:
    DirAccess.make_dir_recursive_absolute(dir)
    var objects: Array = [{"q": -2, "r": 0, "type": "town", "name": "Holdfast", "factionId": 1}]
    var spots := [Vector2i(2, 0), Vector2i(2, -2), Vector2i(0, 2)]
    var encounters := {}
    var i := 0
    for name in guards:
        objects.append({"q": spots[i].x, "r": spots[i].y, "type": "guard", "name": name})
        encounters[name] = {"guards": guards[name], "reward": {}}
        i += 1
    _write(dir.path_join("guards.json"), _map("The Guarded Vale", _blob([Vector2i(0, 0)], 3), objects))
    _write(dir.path_join("guards.encounters.json"), {"objects": encounters})
    _write(dir.path_join("guards.triggers.json"), {"start_companions": companions, "triggers": triggers})
    var path := dir.path_join("campaign.json")
    _write(path, {"chapters": [{"id": "one", "title": "The Vale", "maps": [
        {"id": map_id, "title": "The Guarded Vale", "map": "guards.json", "encounters": "guards.encounters.json",
         "triggers": "guards.triggers.json", "max_level": 3}]}]})
    return path

## Removes the saves a previous run left in `dir`.
static func clear_saves(dir: String) -> void:
    for file in ["campaign.save.json", "campaign_map.save.json", "campaign.resolved.json"]:
        if FileAccess.file_exists(dir.path_join(file)): DirAccess.remove_absolute(dir.path_join(file))

static func _blob(centers: Array, radius: int) -> Array[Vector2i]:
    var cells: Array[Vector2i] = []
    for center: Vector2i in centers:
        for q in range(-radius, radius + 1):
            for r in range(maxi(-radius, -q - radius), mini(radius, -q + radius) + 1):
                var cell: Vector2i = center + Vector2i(q, r)
                if not cell in cells: cells.append(cell)
    return cells

static func _map(title: String, cells: Array[Vector2i], objects: Array) -> Dictionary:
    var tiles: Array = []
    for cell in cells:
        var forest := posmod(cell.x * 7 + cell.y * 3, 5) == 0 and not objects.any(func(o): return o.q == cell.x and o.r == cell.y)
        tiles.append({"q": cell.x, "r": cell.y, "terrain": "forest" if forest else "grass", "passable": true,
            "moveCost": 1.5 if forest else 1.0})
    return {"version": 1, "name": title, "ground": "grass", "tiles": tiles, "objects": objects}

static func _write(path: String, value: Variant) -> void:
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_string(JSON.stringify(value, "  "))
    file.close()
