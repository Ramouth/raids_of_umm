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
const STARTING_ARMY := [{"id": "woad_runner", "count": 24}, {"id": "cruth_slinger", "count": 10}, {"id": "painted_blade", "count": 3}]
const ChapterScene = preload("res://scripts/chapter_scene.gd")
var _story_screen: Control

const EndScreen = preload("res://scripts/end_screen.gd")
## Fixed passage seed for tests; -1 picks a random old mine per expedition.
var passage_seed := -1
var unit_names: Dictionary = {}
var unit_defs: Dictionary = {}
var item_defs: Dictionary = {}
var _xp_title: Label
var _xp_bar: ProgressBar
var _xp_detail: Label
var _last_xp := -1
var _last_level := 1
var _chest_panel: Control
## The open HoMM3-style pop-up (site found, mine claimed, chest choice), if any.
var popup: Control
var _popup_queue: Array[Dictionary] = []
const TownScreen = preload("res://scripts/town_screen.gd")
const MapPopup = preload("res://scripts/map_popup.gd")
const HeroScreen = preload("res://scripts/hero_screen.gd")
const DialoguePanel = preload("res://scripts/dialogue_panel.gd")
const QuestLog = preload("res://scripts/quest_log.gd")
const GarrisonScreen = preload("res://scripts/garrison_screen.gd")
const PartyScreen = preload("res://scripts/party_screen.gd")
const TreeScreen = preload("res://scripts/tree_screen.gd")
var _garrison_button: Button
var _town_button: Button
var dialogue: PanelContainer
var quest_log: PanelContainer
var _offer_box: VBoxContainer
var _held_lines: Array = []  # dialogue from a journey, shown on arrival
var _rival_sprites: Dictionary = {}  # rival id -> Node2D on the map
var _quest_count := 0
const CombatView = preload("res://scripts/combat.gd")
const FogOverlay = preload("res://scripts/fog_overlay.gd")
const RESOURCES := ["Gold", "Wood", "Stone", "Obsidian", "Crystal"]
## Native rules (calendar, movement points, fog, ownership). Null if the extension is missing.
var adventure: RefCounted
var state: Dictionary = {}
var fog: Node2D
var _pending_steps: Array = []
var _destination := NO_CELL
## HoMM3 routes: the first click plans (the route stays drawn), a second click
## on the same hex sets off; Esc or a right-click cancels. Kept across days
## until the hero gets there.
var _planned := NO_CELL
var _right_press := Vector2.ZERO
var _calendar_label: Label
var _treasury_label: Label
var _moves_label: Label
var _end_day_button: Button
## Mirrors the native session's army; assigning pushes it to the session.
var army: Array = STARTING_ARMY.duplicate(true):
    set(value):
        army = value
        if adventure != null:
            state = JSON.parse_string(adventure.set_army(JSON.stringify(value)))
var inventory: Array = []
var cleared_dungeons: Dictionary = {}
var battle: Control
## Injected by game.gd; when the scene runs alone (tests, F6) it makes its own.
var screens: ScreenStack
var _enter_button: Button
var _army_label: Label
var _restart_button: Button
var encounter: Dictionary = {}

func _ready() -> void:
    if screens == null:
        screens = ScreenStack.new()
        screens.name = "Screens"
        screens.base = self
        add_child(screens)
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
    fog = FogOverlay.new()
    fog.name = "Fog"
    fog.data = data
    map_view.add_child(fog)
    if not _start_adventure():
        return
    hero.place_at(_hero_cell())
    hero.entered_cell.connect(_entered_cell)
    hero.journey_finished.connect(_journey_finished)
    sidebar.get_node("Grid").toggled.connect(_toggle_grid)
    sidebar.get_node("Center").pressed.connect(center_hero)
    sidebar.get_node("Overview").pressed.connect(fit_map)
    sidebar.get_node("Portrait").texture = load("res://content/textures/units/british_lord.png")
    encounter = JSON.parse_string(FileAccess.get_file_as_string("res://content_source/dungeon_encounter.json"))
    var units: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://content/data/units.json"))
    if units is Dictionary:
        for unit in units.units:
            unit_names[unit.id] = unit.name
            unit_defs[unit.id] = unit
    var items: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://content/data/items.json"))
    if items is Dictionary:
        for item in items.items: item_defs[item.id] = item
    _build_expedition_controls()
    _build_turn_hud()
    $HUD/Layout/Header/Text/Title.text = data.title
    _apply_state(state)
    _quest_count = state.get("quests", []).size()
    _say(state)
    get_viewport().size_changed.connect(_resize_hud)
    _resize_hud()
    _entered_cell(hero.cell)
    fit_map()

func _start_adventure() -> bool:
    if not ClassDB.class_exists("UmmAdventure"):
        notice.text = "Native adventure rules are missing. Rebuild with ./scripts/run_godot.sh."
        set_process(false)
        set_process_unhandled_input(false)
        return false
    adventure = ClassDB.instantiate("UmmAdventure")
    var encounters: String = str(map_view.map_path).get_basename() + ".encounters.json"
    var triggers: String = str(map_view.map_path).get_basename() + ".triggers.json"
    var passage: int = passage_seed if passage_seed >= 0 else randi() % 1000000
    state = JSON.parse_string(adventure.start(ProjectSettings.globalize_path(map_view.map_path),
        ProjectSettings.globalize_path("res://content/data"),
        ProjectSettings.globalize_path(encounters) if FileAccess.file_exists(encounters) else "", passage,
        ProjectSettings.globalize_path(triggers) if FileAccess.file_exists(triggers) else ""))
    _quest_count = 0
    if not state.get("ok", false):
        notice.text = "Could not start the adventure: " + str(state.get("error", "unknown error"))
        set_process(false)
        set_process_unhandled_input(false)
        return false
    var intro: Array = state.get("lines", [])
    army = army  # push the expedition's army into the new session (replaces state)
    state.lines = intro
    return true

## Called by the town screen. Returns the native reply (ok/error + snapshot).
func recruit(town: Vector2i, unit_id: String, count: int) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.recruit(town.x, town.y, unit_id, count))
    if reply.get("ok", false):
        state = reply
        army = reply.army.duplicate(true)
        _update_turn_hud()
        _update_expedition()
    return reply

## Called by the town screen: build in the town on `town` (one per day).
func build(town: Vector2i, building_id: String) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.build(town.x, town.y, building_id))
    if reply.get("ok", false):
        state = reply
        _update_turn_hud()
        _update_expedition()
    return reply

## Called by the town screen's marketplace.
func trade(give: String, get: String, amount: int) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.trade(give, get, amount))
    if reply.get("ok", false):
        state = reply
        _update_turn_hud()
    return reply

func trade_quote(give: String, get: String, amount: int) -> int:
    return int(JSON.parse_string(adventure.trade_quote(give, get, amount)).get("received", 0))

## Called by the garrison screen. Returns the native reply.
func transfer(site: Vector2i, unit_id: String, count: int, to_garrison: bool) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.transfer(site.x, site.y, unit_id, count, to_garrison))
    if reply.get("ok", false):
        state = reply
        army = reply.army.duplicate(true)
        _update_expedition()
    return reply

