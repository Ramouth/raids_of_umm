extends Control
## The opening, Baldur's Gate II style: painted slides that fade into each
## other, with steady framing and narration appearing line by line beneath.
## Click, Space or Enter moves on; Esc skips the whole intro.

signal finished

const SLIDES := [
    {"image": "intro/01_word.jpg", "lines": [
        "The word is spreading.",
        "Along the salt roads, into the northern halls, from one fire to the next."]},
    {"image": "intro/02_walls.jpg", "lines": [
        "Umm'Natur, the walled nation, has fallen.",
        "For a thousand years its gates stood shut. Now no one keeps the walls."]},
    {"image": "intro/03_tomb.jpg", "lines": [
        "Its last emperor, Sethurakh the Red, Lord of Blood and Sand, died in his own tomb.",
        "He had himself walled in alive, trusting no hand but his own, whispering the old rites to keep death out.",
        "They did not."]},
    {"image": "intro/04_throne.jpg", "lines": [
        "He left no heir. No regent.",
        "The mightiest throne in the world stands empty, and the sand is already climbing its steps."]},
    {"image": "intro/05_hunt.jpg", "pace": 0.7, "lines": [
        "So the hunt begins.",
        "Who will take the throne? Who will reach the capital first? Who will carry off its relics?"]},
    {"image": "intro/06_passage.jpg", "lines": [
        "But your people remember what the others have forgotten:",
        "an old passage under the Greyfang mountains. A way in, ahead of every king standing in line."]},
    {"image": "", "lines": [
        "Your brother Aldren rode east to find it.",
        "He has not written since."]},
]
const TITLE := "RAIDS OF UMM'NATUR"
const FADE := 1.4        # seconds for a slide to fade in or out
var _transition: Tween
var _line_tween: Tween
var _title_active := false

var _slide := -1
var _line := -1
var _front: TextureRect   # the slide being shown
var _back: TextureRect    # the one fading out
var _lines: VBoxContainer
var _hint: Label
var _timer: Timer
var _done := false
var _textures := {}       # every slide's painting, loaded before the first fade (no mid-intro hitch)

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    mouse_filter = Control.MOUSE_FILTER_STOP
    var black := ColorRect.new()
    black.color = Color.BLACK
    black.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    black.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(black)
    _back = _picture()
    _front = _picture()
    var shade := TextureRect.new()    # a soft dusk over the lower third keeps the words readable
    var fade := Gradient.new()
    fade.set_color(0, Color(0, 0, 0, 0.0))
    fade.set_color(1, Color(0, 0, 0, 0.75))
    var ramp := GradientTexture2D.new()
    ramp.gradient = fade
    ramp.fill_from = Vector2(0, 0)
    ramp.fill_to = Vector2(0, 1)
    shade.texture = ramp
    shade.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    shade.stretch_mode = TextureRect.STRETCH_SCALE
    shade.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    shade.offset_top = -320
    shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(shade)
    _lines = VBoxContainer.new()
    _lines.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    _lines.offset_top = -210
    _lines.offset_bottom = -40
    _lines.offset_left = 160
    _lines.offset_right = -160
    _lines.alignment = BoxContainer.ALIGNMENT_END
    _lines.add_theme_constant_override("separation", 10)
    _lines.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(_lines)
    _hint = Label.new()
    _hint.text = "Click / Space: continue      Esc: skip"
    _hint.add_theme_font_size_override("font_size", 13)
    _hint.add_theme_color_override("font_color", Color(0.75, 0.66, 0.5, 0.6))
    _hint.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
    _hint.offset_left = -330
    _hint.offset_top = -30
    add_child(_hint)
    _timer = Timer.new()
    _timer.one_shot = true
    _timer.timeout.connect(advance)
    add_child(_timer)
    for slide in SLIDES:
        var path := "res://content/textures/" + str(slide.image)
        if not str(slide.image).is_empty() and ResourceLoader.exists(path): _textures[slide.image] = load(path)
    advance.call_deferred()

func _picture() -> TextureRect:
    var rect := TextureRect.new()
    rect.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
    rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
    # Smooth filtering preserves the painted artwork at different window sizes.
    rect.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
    rect.modulate.a = 0.0
    add_child(rect)
    return rect

## The next line of narration, or the next slide when this one is told.
func advance() -> void:
    if _done: return
    _timer.stop()
    if _title_active:
        skip()
        return
    if _slide >= 0 and _line + 1 < SLIDES[_slide].lines.size():
        _line += 1
        _show_line(_line)
        return
    _slide += 1
    if _slide >= SLIDES.size():
        _title()
        return
    _line = 0
    _show_slide(str(SLIDES[_slide].image))
    _show_line(0)

