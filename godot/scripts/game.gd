extends Node
## Root of the running game: owns the screen stack and the adventure screen.
## Screens (combat, town, ...) are pushed onto `screens` over the adventure map.

const Adventure = preload("res://scenes/desert.tscn")

@onready var screens: ScreenStack = $Screens
var adventure: Node2D

func _ready() -> void:
    adventure = Adventure.instantiate()
    adventure.screens = screens
    screens.base = adventure
    add_child(adventure)
    move_child(adventure, 0)