## Called by the party screen. Returns the native reply.
func station(id: String, stay: bool) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.station(id, stay))
    if reply.get("ok", false):
        _apply_state(reply)
        _say(reply)
    return reply

func can_govern() -> bool:
    return _owned_town(hero.cell) and data.objects.get(hero.cell, {}).get("type", "") == "town"

func open_party() -> bool:
    if hero.moving or is_instance_valid(battle): return false
    var screen := PartyScreen.new()
    screen.host = self
    screens.push(screen, func(_result: Dictionary):
        _apply_state(state)
        _update_expedition())
    return true

## The commander's path (K): chosen at level 2; each level adds its next skill.
func open_tree() -> bool:
    if hero.moving or is_instance_valid(battle): return false
    var screen := TreeScreen.new()
    screen.host = self
    screens.push(screen, func(_result: Dictionary):
        _apply_state(state)
        _update_expedition())
    return true

func choose_path(id: String) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.choose_path(id))
    if reply.has("day"): _apply_state(reply)
    return reply

func open_hero() -> bool:
    if hero.moving or is_instance_valid(battle): return false
    var screen := HeroScreen.new()
    screen.host = self
    screens.push(screen, func(_result: Dictionary):
        _apply_state(state)
        _update_expedition())
    return true

## Called by the hero screen: the commander's paper doll.
func equip(item_id: String) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.equip(item_id))
    if reply.get("ok", false): state = reply
    return reply

func unequip(slot: String) -> Dictionary:
    var reply: Dictionary = JSON.parse_string(adventure.unequip(slot))
    if reply.get("ok", false): state = reply
    return reply

func open_garrison() -> bool:
    if hero.moving or is_instance_valid(battle) or not _owned_site(hero.cell): return false
    var screen := GarrisonScreen.new()
    screen.host = self
    screen.cell = hero.cell
    screen.title = str(data.objects.get(hero.cell, {}).get("name", "Garrison"))
    screens.push(screen, func(_result: Dictionary):
        _apply_state(state)
        _update_expedition())
    return true

func _owned_site(cell: Vector2i) -> bool:
    for o in state.get("owners", []):
        if o[0] == cell.x and o[1] == cell.y: return int(o[2]) == 1
    return false

func open_town() -> bool:
    if hero.moving or is_instance_valid(battle) or not _owned_town(hero.cell): return false
    var screen := TownScreen.new()
    screen.host = self
    screen.cell = hero.cell
    var landmark: Dictionary = data.objects.get(hero.cell, {})
    if landmark.get("type", "") == "dwelling":
        screen.view_art = "res://content/textures/" + map_view.object_texture(landmark)
        screen.eyebrow_text = "DWELLING  ·  RECRUITS GATHER WEEKLY"
        screen.can_build = false
    elif data.ground == "grass" and ResourceLoader.exists("res://content/textures/screens/town_varenhold.png"):
        screen.view_art = "res://content/textures/screens/town_varenhold.png"
    screens.push(screen, func(_result: Dictionary):
        _apply_state(state)
        _update_expedition())
    return true

func _owned_town(cell: Vector2i) -> bool:
    for town in state.get("towns", []):
        if town.cell[0] == cell.x and town.cell[1] == cell.y: return int(town.owner) == 1
    return false

func _hero_cell() -> Vector2i:
    return Vector2i(state.hero[0], state.hero[1])

func _apply_state(next: Dictionary) -> void:
    state = next
    fog.apply(state)
    var ruled: Dictionary = {}
    for c in state.get("ruled_out", []): ruled[Vector2i(c[0], c[1])] = true
    map_view.mark_ruled_out(ruled)
    _update_offers()
    var quests: Array = state.get("quests", [])
    if quests.size() > _quest_count and _quest_count > 0:
        notice.text = "New quest: %s  (Q to open the quest log)" % quests[-1].title
    _quest_count = quests.size()
    if quest_log != null and quest_log.visible: quest_log.show_quests(quests)
    var guarded: Dictionary = {}
    for c in state.get("guarded", []): guarded[Vector2i(c[0], c[1])] = true
    map_view.set_cleared_guards(guarded)
    map_view.remove_objects(state.get("vanished", []))
    map_view.mark_sites(state.get("sites", []))
    _show_xp_gain()
    _announce_levels(next)
    _sync_inventory()
    if quest_log != null and quest_log.visible: quest_log.show_quests(quests, state.get("lore", []))
    _sync_rivals()
    _update_turn_hud()

## Each level-up pops up (HoMM3 style): what the level brought, or, at the
## first one, the choice of a path, which opens the path screen.
func _announce_levels(reply: Dictionary) -> void:
    var ups: Array = reply.get("level_ups", [])
    reply.erase("level_ups")          # the same reply is applied again later
    for up in ups:
        var skill := str(up.get("skill", ""))
        var spec := {"title": "LEVEL %d" % int(up.level), "picture": "units/british_lord.png"}
        if skill.is_empty() and state.get("tree", {}).get("needs_path", false):
            spec.flavour = "Your commander has earned a name among the Cruths. Choose a path: Marshal, Siegemaster or Quartermaster. It is chosen for good."
            spec.buttons = ["Choose a path"]
            show_popup(spec, func(_choice: int): open_tree.call_deferred())
        elif not skill.is_empty():
            spec.flavour = "Your commander grows in the %s's craft." % str(state.get("tree", {}).get("chosen", "")).capitalize()
            spec.reward = "%s: %s" % [skill, str(up.get("text", ""))]
            show_popup(spec)
        else:
            show_popup(spec)

## Every experience gain floats over the hero; a level-up says what it bought.
func _show_xp_gain() -> void:
    var hp: Dictionary = state.get("hero_progress", {})
    if hp.is_empty(): return
    var xp := int(hp.xp)
    if _last_xp >= 0 and xp > _last_xp:
        var text := "+%d XP" % (xp - _last_xp)
        if int(hp.level) > _last_level:
            text += "   LEVEL %d!" % int(hp.level)
            notice.text = "Level %d reached." % int(hp.level)
        _float_text(text, Color("f0d070") if int(hp.level) == _last_level else Color("ffe9a0"))
    _last_xp = xp
    _last_level = int(hp.level)

func _float_text(text: String, color: Color) -> void:
    if hero == null or map_view == null: return
    var label := Label.new()
    label.text = text
    label.add_theme_font_size_override("font_size", 20)
    label.add_theme_color_override("font_color", color)
    label.add_theme_color_override("font_outline_color", Color("2a1a10"))
    label.add_theme_constant_override("outline_size", 6)
    label.z_index = 50
    label.position = UmmMapData.cell_to_world(hero.cell) + Vector2(-60, -110)
    label.size = Vector2(160, 30)
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    map_view.add_child(label)
    var tween := label.create_tween()
    tween.tween_property(label, "position:y", label.position.y - 50, 1.6)
    tween.parallel().tween_property(label, "modulate:a", 0.0, 1.6).set_delay(0.6)
    tween.tween_callback(label.queue_free)

## Experience a victory at 'cell' would give (from the native preview), 0 if none.
func _xp_at(cell: Vector2i) -> int:
    for e in state.get("encounter_xp", []):
        if e[0] == cell.x and e[1] == cell.y: return int(e[2])
    return 0

