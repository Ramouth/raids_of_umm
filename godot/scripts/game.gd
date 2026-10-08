extends Node
## Root of the running game: plays the intro, then owns the screen stack and
## the adventure screen. Screens (combat, town, ...) are pushed onto `screens`
## over the adventure map. Start with --skip-intro (or SKIP_INTRO=1) to go
## straight to the menu.
##
## The game is a campaign (campaign.json, through the native bridge): a new
## expedition starts its first map; each map's ending leads, through a title
## card, to the next map with the company carried over. Continue reloads the
## campaign position and the save taken on the current map.

const Adventure = preload("res://scenes/desert.tscn")
const Intro = preload("res://scripts/intro.gd")
const Menu = preload("res://scripts/menu.gd")
const TitleCard = preload("res://scripts/title_card.gd")

@onready var screens: ScreenStack = $Screens
var adventure: Node2D
var _menu_layer: CanvasLayer

## The campaign the game plays; tests point this at a fixture.
var campaign_path := "res://content/data/campaign.json"
## Where the campaign save and its map save live (side by side).
var save_dir := "user://"
## Skips the opening intro (tests; same as --skip-intro).
var skip_intro := false
## The native session that holds the campaign; it outlives each map's scene.
var bridge: RefCounted
## The last campaign problem, shown on the main menu.
var campaign_error := ""
## campaign.json as written, for presentation-only extras (a map's "prophecy").
var _campaign_source: Dictionary = {}
var _card_layer: CanvasLayer

func campaign_save_path() -> String: return save_dir.path_join("campaign.save.json")
func map_save_path() -> String: return save_dir.path_join("campaign_map.save.json")

func _ready() -> void:
    if skip_intro or "--skip-intro" in OS.get_cmdline_user_args() or OS.has_environment("SKIP_INTRO"):
        _show_main_menu()
        return
    var layer := CanvasLayer.new()
    layer.layer = 10
    add_child(layer)
    var intro := Intro.new()
    layer.add_child(intro)
    intro.finished.connect(func():
        layer.queue_free()
        _show_main_menu())

## True when a campaign save exists that still has a map to play.
func can_continue() -> bool:
    var path := campaign_save_path()
    if not FileAccess.file_exists(path): return false
    var save: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
    return save is Dictionary and not save.get("finished", false)

func _show_main_menu() -> void:
    if is_instance_valid(_menu_layer): _menu_layer.queue_free()
    _menu_layer = CanvasLayer.new()
    _menu_layer.layer = 20
    add_child(_menu_layer)
    var menu: Control = Menu.new()
    menu.can_load = can_continue()
    menu.message = campaign_error
    _menu_layer.add_child(menu)
    menu.finished.connect(_main_menu_action)

func _main_menu_action(result: Dictionary) -> void:
    match str(result.get("action", "")):
        "new":
            _menu_layer.queue_free()
            _start_new_expedition()
        "load":
            _menu_layer.queue_free()
            continue_campaign()
        "quit": get_tree().quit()

func _start_new_expedition() -> void:
    var layer := CanvasLayer.new()
    layer.layer = 10
    add_child(layer)
    var cutscene: Control = Intro.new()
    cutscene.slides = [
        {"image": "slide.07_howls", "lines": [
            "Howls… howls in the night. Varenhold, your home and birthplace, is suddenly filled with wolves.",
            "Your father reigns here. Your brother stands to inherit."]},
        {"image": "slide.08_call", "lines": [
            "Some howls make your blood run cold. Others seem to call you out into the night.",
            "If no one can travel the roads, the wolves will ravage House Varen's domain."]}
    ]
    cutscene.show_title = false
    layer.add_child(cutscene)
    cutscene.finished.connect(func():
        layer.queue_free()
        new_campaign())

## Loads the campaign from its first map and starts that map.
func new_campaign() -> bool:
    _leave_map()
    bridge = _new_bridge()
    if bridge == null: return false
    var resolved := _resolve_campaign(campaign_path)
    if resolved.is_empty(): return _campaign_failed(campaign_error)
    var reply: Dictionary = JSON.parse_string(bridge.campaign_load(resolved, ProjectSettings.globalize_path("res://content/data")))
    if not reply.get("ok", false):
        return _campaign_failed("The campaign could not be loaded: %s" % str(reply.get("error", "unknown error")))
    _start_map(false, true)
    return true

## Continue: the campaign position, then the save taken on that map (if any).
func continue_campaign() -> bool:
    _leave_map()
    bridge = _new_bridge()
    if bridge == null: return false
    var reply: Dictionary = JSON.parse_string(bridge.campaign_load_save(ProjectSettings.globalize_path(campaign_save_path())))
    if not reply.get("ok", false):
        return _campaign_failed("The saved campaign could not be loaded: %s" % str(reply.get("error", "unknown error")))
    if reply.campaign.get("finished", false):
        return _campaign_failed("That campaign is already complete. Start a new expedition.")
    _read_campaign_source()
    _start_map(true)
    return true