func _show_slide(image: String) -> void:
    if _transition != null: _transition.kill()
    _clear_lines()
    # Every line of the slide is laid out now, unseen, so nothing moves as they appear.
    for text in SLIDES[_slide].lines: _lines.add_child(_line_label(str(text)))
    # The old slide becomes the back layer and fades away beneath the new one.
    var old := _front
    _front = _back
    _back = old
    move_child(_back, 1)                 # always: black, back, front, shade, words
    move_child(_front, 2)
    _front.texture = _textures.get(image, null)
    _front.modulate.a = 0.0
    # Static image geometry avoids subpixel resampling shimmer during slow zooms.
    _front.scale = Vector2.ONE
    _back.scale = Vector2.ONE
    _transition = create_tween().set_parallel()
    _transition.tween_property(_back, "modulate:a", 0.0, FADE)
    _transition.tween_property(_front, "modulate:a", 1.0, FADE)

func _line_label(text: String) -> Label:
    var label := Label.new()
    label.text = text
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    var font := SystemFont.new()
    font.font_names = PackedStringArray(["Noto Serif", "DejaVu Serif", "Georgia"])
    label.add_theme_font_override("font", font)
    label.add_theme_font_size_override("font_size", 26)
    label.add_theme_color_override("font_color", Color("ecdcb8"))
    label.add_theme_color_override("font_outline_color", Color.BLACK)
    label.add_theme_constant_override("outline_size", 6)
    label.modulate.a = 0.0
    return label

func _clear_lines() -> void:
    if _line_tween != null: _line_tween.kill()
    for child in _lines.get_children():
        _lines.remove_child(child)
        child.queue_free()

## Reveals line `index` of the current slide; the ones before it dim, BG2 style.
func _show_line(index: int) -> void:
    var labels := _lines.get_children()
    if _line_tween != null: _line_tween.kill()
    _line_tween = create_tween().set_parallel()
    for i in index: _line_tween.tween_property(labels[i], "modulate:a", 0.45, 0.6)
    var first := index == 0
    _line_tween.tween_property(labels[index], "modulate:a", 1.0, 1.0).set_delay(FADE * 0.6 if first else 0.0)
    var text := str(SLIDES[_slide].lines[index])
    var pace: float = SLIDES[_slide].get("pace", 1.0)   # < 1: a slide that moves on sooner
    _timer.start((3.5 + text.length() / 18.0) * pace + (FADE if first else 0.0))

func _title() -> void:
    if _title_active: return
    _title_active = true
    _timer.stop()
    _clear_lines()
    if _transition != null: _transition.kill()
    _transition = create_tween().set_parallel()
    _transition.tween_property(_front, "modulate:a", 0.0, FADE)
    _transition.tween_property(_back, "modulate:a", 0.0, FADE)
    var title := Label.new()
    title.name = "Title"
    title.text = TITLE
    var font := SystemFont.new()
    font.font_names = PackedStringArray(["Noto Serif", "DejaVu Serif", "Georgia"])
    title.add_theme_font_override("font", font)
    title.add_theme_font_size_override("font_size", 54)
    title.add_theme_color_override("font_color", Color("e6c27a"))
    title.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    title.grow_horizontal = Control.GROW_DIRECTION_BOTH
    title.grow_vertical = Control.GROW_DIRECTION_BOTH
    title.modulate.a = 0.0
    add_child(title)
    var tween := create_tween()
    tween.tween_property(title, "modulate:a", 1.0, FADE).set_delay(FADE)
    tween.tween_interval(2.0)
    tween.tween_property(title, "modulate:a", 0.0, FADE)
    tween.tween_callback(skip)

## Ends the intro now (Esc, or after the title).
func skip() -> void:
    if _done: return
    _done = true
    _timer.stop()
    if _transition != null: _transition.kill()
    if _line_tween != null: _line_tween.kill()
    finished.emit()

func _gui_input(event: InputEvent) -> void:
    if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
        advance()
        accept_event()

func _unhandled_input(event: InputEvent) -> void:
    if not (event is InputEventKey and event.pressed and not event.echo): return
    if event.keycode == KEY_ESCAPE: skip()
    elif event.keycode in [KEY_SPACE, KEY_ENTER, KEY_KP_ENTER]: advance()
    else: return
    get_viewport().set_input_as_handled()
