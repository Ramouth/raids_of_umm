extends Node2D

signal entered_cell(cell: Vector2i)
signal journey_finished

var cell := Vector2i.ZERO
var moving := false
var _journey: Tween
@onready var sprite: AnimatedSprite2D = $Sprite

func _ready() -> void:
    var frames := SpriteFrames.new()
    frames.remove_animation("default")
    for clip in ["idle", "walk"]:
        frames.add_animation(clip)
        frames.set_animation_speed(clip, 6.0 if clip == "idle" else 9.0)
        var sheet: Texture2D = load("res://content/textures/units/armoured_warrior_%s.png" % clip)
        for index in range(4):
            var frame := AtlasTexture.new()
            frame.atlas = sheet
            frame.region = Rect2(index * 64, 0, 64, 64)
            frames.add_frame(clip, frame)
    sprite.sprite_frames = frames
    sprite.play("idle")

func place_at(start: Vector2i) -> void:
    cell = start
    position = UmmMapData.cell_to_world(start)

func follow(path: Array[Vector2i]) -> void:
    if moving or path.is_empty():
        return
    moving = true
    sprite.play("walk")
    _journey = create_tween()
    for next in path:
        var destination := UmmMapData.cell_to_world(next)
        _journey.tween_property(self, "position", destination, 0.22)
        _journey.tween_callback(_arrive.bind(next))
    _journey.tween_callback(_finish)

## HoMM3: the walk waits while a site's pop-up is open.
func pause_journey() -> void:
    if _journey and _journey.is_valid(): _journey.pause()
    sprite.play("idle")

func resume_journey() -> void:
    if _journey and _journey.is_valid() and moving:
        sprite.play("walk")
        _journey.play()

func _arrive(next: Vector2i) -> void:
    cell = next
    entered_cell.emit(cell)

func _finish() -> void:
    moving = false
    sprite.play("idle")
    journey_finished.emit()