## Called by the adventure once a map's ending has been read and the campaign
## has moved on (step = the campaign_advance reply): a title card, then the map.
func next_map(step: Dictionary) -> void:
    var campaign: Dictionary = step.get("campaign", {})
    var map: Dictionary = campaign.get("map", {}) if campaign.get("map") is Dictionary else {}
    _leave_map()
    _card_layer = CanvasLayer.new()
    _card_layer.layer = 15
    add_child(_card_layer)
    var card: Control = TitleCard.new()
    card.name = "TitleCard"
    var chapter: Variant = campaign.get("chapter")
    card.chapter_title = str(chapter.get("title", "")) if chapter is Dictionary else ""
    card.map_title = str(map.get("title", ""))
    card.new_chapter = bool(step.get("new_chapter", false))
    card.prophecy = _prophecy_for(str(map.get("id", "")))
    _card_layer.add_child(card)
    card.finished.connect(func(_result: Dictionary):
        _card_layer.queue_free()
        _start_map(false))

func title_card() -> Control:
    return _card_layer.get_node_or_null("TitleCard") if is_instance_valid(_card_layer) else null

## Back to the main menu (after the last map, or from the game menu).
func return_to_menu() -> void:
    _leave_map()
    _show_main_menu()

func open_game_menu() -> void:
    if screens.depth() > 0: return
    var menu: Control = Menu.new()
    menu.in_game = true
    menu.can_load = can_continue()
    screens.push(menu, _game_menu_action)

func _game_menu_action(result: Dictionary) -> void:
    match str(result.get("action", "")):
        "save": adventure.save_game()
        "load": continue_campaign()
        "main_menu": return_to_menu()
        "quit": get_tree().quit()

func _start_map(resume: bool, announce_quest := false) -> void:
    var current: Dictionary = JSON.parse_string(bridge.campaign_current())
    var map: Dictionary = current.get("campaign", {}).get("map", {}) if current.get("campaign", {}).get("map") is Dictionary else {}
    if map.is_empty():
        _campaign_failed("The campaign has no map to start.")
        return
    adventure = Adventure.instantiate()
    adventure.screens = screens
    adventure.game_root = self
    adventure.campaign = bridge
    adventure.campaign_map = map
    adventure.save_path = map_save_path()
    adventure.campaign_save_path = campaign_save_path()
    if resume: adventure.resume_save = map_save_path()
    adventure.get_node("Map").map_path = str(map.map)
    screens.base = adventure
    add_child(adventure)
    move_child(adventure, 0)
    if announce_quest and adventure.adventure != null:
        var quests: Array = adventure.state.get("quests", [])
        if not quests.is_empty():
            adventure.notice.text = "Quest log updated: %s. Press Q to view." % str(quests[0].get("title", "a new task"))

func _leave_map() -> void:
    screens.clear()
    if is_instance_valid(adventure):
        adventure.queue_free()
    adventure = null
    screens.base = null
    if is_instance_valid(_card_layer): _card_layer.queue_free()

func _new_bridge() -> RefCounted:
    if not ClassDB.class_exists("UmmAdventure"):
        _campaign_failed("Native adventure rules are missing. Rebuild with ./scripts/run_godot.sh.")
        return null
    campaign_error = ""
    return ClassDB.instantiate("UmmAdventure")

func _campaign_failed(message: String) -> bool:
    campaign_error = message
    push_warning(message)
    _show_main_menu()
    return false

## Writes a copy of the campaign whose map files are absolute paths to the
## staged content, and returns its OS path ("" with campaign_error set).
## Repo campaigns name maps relative to data/ ("maps/x.json"); the staged copy
## lives in content/data while the maps are staged to content/maps.
func _resolve_campaign(path: String) -> String:
    if not FileAccess.file_exists(path):
        campaign_error = "The campaign file %s is missing. Run ./scripts/run_godot.sh to stage data/campaign.json." % path
        return ""
    var source: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
    if not source is Dictionary:
        campaign_error = "The campaign file %s is not valid JSON." % path
        return ""
    _campaign_source = source.duplicate(true)
    var base := path.get_base_dir()
    for chapter in source.get("chapters", []):
        for map in chapter.get("maps", []):
            for key in ["map", "encounters", "triggers"]:
                var file := str(map.get(key, ""))
                if file.is_empty() or file.is_absolute_path(): continue
                for candidate in [base.path_join(file), "res://content".path_join(file), "res://content/maps".path_join(file.get_file())]:
                    if FileAccess.file_exists(candidate):
                        file = ProjectSettings.globalize_path(candidate)
                        break
                map[key] = file
    var resolved := save_dir.path_join("campaign.resolved.json")
    var out := FileAccess.open(resolved, FileAccess.WRITE)
    if out == null:
        campaign_error = "Cannot write %s." % resolved
        return ""
    out.store_string(JSON.stringify(source, "  "))
    out.close()
    return ProjectSettings.globalize_path(resolved)

func _read_campaign_source() -> void:
    var source := save_dir.path_join("campaign.resolved.json")
    var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(source)) if FileAccess.file_exists(source) else null
    if parsed is Dictionary: _campaign_source = parsed

func _prophecy_for(map_id: String) -> String:
    for chapter in _campaign_source.get("chapters", []):
        for map in chapter.get("maps", []):
            if str(map.get("id", "")) == map_id: return str(map.get("prophecy", ""))
    return ""