## "reaches level 3" when a gain would cross the next threshold.
func _level_note(gain: int) -> String:
    var hp: Dictionary = state.get("hero_progress", {})
    if hp.is_empty(): return ""
    return "  → level %d!" % (int(hp.level) + 1) if int(hp.xp) + gain >= int(hp.next) else ""

## Items handed over by the story or found at sites join the combat loot list.
func _sync_inventory() -> void:
    var held: Dictionary = {}
    for item in inventory: held[str(item.id)] = true
    for id in state.get("items", []):
        if held.has(str(id)): continue
        var def: Dictionary = item_defs.get(str(id), {})
        inventory.append({"id": str(id), "name": str(def.get("name", str(id).capitalize())),
            "description": str(def.get("description", ""))})

## HoMM3 treasure chest: gold now, or experience for the companions.
func claim_chest(gold: bool) -> bool:
    if state.get("chest") == null: return false
    var reply: Dictionary = JSON.parse_string(adventure.claim_chest(gold))
    _apply_state(reply)
    _say(reply)
    notice.text = str(reply.get("found", ""))
    if is_instance_valid(_chest_panel):
        if _chest_panel == popup: popup = null
        _chest_panel.queue_free()
    _update_expedition()
    return true

func _offer_chest(chest: Dictionary) -> void:
    if is_instance_valid(_chest_panel): return
    var landmark: Dictionary = data.objects.get(hero.cell, {})
    show_popup({"title": str(landmark.get("name", "A chest")),
        "flavour": "The lid is stiff with frost. Inside: coin, and a soldier's journal that someone kept for a long time.",
        "picture": map_view.object_texture(landmark),
        "buttons": ["Keep the gold  (+%d Gold)" % int(chest.gold), "Read the journal  (+%d XP%s)" % [int(chest.xp), _level_note(int(chest.xp))]]},
        func(choice: int): claim_chest(choice == 0))
    _chest_panel = popup

## HoMM3 pop-up: {title, flavour, reward, picture (texture path under content/textures), buttons}.
## Queued if one is already open; `on_close` receives the chosen button's index.
func show_popup(spec: Dictionary, on_close := Callable()) -> void:
    if is_instance_valid(popup):
        _popup_queue.append({"spec": spec, "on_close": on_close})
        return
    var box := MapPopup.new()
    box.name = "MapPopup"
    box.title = str(spec.get("title", ""))
    box.flavour = str(spec.get("flavour", ""))
    box.reward = str(spec.get("reward", ""))
    var art := str(spec.get("picture", ""))
    if not art.is_empty() and ResourceLoader.exists("res://content/textures/" + art): box.picture = load("res://content/textures/" + art)
    var labels: Array[String] = []
    for text in spec.get("buttons", ["OK"]): labels.append(str(text))
    box.buttons = labels
    box.theme = $HUD/Layout.theme
    box.closed.connect(func(choice: int):
        popup = null
        if on_close.is_valid(): on_close.call(choice)
        if not _popup_queue.is_empty():
            var next: Dictionary = _popup_queue.pop_front()
            show_popup(next.spec, next.on_close)
        elif hero.moving: hero.resume_journey())
    popup = box
    if hero.moving: hero.pause_journey()
    $HUD.add_child(box)

## A site's pop-up from the engine's "Name: what happened" line.
func _site_popup(cell: Vector2i, found: String) -> Dictionary:
    var landmark: Dictionary = data.objects.get(cell, {})
    var name := str(landmark.get("name", ""))
    var body := found.trim_prefix(name + ": ") if not name.is_empty() else found
    var kind := str(landmark.get("kind", ""))
    var flavour := ""
    match str(landmark.get("type", "")):
        "pickup":
            flavour = {"wood": "A stack of cut timber, left to weather under the pines.",
                "stone": "Dressed stone from a fallen wall, ready to haul.",
                "gold": "A soldier's purse, dropped in the grass and never missed.",
                "crystal": "Frost-clear crystals glitter between the roots.",
                "obsidian": "Black glass, sharp as the day the mountain spat it out.",
                "campfire": "An abandoned campfire. Whoever sat here left in a hurry, and left their goods."}.get(kind, "Something worth taking lies here.")
        "artifact": flavour = "Half-buried in the earth, something waits for a hand to lift it."
        "mill": flavour = "The miller counts out this week's due." if not body.contains("nothing more") else "The miller shakes his head: you have had this week's due."
        "stables": flavour = "The stable master brings out fresh horses."
        "watchtower": flavour = "From the top of the tower, the land for leagues around lies open."
        "learning_stone": flavour = "Runes cut deep into the stone. Your company reads them by torchlight."
        "obelisk": flavour = "A black obelisk, older than any house in the marches. Veins of light move under its surface."
    if not body.is_empty(): body = body.left(1).to_upper() + body.substr(1)
    return {"title": name if not name.is_empty() else "Found", "flavour": flavour, "reward": body,
        "picture": map_view.object_texture(landmark) if not landmark.is_empty() else ""}

## What a site does, for the sidebar (HoMM3 right-click text).
func _site_text(cell: Vector2i, landmark: Dictionary) -> String:
    var used := false
    for entry in state.get("sites", []):
        if entry.cell[0] == cell.x and entry.cell[1] == cell.y: used = entry.used
    match str(landmark.get("type", "")):
        "pickup":
            var kind := str(landmark.get("kind", ""))
            if kind == "chest": return "A chest: gold, or experience for you and your companions."
            if kind == "campfire": return "An abandoned camp: gold and timber."
            return "Resources lying unclaimed: %s." % kind.capitalize()
        "artifact": return "Something lies half-buried here." + (" (taken)" if used else "")
        "mill":
            var pay := "+500 Gold" if landmark.get("kind", "") == "watermill" else "+2 Wood, +2 Stone"
            return "Pays %s once a week.%s" % [pay, " Visited this week." if used else ""]
        "stables": return "Fresh horses: +3 movement until the week ends.%s" % [" Used this week." if used else ""]
        "watchtower": return "Climb it to see far across the land.%s" % [" Visited." if used else ""]
        "learning_stone": return "Runes that teach: +400 XP, once.%s%s" % [_level_note(400) if not used else "", " Read." if used else ""]
        "obelisk": return "A black stone veined with violet. It shows where the passage is not.%s" % [" Read." if used else ""]
        "dwelling":
            var unit := str(unit_names.get(str(landmark.get("kind", "")), str(landmark.get("kind", "")).capitalize()))
            return "A dwelling: capture it to recruit %s each week." % unit
    return ""

## Shows war-bands the player can currently see; hides the rest.
func _sync_rivals() -> void:
    var seen: Dictionary = {}
    for rival in state.get("rivals", []):
        var id := int(rival.id)
        seen[id] = true
        var sprite: Node2D = _rival_sprites.get(id)
        if sprite == null:
            sprite = _make_rival_sprite(str(rival.name))
            _rival_sprites[id] = sprite
        sprite.visible = true
        if not sprite.has_meta("moving"):
            sprite.position = UmmMapData.cell_to_world(Vector2i(rival.cell[0], rival.cell[1]))
    for id in _rival_sprites:
        if not seen.has(id) and not _rival_sprites[id].has_meta("moving"):
            _rival_sprites[id].visible = false

