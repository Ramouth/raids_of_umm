extends Control
## A committed story choice followed by a short, illustrated night sequence.
## The silhouettes are drawn locally, so the scene works without new raster assets.
signal finished(result: Dictionary)

var choice: Dictionary = {}
var outcome: Dictionary = {}
var frame := -1
var _scene := "tunnel"
var _words: Label
var _heading: Label
var _buttons: VBoxContainer
var _elapsed := 0.0
var _closed := false
var _fade: Tween

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    mouse_filter = Control.MOUSE_FILTER_STOP
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    margin.add_theme_constant_override("margin_left", 100)
    margin.add_theme_constant_override("margin_right", 100)
    margin.add_theme_constant_override("margin_top", 36)
    margin.add_theme_constant_override("margin_bottom", 34)
    add_child(margin)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 14)
    margin.add_child(column)
    _heading = _label("", 18, Color("c5ac79"))
    column.add_child(_heading)
    var space := Control.new()
    space.size_flags_vertical = Control.SIZE_EXPAND_FILL
    column.add_child(space)
    _words = _label("", 25, Color("eee4d2"))
    _words.custom_minimum_size.y = 92
    column.add_child(_words)
    _buttons = VBoxContainer.new()
    _buttons.add_theme_constant_override("separation", 8)
    column.add_child(_buttons)
    if not choice.is_empty():
        _heading.text = str(choice.title).to_upper()
        _words.text = str(choice.text)
        for option in choice.options:
            var button := Button.new()
            button.text = str(option.label) + "\n" + str(option.get("detail", ""))
            button.custom_minimum_size.y = 60
            button.pressed.connect(select.bind(str(option.id)))
            _buttons.add_child(button)
    else:
        _heading.text = "THE OLD PASSAGE  ·  THAT NIGHT"
        var button := Button.new()
        button.name = "Continue"
        button.text = "Continue"
        button.custom_minimum_size.y = 44
        button.pressed.connect(advance)
        _buttons.add_child(button)
        advance()
    (_buttons.get_child(0) as Button).grab_focus()

