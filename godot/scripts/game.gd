extends Node
## Root of the running game: plays the intro, then owns the screen stack and
## the adventure screen. Screens (combat, town, ...) are pushed onto `screens`
## over the adventure map. Start with --skip-intro (or SKIP_INTRO=1) to go
## straight to the map.

const Adventure = preload("res://scenes/desert.tscn")
const Intro = preload("res://scripts/intro.gd")
const Menu = preload("res://scripts/menu.gd")

@onready var screens: ScreenStack = $Screens
var adventure: Node2D
var _menu_layer: CanvasLayer

func _ready() -> void:
    if "--skip-intro" in OS.get_cmdline_user_args() or OS.has_environment("SKIP_INTRO"):
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

func _show_main_menu() -> void:
    if is_instance_valid(_menu_layer): _menu_layer.queue_free()
    _menu_layer = CanvasLayer.new()
    _menu_layer.layer = 20
    add_child(_menu_layer)
    var menu: Control = Menu.new()
    menu.can_load = FileAccess.file_exists("user://quicksave.json")
    _menu_layer.add_child(menu)
    menu.finished.connect(_main_menu_action)

func _main_menu_action(result: Dictionary) -> void:
    match str(result.get("action", "")):
        "new":
            _menu_layer.queue_free()
            _start_new_expedition()
        "load":
            _menu_layer.queue_free()
            _start_adventure(true)
        "quit": get_tree().quit()

func _start_new_expedition() -> void:
    var layer := CanvasLayer.new()
    layer.layer = 10
    add_child(layer)
    var cutscene: Control = Intro.new()
    cutscene.slides = [
        {"image": "intro/07_howls.png", "lines": [
            "Howls… howls in the night. Varenhold, your home and birthplace, is suddenly filled with wolves.",
            "Your father reigns here. Your brother stands to inherit."]},
        {"image": "intro/08_call.png", "lines": [
            "Some howls make your blood run cold. Others seem to call you out into the night.",
            "If no one can travel the roads, the wolves will ravage House Varen's domain."]}
    ]
    cutscene.show_title = false
    layer.add_child(cutscene)
    cutscene.finished.connect(func():
        layer.queue_free()
        _start_adventure(false, true))

func open_game_menu() -> void:
    if screens.depth() > 0: return
    var menu: Control = Menu.new()
    menu.in_game = true
    menu.can_load = FileAccess.file_exists("user://quicksave.json")
    screens.push(menu, _game_menu_action)

func _game_menu_action(result: Dictionary) -> void:
    match str(result.get("action", "")):
        "save": adventure.save_game()
        "load": adventure.load_game()
        "main_menu":
            adventure.queue_free()
            adventure = null
            screens.base = null
            _show_main_menu()
        "quit": get_tree().quit()

func _start_adventure(load_saved := false, announce_wolf_quest := false) -> void:
    adventure = Adventure.instantiate()
    adventure.screens = screens
    adventure.game_root = self
    screens.base = adventure
    add_child(adventure)
    move_child(adventure, 0)
    if load_saved:
        adventure.call_deferred("load_game")
    elif announce_wolf_quest and adventure.adventure != null:
        adventure.notice.text = "Quest log updated: Kill all five wolf packs near the castle and drive them from House Varen's domain. Press Q to view."