func _make_rival_sprite(title: String) -> Node2D:
    var anchor := Node2D.new()
    anchor.name = title.validate_node_name()
    map_view.get_node("Scenery").add_child(anchor)
    var shadow := Polygon2D.new()
    var ring := PackedVector2Array()
    for i in 16: ring.append(Vector2(cos(i * TAU / 16) * 22, sin(i * TAU / 16) * 9))
    shadow.polygon = ring
    shadow.color = Color(0.72, 0.25, 0.17, 0.55)
    anchor.add_child(shadow)
    var sprite := Sprite2D.new()
    sprite.texture = load("res://content/textures/units/shariw_hero.png")
    sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    sprite.scale = Vector2(0.5, 0.5)
    sprite.position.y = -28
    anchor.add_child(sprite)
    var label := Label.new()
    label.text = title
    label.position = Vector2(-80, 4)
    label.size = Vector2(160, 20)
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.add_theme_font_size_override("font_size", 12)
    label.add_theme_color_override("font_color", Color("ffb0a0"))
    label.add_theme_color_override("font_outline_color", Color("351810"))
    label.add_theme_constant_override("outline_size", 5)
    anchor.add_child(label)
    return anchor

## Animates the war-band moves the player could see during the Shariw turn.
func _animate_rivals(moves: Array) -> void:
    var tweens: Array[Tween] = []
    for move in moves:
        var sprite: Node2D = _rival_sprites.get(int(move.id))
        if sprite == null:
            sprite = _make_rival_sprite("Shariw war-band")
            _rival_sprites[int(move.id)] = sprite
        sprite.visible = true
        sprite.set_meta("moving", true)
        var tween := create_tween()
        for c in move.path:
            tween.tween_property(sprite, "position", UmmMapData.cell_to_world(Vector2i(c[0], c[1])), 0.16)
        tween.tween_callback(func(): sprite.remove_meta("moving"))
        tweens.append(tween)
    for tween in tweens:
        if tween.is_running(): await tween.finished
    _sync_rivals()

func _rival_at(cell: Vector2i) -> Dictionary:
    for rival in state.get("rivals", []):
        if rival.cell[0] == cell.x and rival.cell[1] == cell.y: return rival
    return {}

## Ends the scenario when the Shariw sealed the passage.
func _check_lost() -> bool:
    if not state.get("lost", false): return false
    _show_end(false, str(state.get("lost_reason", "")))
    return true

func _guarded(cell: Vector2i) -> bool:
    if not _rival_at(cell).is_empty(): return true
    for c in state.get("guarded", []):
        if c[0] == cell.x and c[1] == cell.y: return true
    return false

func _army_text(stacks: Array) -> String:
    var parts: Array[String] = []
    for stack in stacks: parts.append("%d %s" % [stack.count, unit_names.get(stack.id, str(stack.id).replace("_", " "))])
    return ", ".join(parts)

## Shows dialogue carried by a native reply (WC3 transmission).
func _say(reply: Dictionary) -> void:
    var lines: Array = reply.get("lines", [])
    if not lines.is_empty() and dialogue != null: dialogue.say(lines)

func _update_offers() -> void:
    if _offer_box == null: return
    for child in _offer_box.get_children(): child.queue_free()
    for offer in state.get("offers", []):
        var button := Button.new()
        button.text = str(offer.label)
        button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
        button.pressed.connect(accept_offer.bind(str(offer.id)))
        _offer_box.add_child(button)

func accept_offer(id: String) -> bool:
    var reply: Dictionary = JSON.parse_string(adventure.accept_offer(id))
    if not reply.get("ok", false):
        notice.text = str(reply.get("error", "That offer is not available."))
        return false
    _apply_state(reply)
    _say(reply)
    return true

const SAVE_PATH := "user://quicksave.json"

func save_game() -> bool:
    if adventure == null or hero.moving or is_instance_valid(battle) or turn_busy: return false
    var extra := {"inventory": inventory, "cleared_dungeons": cleared_dungeons.keys().map(func(c): return [c.x, c.y])}
    var reply: Dictionary = JSON.parse_string(adventure.save_game(ProjectSettings.globalize_path(SAVE_PATH), JSON.stringify(extra)))
    notice.text = "Game saved (F9 to load)." if reply.get("ok", false) else str(reply.get("error", "Could not save."))
    return reply.get("ok", false)

func load_game() -> bool:
    if adventure == null or hero.moving or is_instance_valid(battle) or turn_busy: return false
    var reply: Dictionary = JSON.parse_string(adventure.load_game(ProjectSettings.globalize_path(SAVE_PATH)))
    if not reply.get("ok", false):
        notice.text = str(reply.get("error", "Could not load."))
        return false
    var extra: Dictionary = reply.get("extra", {})
    inventory = extra.get("inventory", [])
    cleared_dungeons.clear()
    for c in extra.get("cleared_dungeons", []): cleared_dungeons[Vector2i(c[0], c[1])] = true
    if dialogue != null: dialogue.skip_all()
    for id in _rival_sprites: _rival_sprites[id].queue_free()
    _rival_sprites.clear()
    state = reply
    army = reply.army.duplicate(true)
    state = reply
    _last_xp = -1  # a loaded game is not a gain
    _quest_count = state.get("quests", []).size()
    _apply_state(state)
    hero.place_at(_hero_cell())
    _entered_cell(hero.cell)
    center_hero()
    notice.text = "Game loaded: day %d of week %d." % [state.day_of_week, state.week]
    return true

func toggle_quest_log() -> void:
    quest_log.visible = not quest_log.visible
    if quest_log.visible: quest_log.show_quests(state.get("quests", []), state.get("lore", []))

