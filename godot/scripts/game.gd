extends Node
## Root of the running game: plays the intro, then owns the screen stack and
## the adventure screen. Screens (combat, town, ...) are pushed onto `screens`
## over the adventure map. Start with --skip-intro (or SKIP_INTRO=1) to go
## straight to the map.

const Adventure = preload("res://scenes/desert.tscn")
const Intro = preload("res://scripts/intro.gd")

@onready var screens: ScreenStack = $Screens
var adventure: Node2D

func _ready() -> void:
    if "--skip-intro" in OS.get_cmdline_user_args() or OS.has_environment("SKIP_INTRO"):
        _start_adventure()
        return
    var layer := CanvasLayer.new()
    layer.layer = 10
    add_child(layer)
    var intro := Intro.new()
    layer.add_child(intro)
    intro.finished.connect(func():
        layer.queue_free()
        _start_adventure())

func _start_adventure() -> void:
    adventure = Adventure.instantiate()
    adventure.screens = screens
    screens.base = adventure
    add_child(adventure)
    move_child(adventure, 0)