func _label(text: String, font_size: int, colour: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.add_theme_font_size_override("font_size", font_size)
    label.add_theme_color_override("font_color", colour)
    return label

func select(id: String) -> void:
    if _closed or choice.is_empty(): return
    if not choice.options.any(func(option): return str(option.id) == id): return
    _closed = true
    finished.emit({"choice": id})

func advance() -> void:
    if _closed or not choice.is_empty(): return
    frame += 1
    var frames: Array = outcome.get("frames", [])
    if frame >= frames.size():
        _closed = true
        finished.emit({})
        return
    _scene = str(frames[frame].scene)
    _words.text = str(frames[frame].text)
    _words.modulate.a = 0.0
    if _fade != null: _fade.kill()
    _fade = create_tween()
    _fade.tween_property(_words, "modulate:a", 1.0, 0.6)
    _elapsed = 0.0
    queue_redraw()

func _unhandled_key_input(event: InputEvent) -> void:
    if not choice.is_empty(): return
    if event is InputEventKey and event.pressed and not event.echo:
        if event.keycode == KEY_ESCAPE:
            if not _closed:
                _closed = true
                finished.emit({})
        elif event.keycode in [KEY_SPACE, KEY_ENTER, KEY_KP_ENTER]: advance()
        else: return
        get_viewport().set_input_as_handled()

func _process(delta: float) -> void:
    _elapsed += delta
    queue_redraw()

func _draw() -> void:
    draw_rect(Rect2(Vector2.ZERO, size), Color("070b13"))
    draw_set_transform(Vector2.ZERO, 0.0, size / Vector2(1280, 720))
    for i in 55:
        var star := Vector2(70 + (i * 173) % 1140, 60 + (i * 67) % 275)
        draw_circle(star, 0.8, Color("798597"))
    var ink := Color("111b29")
    var rim := Color("344355")
    if _scene in ["home", "horse", "blood"]:
        draw_circle(Vector2(980, 136), 28, Color("a6b0b7"))
        draw_rect(Rect2(100, 190, 1080, 240), ink)
        for x in range(110, 1160, 50): draw_rect(Rect2(x, 172, 26, 34), ink)
        draw_rect(Rect2(510, 195, 260, 235), Color("05080e"))
        draw_arc(Vector2(640, 245), 130, PI, TAU, 36, rim, 4)
        draw_line(Vector2(0, 432), Vector2(1280, 432), rim, 2)
        for x in [440, 840]:
            draw_rect(Rect2(x, 245, 20, 35), Color("c09048"))
            draw_circle(Vector2(x + 10, 260), 30, Color(0.8, 0.45, 0.15, 0.06))
        if _scene != "home":
            var arrive := minf(_elapsed / 2.0, 1.0)
            _horse(Vector2(540 + arrive * 90, 360), _scene == "blood")
    else:
        draw_colored_polygon(PackedVector2Array([Vector2(0, 430), Vector2(0, 170), Vector2(400, 65), Vector2(530, 150), Vector2(450, 430)]), ink)
        draw_colored_polygon(PackedVector2Array([Vector2(830, 430), Vector2(750, 150), Vector2(890, 60), Vector2(1280, 165), Vector2(1280, 430)]), ink)
        for i in 8:
            var y := 280.0 + i * 20.0
            draw_line(Vector2(570 - i * 13, y), Vector2(710 + i * 13, y), rim, 1)
        if _scene in ["alone", "together", "tunnel"]:
            var descent := minf(_elapsed / 4.0, 1.0) * 12.0
            _person(Vector2(623, 352 - descent), true)
            if _scene == "together":
                _person(Vector2(690, 374 - descent), false)
                draw_line(Vector2(633, 323 - descent), Vector2(680, 345 - descent), Color("aa9071"), 1)
    draw_rect(Rect2(0, 465, 1280, 255), Color("070b13"))
    draw_set_transform(Vector2.ZERO)

func _person(at: Vector2, lantern: bool) -> void:
    draw_circle(at + Vector2(0, -63), 10, Color("687080"))
    draw_colored_polygon(PackedVector2Array([at + Vector2(-9, -53), at + Vector2(9, -53), at + Vector2(20, 0), at + Vector2(-20, 0)]), Color("4b5364"))
    if lantern:
        for radius in [45, 30, 18]: draw_circle(at + Vector2(-25, -38), radius, Color(0.95, 0.65, 0.3, 0.035))
        draw_rect(Rect2(at + Vector2(-29, -45), Vector2(8, 13)), Color("e3b56d"))

func _horse(at: Vector2, blood: bool) -> void:
    var coat := Color("707887")
    draw_style_box(_oval(coat), Rect2(at + Vector2(-75, -45), Vector2(135, 64)))
    draw_colored_polygon(PackedVector2Array([at + Vector2(30, -23), at + Vector2(51, -91), at + Vector2(76, -98), at + Vector2(99, -72), at + Vector2(83, -56), at + Vector2(62, -60), at + Vector2(59, 0)]), coat)
    for x in [-53, -30, 25, 46]: draw_line(at + Vector2(x, 4), at + Vector2(x - 6, 65), coat, 9)
    draw_line(at + Vector2(59, -85), at + Vector2(58, -111), coat, 7)
    draw_line(at + Vector2(72, -90), at + Vector2(76, -113), coat, 7)
    draw_line(at + Vector2(-70, -27), at + Vector2(-91, 18), coat, 7)
    draw_rect(Rect2(at + Vector2(-27, -47), Vector2(48, 25)), Color("302e30"))
    draw_line(at + Vector2(85, -65), at + Vector2(96, 36), Color("999078"), 2)
    if blood:
        draw_rect(Rect2(at + Vector2(-24, -45), Vector2(43, 26)), Color("693b3a"))
        draw_circle(at + Vector2(20, -8), 50, Color(0.95, 0.65, 0.3, 0.09))

func _oval(colour: Color) -> StyleBoxFlat:
    var style := StyleBoxFlat.new()
    style.bg_color = colour
    style.set_corner_radius_all(28)
    return style