func _build_turn_hud() -> void:
    dialogue = DialoguePanel.new()
    dialogue.name = "Dialogue"
    $HUD/Layout.add_child(dialogue)
    dialogue.position = Vector2(28, 600)
    quest_log = QuestLog.new()
    quest_log.name = "QuestLog"
    $HUD/Layout.add_child(quest_log)
    quest_log.position = Vector2(40, 170)
    _offer_box = VBoxContainer.new()
    _offer_box.name = "Offers"
    sidebar.add_child(_offer_box)
    sidebar.move_child(_offer_box, sidebar.get_node("Spacer").get_index())
    var commander := Button.new()
    commander.name = "Hero"
    commander.text = "Hero & items     H"
    commander.pressed.connect(open_hero)
    sidebar.add_child(commander)
    sidebar.move_child(commander, sidebar.get_node("Grid").get_index())
    var tree := Button.new()
    tree.name = "Tree"
    tree.text = "Commander's path     K"
    tree.pressed.connect(open_tree)
    sidebar.add_child(tree)
    sidebar.move_child(tree, sidebar.get_node("Grid").get_index())
    var party := Button.new()
    party.name = "Party"
    party.text = "Companions     P"
    party.pressed.connect(open_party)
    sidebar.add_child(party)
    sidebar.move_child(party, sidebar.get_node("Grid").get_index())
    var quests := Button.new()
    quests.name = "Quests"
    quests.text = "Quest log     Q"
    quests.pressed.connect(toggle_quest_log)
    sidebar.add_child(quests)
    sidebar.move_child(quests, sidebar.get_node("Grid").get_index())
    _calendar_label = $HUD/Layout/Header/Text/Eyebrow
    _treasury_label = Label.new()
    _treasury_label.name = "Treasury"
    _treasury_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    _treasury_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
    _treasury_label.add_theme_font_size_override("font_size", 14)
    $HUD/Layout/Header.add_child(_treasury_label)
    _moves_label = Label.new()
    _moves_label.name = "Moves"
    _moves_label.add_theme_font_size_override("font_size", 13)
    sidebar.add_child(_moves_label)
    sidebar.move_child(_moves_label, sidebar.get_node("Location").get_index() + 1)
    # Experience: always on screen, with what the next level brings.
    _xp_title = Label.new()
    _xp_title.name = "XpTitle"
    _xp_title.add_theme_font_size_override("font_size", 13)
    _xp_title.add_theme_color_override("font_color", Color("f0c870"))
    sidebar.add_child(_xp_title)
    sidebar.move_child(_xp_title, _moves_label.get_index() + 1)
    _xp_bar = ProgressBar.new()
    _xp_bar.name = "XpBar"
    _xp_bar.show_percentage = false
    _xp_bar.custom_minimum_size = Vector2(0, 10)
    var fill := StyleBoxFlat.new()
    fill.bg_color = Color("c9a24a")
    var back := StyleBoxFlat.new()
    back.bg_color = Color(0.2, 0.15, 0.1)
    back.border_color = Color("6b5230")
    back.set_border_width_all(1)
    _xp_bar.add_theme_stylebox_override("fill", fill)
    _xp_bar.add_theme_stylebox_override("background", back)
    sidebar.add_child(_xp_bar)
    sidebar.move_child(_xp_bar, _xp_title.get_index() + 1)
    _xp_detail = Label.new()
    _xp_detail.name = "XpDetail"
    _xp_detail.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    _xp_detail.add_theme_font_size_override("font_size", 12)
    _xp_detail.add_theme_color_override("font_color", Color("c8b08a"))
    sidebar.add_child(_xp_detail)
    sidebar.move_child(_xp_detail, _xp_bar.get_index() + 1)
    _end_day_button = Button.new()
    _end_day_button.name = "EndDay"
    _end_day_button.text = "End day     E"
    _end_day_button.pressed.connect(end_day)
    sidebar.add_child(_end_day_button)
    sidebar.move_child(_end_day_button, sidebar.get_node("Grid").get_index())
    $HUD/Layout/Footer.text = "CLICK travel  •  E end day  •  T town  •  R garrison  •  P companions  •  Q quests  •  F5 save  •  F9 load"

func _update_turn_hud() -> void:
    if not is_instance_valid(_calendar_label): return
    _calendar_label.text = "DAY %d   ·   WEEK %d   ·   MONTH %d" % [state.day_of_week, state.week, state.month]
    var parts: Array[String] = []
    for resource in RESOURCES:
        var line := "%s %d" % [resource, state.treasury[resource]]
        var net := int(state.income[resource]) - (int(state.get("upkeep", 0)) if resource == "Gold" else 0)
        if net != 0:
            line += " (%+d)" % net
        parts.append(line)
    _treasury_label.text = "    ".join(parts)
    _moves_label.text = "Movement  %.1f / %.0f" % [state.moves, state.moves_max]
    var hp: Dictionary = state.get("hero_progress", {})
    if is_instance_valid(_xp_bar) and not hp.is_empty():
        var span := maxi(1, int(hp.next) - int(hp.prev))
        var needs: bool = state.get("tree", {}).get("needs_path", false)
        _xp_title.text = "Commander  ·  Level %d%s" % [int(hp.level), "   ·   choose your path (K)" if needs else ""]
        _xp_bar.max_value = span
        _xp_bar.value = int(hp.xp) - int(hp.prev)
        if int(hp.level) >= int(hp.get("max", 10)):
            _xp_detail.text = "The height of a commander's craft: level %d." % int(hp.level)
        else:
            _xp_detail.text = "%d / %d XP  ·  %d to level %d" % [
                int(hp.xp) - int(hp.prev), span, int(hp.next) - int(hp.xp), int(hp.level) + 1]
    _end_day_button.disabled = hero.moving or is_instance_valid(battle) or turn_busy or not state.get("story_choice", {}).is_empty() or state.get("won", false)

## True while the Shariw turn animates; blocks another End Day.
var turn_busy := false

func end_day() -> void:
    if not state.get("story_choice", {}).is_empty() or state.get("won", false): return
    if adventure == null or hero.moving or is_instance_valid(battle) or turn_busy: return
    var next: Dictionary = JSON.parse_string(adventure.end_day())
    if not next.get("ok", false): return
    _apply_state(next)
    _say(next)
    var message := "Day %d of week %d dawns. Your expedition is rested." % [state.day_of_week, state.week]
    if not str(next.get("event", "")).is_empty():
        message += " " + str(next.event)
    notice.text = message
    _inspect(_hover)
    if _check_lost(): return
    var moves: Array = next.get("rival_moves", [])
    if not moves.is_empty():
        turn_busy = true
        _update_turn_hud()
        await _animate_rivals(moves)
        turn_busy = false
        _update_turn_hud()
    if state.get("encounter") != null and not is_instance_valid(battle):
        _engage()

func _resize_hud() -> void:
    var width := get_viewport_rect().size.x
    $HUD/Layout/Header.offset_right = width - 324.0
    $HUD/Layout/Footer.offset_right = width - 324.0
    $HUD/Layout/Notice.offset_right = width - 324.0

func _process(delta: float) -> void:
    _present_story()
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
    if is_instance_valid(popup): return   # the pop-up has the player's attention
    if event is InputEventMouseButton:
        if event.button_index in [MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE]:
            _dragging = event.pressed and _over_map(event.position)
        if event.button_index == MOUSE_BUTTON_RIGHT:   # a right-click (not a drag) cancels the plan
            if event.pressed: _right_press = event.position
            elif event.position.distance_to(_right_press) < 6.0 and _planned != NO_CELL: cancel_route()
        if not event.pressed or not _over_map(event.position):
            return
        if event.button_index == MOUSE_BUTTON_WHEEL_UP:
            _zoom(1)
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            _zoom(-1)
        elif event.button_index == MOUSE_BUTTON_LEFT:
            var world_point: Vector2 = get_canvas_transform().affine_inverse() * event.position
            click_cell(UmmMapData.world_to_cell(world_point))
    elif event is InputEventMouseMotion and _dragging:
        camera.position -= event.relative / camera.zoom
        _clamp_camera()
    elif event is InputEventKey and event.pressed and not event.echo:
        match event.keycode:
            KEY_G:
                sidebar.get_node("Grid").button_pressed = not overlay.show_grid
            KEY_SPACE, KEY_ENTER:
                if dialogue.is_speaking(): dialogue.advance()
                elif event.keycode == KEY_ENTER and _planned != NO_CELL: travel_to(_planned)
                elif event.keycode == KEY_SPACE: center_hero()
            KEY_Q:
                toggle_quest_log()
            KEY_P:
                open_party()
            KEY_H:
                open_hero()
            KEY_K:
                open_tree()
            KEY_F5:
                save_game()
            KEY_F9:
                load_game()
            KEY_HOME:
                fit_map()
            KEY_E:
                end_day()
            KEY_T:
                open_town()
            KEY_R:
                open_garrison()
            KEY_ESCAPE:
                if not hero.moving:
                    if _planned != NO_CELL: cancel_route()
                    else: _inspect(NO_CELL)

