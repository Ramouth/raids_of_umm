class_name ScreenStack
extends Node
## Full-screen views stacked over the adventure map (combat, town, ...).
## The SDL StateMachine model: push a screen, it emits `finished(result)`,
## the stack pops it and hands the result to the caller's callback.
##
## The screen underneath is told via optional `screen_covered()` /
## `screen_uncovered()` methods so it can pause input and hide its HUD.

signal changed

const BASE_LAYER := 10

var base: Node  # the adventure screen; never popped
var _stack: Array[Dictionary] = []  # {screen, layer, on_close}

func push(screen: Control, on_close: Callable = Callable()) -> void:
    assert(screen.has_signal("finished"), "Screens must emit finished(result)")
    var below := _below()
    if below != null and below.has_method("screen_covered"):
        below.screen_covered()
    var layer := CanvasLayer.new()
    layer.layer = BASE_LAYER + _stack.size()
    add_child(layer)
    var backdrop := ColorRect.new()
    backdrop.color = Color("191610")
    backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    layer.add_child(backdrop)
    layer.add_child(screen)
    _stack.append({"screen": screen, "layer": layer, "on_close": on_close})
    screen.finished.connect(_on_finished.bind(screen), CONNECT_ONE_SHOT)
    changed.emit()

func top() -> Control:
    return null if _stack.is_empty() else _stack[-1].screen

func depth() -> int:
    return _stack.size()

func is_open(screen: Node) -> bool:
    return _stack.any(func(entry): return entry.screen == screen)

func _below() -> Node:
    return base if _stack.is_empty() else _stack[-1].screen

func _on_finished(result: Dictionary, screen: Control) -> void:
    var index := _stack.find_custom(func(entry): return entry.screen == screen)
    if index < 0:
        return
    var entry: Dictionary = _stack[index]
    _stack.remove_at(index)
    entry.layer.queue_free()
    var below := _below()
    if below != null and below.has_method("screen_uncovered"):
        below.screen_uncovered()
    if entry.on_close.is_valid():
        entry.on_close.call(result)
    changed.emit()