func _inspect(cell: Vector2i) -> void:
    _hover = cell
    overlay.hover = cell
    _preview.clear()
    overlay.reachable = -1
    if not data.tiles.has(cell):
        sidebar.get_node("Inspection").text = "Hover over the land to inspect terrain and landmarks."
        sidebar.get_node("Route").text = "Click a reachable hex to travel."
    elif fog != null and not fog.is_explored(cell):
        sidebar.get_node("Inspection").text = "Unexplored\n[%d, %d]" % [cell.x, cell.y]
        sidebar.get_node("Route").text = "Scout closer to chart a course."
    else:
        var tile: Dictionary = data.tiles[cell]
        var terrain := str(tile.terrain).capitalize()
        if tile.get("road", false):
            terrain += " · Road"
        var landmark: Dictionary = data.objects.get(cell, {})
        var heading := str(landmark.get("name", terrain))
        sidebar.get_node("Inspection").text = "%s\n%s · [%d, %d]" % [heading, terrain, cell.x, cell.y]
        var rival := _rival_at(cell)
        if not rival.is_empty():
            sidebar.get_node("Inspection").text = "%s\n%s" % [rival.name, _army_text(rival.army)]
        elif _guarded(cell):
            sidebar.get_node("Inspection").text += "\nGuarded by: " + _army_text(_encounters_for(landmark))
        var xp_here := _xp_at(cell)
        if xp_here > 0:
            var gold := int(_encounter_entry(landmark).get("reward", {}).get("Gold", 0))
            sidebar.get_node("Inspection").text += "\nVictory: %s+%d XP%s" % [
                ("%d gold  ·  " % gold) if gold > 0 else "", xp_here, _level_note(xp_here)]
        var about := _site_text(cell, landmark)
        if not about.is_empty():
            sidebar.get_node("Inspection").text += "\n" + about
        if not data.is_passable(cell):
            sidebar.get_node("Route").text = "Impassable terrain."
        elif cell == hero.cell:
            sidebar.get_node("Route").text = "Your expedition is here."
        else:
            var plan: Dictionary = JSON.parse_string(adventure.preview(cell.x, cell.y))
            for c in plan.path: _preview.append(Vector2i(c[0], c[1]))
            overlay.reachable = int(plan.reachable)
            if _preview.is_empty():
                sidebar.get_node("Route").text = "No route to this hex."
            elif plan.reachable >= _preview.size():
                var verb := "attack" if _guarded(cell) else "travel"
                sidebar.get_node("Route").text = "%d hexes · %.1f movement\nClick to %s." % [_preview.size(), plan.cost, verb]
            elif plan.reachable > 0:
                sidebar.get_node("Route").text = "%d hexes · %.1f movement\nToday you reach %d of them." % [_preview.size(), plan.cost, plan.reachable]
            else:
                sidebar.get_node("Route").text = "%d hexes · %.1f movement\nNo movement left today — end the day (E)." % [_preview.size(), plan.cost]
    overlay.origin = hero.cell
    overlay.path = _preview.duplicate()
    if _planned != NO_CELL and cell != _planned:   # the planned route stays drawn while you look around
        var kept: Dictionary = JSON.parse_string(adventure.preview(_planned.x, _planned.y))
        overlay.path.clear()
        for c in kept.path: overlay.path.append(Vector2i(c[0], c[1]))
        overlay.reachable = int(kept.reachable)
        if not _preview.is_empty(): sidebar.get_node("Route").text += "
(Click to plan this route instead.)"
    elif _planned != NO_CELL and cell == _planned and not _preview.is_empty():
        sidebar.get_node("Route").text = sidebar.get_node("Route").text.split("\n")[0] + "\nClick again (or Enter) to set off · Esc cancels."
    overlay.queue_redraw()

## A left click on the map: plan a route there, or set off if it is the planned one.
func click_cell(cell: Vector2i) -> bool:
    if cell == _planned: return travel_to(cell)
    return plan_route(cell)

## Draws a route to `cell` and keeps it until the hero gets there or it is cancelled.
func plan_route(cell: Vector2i) -> bool:
    if hero.moving or is_instance_valid(battle) or turn_busy or is_instance_valid(popup): return false
    _inspect(cell)
    if _preview.is_empty(): return false
    _planned = cell
    _inspect(cell)
    var goal := str(data.objects.get(cell, {}).get("name", "there"))
    notice.text = "Route planned to %s. Click it again (or Enter) to set off; Esc or right-click cancels." % goal
    return true

func cancel_route() -> void:
    if _planned == NO_CELL: return
    _planned = NO_CELL
    notice.text = "Route cancelled."
    _inspect(_hover)

func travel_to(cell: Vector2i) -> bool:
    if hero.moving or is_instance_valid(battle) or turn_busy or is_instance_valid(popup):
        return false
    _inspect(cell)
    if _preview.is_empty():
        return false
    var reply: Dictionary = JSON.parse_string(adventure.travel(cell.x, cell.y))
    if not reply.get("ok", false):
        return false
    if reply.steps.is_empty():
        if reply.encounter != null:
            state = reply
            _say(reply)
            _engage()
            return true
        notice.text = "Not enough movement left today. End the day (E) to rest."
        return false
    _pending_steps = reply.steps
    _destination = cell
    _held_lines = reply.get("lines", [])
    var walked: Array[Vector2i] = []
    for step in reply.steps: walked.append(Vector2i(step.cell[0], step.cell[1]))
    if reply.encounter != null:
        _destination = walked[-1]  # stop beside the guards, then fight
    overlay.path = walked.duplicate()
    overlay.reachable = -1
    notice.text = "On the march…"
    sidebar.get_node("Route").text = "Travelling · %d hexes" % walked.size()
    state = reply
    hero.follow(walked)
    _update_turn_hud()
    return true

func _entered_cell(cell: Vector2i) -> void:
    if not _pending_steps.is_empty():
        var step: Dictionary = _pending_steps.pop_front()
        fog.reveal(step.revealed)
        if not str(step.capture).is_empty():
            notice.text = "%s now flies the Compact's banner." % step.capture
            var taken: Dictionary = data.objects.get(cell, {})
            if str(taken.get("type", "")) not in ["town", "dwelling"]:   # towns open their own screen
                show_popup({"title": str(step.capture), "flavour": "Your banner goes up over %s. It will pay into your treasury every dawn while you hold it." % step.capture,
                    "picture": map_view.object_texture(taken)})
        if not str(step.get("found", "")).is_empty():
            notice.text = str(step.found)
            _found_on_journey = true
            show_popup(_site_popup(cell, str(step.found)))
    var landmark: Dictionary = data.objects.get(cell, {})
    sidebar.get_node("Location").text = str(landmark.get("name", "%s · [%d, %d]" % [str(data.tiles[cell].terrain).capitalize(), cell.x, cell.y]))
    overlay.origin = cell
    if not overlay.path.is_empty() and overlay.path[0] == cell:
        overlay.path.pop_front()
    overlay.queue_redraw()
    _update_expedition()

var _found_on_journey := false

func _journey_finished() -> void:
    var captured := notice.text.ends_with("banner.") or _found_on_journey
    _found_on_journey = false
    _apply_state(state)
    if hero.cell == _planned or state.get("encounter") != null:
        _planned = NO_CELL           # arrived, or the road was barred: the plan is done
    if _check_lost(): return
    if not _held_lines.is_empty():
        dialogue.say(_held_lines)
        _held_lines = []
    if state.get("encounter") != null:
        _engage()
        return
    if state.get("chest") != null:
        _offer_chest(state.chest)
        return
    var object: Dictionary = data.objects.get(hero.cell, {})
    if captured and object.get("type", "") == "dwelling" and hero.cell == _destination:
        open_town.call_deferred()  # a freshly taken dwelling shows its recruits at once
    if hero.cell != _destination:
        notice.text = "Out of movement — the expedition makes camp. End the day (E) to press on."
    elif not captured:
        match object.get("type", ""):
            "town":
                notice.text = "%s · Banners stir above the gates." % object.name
                if _owned_town(hero.cell): open_town.call_deferred()
            "dwelling":
                notice.text = "%s · Its creatures watch your banners." % object.name
                if _owned_town(hero.cell): open_town.call_deferred()
            "dungeon": notice.text = "%s · A cold wind rises from the sealed entrance." % object.name
            "artifact": notice.text = "%s · Only a hollow in the earth remains." % object.name
            "old_mine": notice.text = "%s · The shaft is empty and cold. Something below is breathing, slowly." % object.name
            "quest_giver": notice.text = "%s · An abandoned camp. Check the journal for what you found." % object.name
            "guard": notice.text = "%s · Their fires are cold. Nothing here yet." % object.name
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
    _town_button = Button.new()
    _town_button.name = "EnterTown"
    _town_button.text = "Enter town     T"
    _town_button.pressed.connect(open_town)
    sidebar.add_child(_town_button)
    sidebar.move_child(_town_button, sidebar.get_node("Spacer").get_index())
    _garrison_button = Button.new()
    _garrison_button.name = "Garrison"
    _garrison_button.text = "Garrison     R"
    _garrison_button.pressed.connect(open_garrison)
    sidebar.add_child(_garrison_button)
    sidebar.move_child(_garrison_button, sidebar.get_node("Spacer").get_index())
    _restart_button = Button.new()
    _restart_button.text = "New expedition"
    _restart_button.tooltip_text = "Reset the army, collected loot, and cleared dungeons; return to Varenhold."
    _restart_button.pressed.connect(new_expedition)
    sidebar.add_child(_restart_button)
    sidebar.move_child(_restart_button, sidebar.get_node("Spacer").get_index())

func _update_expedition() -> void:
    if not is_instance_valid(_army_label): return
    var summary: Array[String] = []
    for stack in army: summary.append("%d %s" % [stack.count, str(stack.id).replace("_", " ")])
    _army_label.text = ", ".join(summary) if not army.is_empty() else "Your army has fallen. Start a new expedition to fight again."
    var companions: Array[String] = []
    for sc in state.get("specials", []):
        var note := " (governing)" if sc.stationed != null else (" (wounded until day %d)" % int(sc.wounded_until) if sc.get("wounded", false) else "")
        companions.append("%s L%d%s" % [sc.name, sc.level, note])
    if not companions.is_empty():
        _army_label.text += "\nCompanions: " + ", ".join(companions)
    if not inventory.is_empty():
        var loot: Array[String] = []
        for item in inventory: loot.append(item.name)
        _army_label.text += "\nLoot: " + ", ".join(loot)
    var at_dungeon: bool = data.objects.get(hero.cell, {}).get("type", "") == "dungeon"
    _enter_button.visible = at_dungeon
    _enter_button.disabled = hero.moving or army.is_empty() or cleared_dungeons.has(hero.cell)
    _enter_button.text = "Dungeon cleared" if cleared_dungeons.has(hero.cell) else "Enter dungeon"
    _restart_button.visible = army.is_empty()
    if is_instance_valid(_town_button):
        _town_button.visible = _owned_town(hero.cell)
        _town_button.disabled = hero.moving
        _garrison_button.visible = _owned_site(hero.cell)
        _garrison_button.disabled = hero.moving

func enter_dungeon() -> bool:
    if hero.moving or is_instance_valid(battle) or army.is_empty() or cleared_dungeons.has(hero.cell): return false
    var landmark: Dictionary = data.objects.get(hero.cell, {})
    if landmark.get("type", "") != "dungeon": return false
    var guards := encounter
    var entry := _encounter_entry(landmark)
    if not entry.is_empty():
        guards = {"guards": entry.get("guards", []), "reward": entry.get("item", "")}
    return start_battle(guards, str(landmark.name) + "  /  Dungeon guards", _dungeon_finished.bind(hero.cell))

## Pushes a combat screen. on_result receives the combat result Dictionary.
func start_battle(guards: Dictionary, title: String, on_result: Callable) -> bool:
    var view := CombatView.new()
    screens.push(view, func(result: Dictionary):
        battle = null
        army = result.survivors.duplicate(true)
        _companions_after(result)
        on_result.call(result)
        _update_expedition())
    # Companions ride in beside the troops (wounded or unpaid ones stay behind).
    var fighters: Array = army.duplicate(true)
    var bonus: Dictionary = state.get("army_bonus", {})   # commander's items, Drill Yard
    for stack in fighters:
        stack["attack_bonus"] = int(bonus.get("attack", 0))
        stack["defense_bonus"] = int(bonus.get("defense", 0))
        stack["speed_bonus"] = int(bonus.get("speed", 0))
        stack["readied_shot"] = bool(bonus.get("readied_shot", false))   # Marksmen's Tower
    guards["fieldworks"] = state.get("fieldwork_stock", {}).duplicate()
    guards["tactics"] = int(state.get("tactics_rank", 0))   # opening orders the commander may give
    if not army.is_empty(): fighters.append_array(state.get("battle_companions", []))
    if not view.begin(fighters, guards, title):
        view.finished.emit({"result": "error", "survivors": army.duplicate(true), "rewards": []})
        notice.text = "Could not start combat. Check the native build and data files."
        return false
    battle = view
    _dragging = false
    return true

## Fallen companions: wounded for a few days, or gone if the battle was lost.
func _companions_after(result: Dictionary) -> void:
    var fallen: Array = result.get("fallen", [])
    if fallen.is_empty() or adventure == null: return
    var reply: Dictionary = JSON.parse_string(adventure.companions_fell(JSON.stringify(fallen), result.result == "defeat"))
    if reply.get("ok", false):
        _apply_state(reply)
        _say(reply)

func screen_covered() -> void:
    set_process(false)
    set_process_unhandled_input(false)
    $HUD.hide()

func screen_uncovered() -> void:
    $HUD.show()
    set_process(true)
    set_process_unhandled_input(true)

func _encounters_for(landmark: Dictionary) -> Array:
    return _encounter_entry(landmark).get("guards", [])

## The map's encounter entry for an object (mirrors the native lookup).
func _encounter_entry(landmark: Dictionary) -> Dictionary:
    var path: String = str(map_view.map_path).get_basename() + ".encounters.json"
    if not FileAccess.file_exists(path): return {}
    if not has_meta("encounters"):
        set_meta("encounters", JSON.parse_string(FileAccess.get_file_as_string(path)))
    var table: Dictionary = get_meta("encounters")
    var key := "old_mine" if landmark.get("type", "") == "old_mine" else "guard"
    if table.get("objects", {}).has(landmark.get("name", "")):
        return table.objects[landmark.name]
    if landmark.get("type", "") == "dungeon": return {}
    return table.get("defaults", {}).get(key, {})

var _encounter_type := ""

## Starts the pending fight, once whoever is speaking has had their say
## (a battle screen would hide the transmission).
func _engage() -> void:
    if dialogue != null and dialogue.is_speaking():
        notice.text = "Click the message to hear them out; the fight follows."
        await dialogue.drained
        if state.get("encounter") == null or is_instance_valid(battle): return   # walked away meanwhile
    _begin_encounter(state.encounter)

func _begin_encounter(encounter: Dictionary) -> void:
    _encounter_type = str(encounter.type)
    var guards := {"guards": encounter.guards, "reward": encounter.item}
    notice.text = ("The %s attacks!" if encounter.type == "ambush" else "%s bars the way!") % encounter.name
    if int(encounter.get("xp", 0)) > 0: notice.text += "  Victory is worth +%d XP." % int(encounter.xp)
    if army.is_empty() or not start_battle(guards, str(encounter.name), _encounter_finished):
        adventure.resolve_encounter(false)

func _encounter_finished(result: Dictionary) -> void:
    var victory: bool = result.result == "victory"
    result["encounter_type"] = _encounter_type
    if victory:
        inventory.append_array(result.rewards.duplicate(true))
    var reply: Dictionary = JSON.parse_string(adventure.resolve_encounter(victory))
    var place := str(data.objects.get(Vector2i(reply.hero[0], reply.hero[1]), {}).get("name", "The guards"))
    _apply_state(reply)
    _say(reply)
    hero.place_at(_hero_cell())
    _entered_cell(hero.cell)
    if _check_lost(): return
    if not victory:
        notice.text = "The expedition falls back. The guards still hold the way." if not army.is_empty() else ""
        _check_defeat()
        return
    if str(result.get("encounter_type", "")) in ["rival", "ambush"]:
        notice.text = "The Shariw war-band is scattered!"
        center_hero()
        return
    match str(reply.get("find", "none")):
        "passage":
            notice.text = "Beneath %s, a stairway plunges into the dark: the old passage!" % place
            if state.get("won", false): _show_end(true)
        "loot":
            notice.text = "%s is empty of secrets, but a forgotten cache of gold and crystal lies within." % place
        "collapse":
            notice.text = "%s ends in a wall of fallen rock. The passage is not here." % place
        _:
            notice.text = "Victory! %s is broken; the way is open." % place
    center_hero()

func _check_defeat() -> void:
    if army.is_empty():
        _show_end(false)

var _end_screen: Control

func _present_story() -> void:
    if state.is_empty() or is_instance_valid(_story_screen) or is_instance_valid(_end_screen): return
    if hero.moving or is_instance_valid(battle) or turn_busy or is_instance_valid(popup): return
    if screens.depth() > 0 or dialogue.is_speaking(): return
    var choice: Dictionary = state.get("story_choice", {})
    var outcome: Dictionary = state.get("story_outcome", {})
    if choice.is_empty() and outcome.is_empty(): return
    var scene := ChapterScene.new()
    scene.theme = $HUD/Layout.theme
    _story_screen = scene
    scene.choice = choice
    scene.outcome = outcome
    screens.push(scene, func(result: Dictionary):
        _story_screen = null
        if result.has("choice"):
            var reply: Dictionary = JSON.parse_string(adventure.choose_story(str(result.choice)))
            if not reply.get("ok", false):
                notice.text = str(reply.get("error", "The choice could not be made."))
                return
            _apply_state(reply)
            hero.place_at(_hero_cell())
            _say(reply)
            # Keep the committed branch for the next chapter, even after restarting the demo.
            var saved: Dictionary = JSON.parse_string(adventure.save_game(
                ProjectSettings.globalize_path("user://chapter_one.json"),
                JSON.stringify({"inventory": inventory, "cleared_dungeons": cleared_dungeons.keys().map(func(c): return [c.x, c.y])})))
            if not saved.get("ok", false): push_warning("Could not save chapter outcome: " + str(saved.get("error", "")))
            _present_story()
        else:
            _show_end(true))

func _show_end(victory: bool, reason := "") -> void:
    if is_instance_valid(_end_screen): return  # already showing
    var screen := EndScreen.new()
    screen.theme = $HUD/Layout.theme
    _end_screen = screen
    screen.victory = victory
    screen.illustration = "res://content/textures/screens/%s.png" % ("victory" if victory else ("sealed" if not reason.is_empty() else ""))
    if not reason.is_empty():
        screen.heading = "The Passage Is Sealed"
        screen.body = reason + " The Shariw have sealed the old passage, your search for Aldren cannot continue along this road."
    elif victory and not state.get("story_outcome", {}).is_empty():
        screen.heading = str(state.story_outcome.heading)
        screen.body = str(state.story_outcome.body)
        screen.illustration = ""
    elif victory:
        screen.heading = "The Old Passage Is Found"
        screen.body = "Below the old mine, steps too even for any mason lead down into warm dark. The veins in the walls brighten as you pass, as if something has been expecting you. Far to the south, the air tastes of sand. Umm'Natur is waiting."
    else:
        screen.heading = "The Expedition Is Lost"
        screen.body = "The marches keep their secrets. Somewhere under the Greyfangs, the old passage waits, and something in it is still counting."
    screen.stats = "Day %d · Week %d · Gold %d" % [state.day_of_week, state.week, state.treasury.Gold]
    screens.push(screen, func(result: Dictionary):
        if result.get("action") == "quit": get_tree().quit()
        else: _restart())

func _dungeon_finished(result: Dictionary, cell: Vector2i) -> void:
    if result.result == "victory" and not cleared_dungeons.has(cell):
        cleared_dungeons[cell] = true
        inventory.append_array(result.rewards.duplicate(true))
        for item in result.rewards:
            var reply: Dictionary = JSON.parse_string(adventure.add_item(str(item.id)))
            _apply_state(reply)
            _say(reply)
    notice.text = {"victory": "Victory! Survivors and loot have returned to the expedition.", "defeat": "The army has fallen.", "retreat": "Your survivors return. The dungeon remains guarded."}.get(result.result, "Returned to the map.")
    _check_defeat()

func new_expedition() -> void:
    if is_instance_valid(battle) or not army.is_empty(): return
    _restart()

func _restart() -> void:
    _end_screen = null
    _story_screen = null
    if dialogue != null: dialogue.skip_all()
    for id in _rival_sprites: _rival_sprites[id].queue_free()
    _rival_sprites.clear()
    army = STARTING_ARMY.duplicate(true)  # pushed into the new session by _start_adventure
    inventory.clear()
    cleared_dungeons.clear()
    if not _start_adventure(): return
    _last_xp = -1
    _apply_state(state)
    _say(state)
    hero.place_at(_hero_cell())
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
