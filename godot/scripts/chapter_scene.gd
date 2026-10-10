extends Control
## A committed story choice followed by a short, illustrated night sequence.
## The silhouettes are drawn locally, so the scene works without new raster assets.
## Also plays mid-map story scenes (desert.gd): `outcome = {frames}`, a `heading`.
signal finished(result: Dictionary)

const SceneStills = preload("res://scripts/scene_stills.gd")
## Scene ids this file knows how to draw; any other id gets a neutral backdrop.
const NIGHT_SCENES := ["home", "horse", "blood", "road", "road_alone", "road_together"]
const PASSAGE_SCENES := ["passage", "alone", "together", "tunnel"]

var choice: Dictionary = {}
var outcome: Dictionary = {}
var frame := -1
var _scene := "road"
var _words: Label
var _heading: Label
var _buttons: VBoxContainer
var _elapsed := 0.0
var _closed := false
var _fade: Tween
var _conversation_index := -1
var _speaker: Label
var _portrait: TextureRect
## The commander (the player): their portrait, captioned, lit while they speak.
var _commander: Control
var _next_line: Button
## Frame mode: the line above the picture, and the last frame's button.
var heading := ""
var close_label := "Continue"
var _continue: Button

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    mouse_filter = Control.MOUSE_FILTER_STOP
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    margin.add_theme_constant_override("margin_left", 100)
    margin.add_theme_constant_override("margin_right", 100)
    margin.add_theme_constant_override("margin_top", 36)
    margin.add_theme_constant_override("margin_bottom", 34)
    margin.mouse_filter = Control.MOUSE_FILTER_IGNORE   # clicks reach the scene (next frame)
    add_child(margin)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 14)
    column.mouse_filter = Control.MOUSE_FILTER_IGNORE
    margin.add_child(column)
    _heading = _label("", 18, Color("c5ac79"))
    column.add_child(_heading)
    var space := Control.new()
    space.size_flags_vertical = Control.SIZE_EXPAND_FILL
    space.mouse_filter = Control.MOUSE_FILTER_IGNORE
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
        if choice.get("presentation", "") == "conversation":
            _scene = "conversation"
            var portraits := HBoxContainer.new()
            portraits.alignment = BoxContainer.ALIGNMENT_CENTER
            portraits.add_theme_constant_override("separation", 260)
            column.add_child(portraits)
            column.move_child(portraits, 1)
            # The one you are talking to: their portrait, or a plain silhouette
            # when they have none (never someone who is not there).
            _portrait = TextureRect.new()
            _portrait.name = "Portrait"
            _portrait.custom_minimum_size = Vector2(180, 180)
            _portrait.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
            _portrait.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
            _portrait.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
            _portrait.texture = portrait_for(_first_speaker())
            _portrait.size_flags_vertical = Control.SIZE_SHRINK_BEGIN   # level with the commander's face
            portraits.add_child(_portrait)
            _commander = VBoxContainer.new()
            _commander.name = "Commander"
            _commander.add_theme_constant_override("separation", 4)
            var face := TextureRect.new()
            face.name = "Face"
            face.custom_minimum_size = Vector2(180, 180)
            face.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
            face.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
            face.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
            face.texture = Art.texture("hero.portrait")
            _commander.add_child(face)
            var caption := _label("YOU  ·  HOUSE VAREN", 16, Color("a9ccb7"))
            _commander.add_child(caption)
            portraits.add_child(_commander)
            _speaker = _label("", 18, Color("a9ccb7"))
            column.add_child(_speaker)
            column.move_child(_speaker, _words.get_index())
            _next_line = Button.new()
            _next_line.text = "Continue"
            _next_line.custom_minimum_size.y = 44
            _next_line.pressed.connect(_advance_conversation)
            _buttons.add_child(_next_line)
        if choice.get("kind", "") == "party":
            _party_cards(column)
        for option in choice.options:
            if choice.get("kind", "") == "party": break   # each companion's card holds its own button
            var button := Button.new()
            button.text = str(option.label)
            if not str(option.get("detail", "")).is_empty(): button.text += "\n" + str(option.detail)
            button.custom_minimum_size.y = 60
            button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
            button.visible = _next_line == null
            # An option whose story condition does not hold yet: shown, not takeable.
            button.disabled = bool(option.get("locked", false))
            if button.disabled: button.tooltip_text = "Not open to you yet."
            button.pressed.connect(select.bind(str(option.id)))
            _buttons.add_child(button)
    else:
        _heading.text = heading if not heading.is_empty() else "THE OLD PASSAGE  ·  THAT NIGHT"
        # A frame may be a spoken line (desert.gd joins lines to their scene): who speaks.
        _speaker = _label("", 18, Color("a9ccb7"))
        column.add_child(_speaker)
        column.move_child(_speaker, _words.get_index())
        var button := Button.new()
        _continue = button
        # Keys turn frames in _unhandled_key_input; a focused button would turn
        # the same Space / Enter a second time on release.
        button.focus_mode = Control.FOCUS_NONE
        button.name = "Continue"
        button.text = "Continue"
        button.custom_minimum_size.y = 44
        button.pressed.connect(advance)
        _buttons.add_child(button)
        # An ending without frames closes at once: after the stack has connected to us.
        if outcome.get("frames", []).is_empty(): advance.call_deferred()
        else: advance()
    if _next_line != null: _advance_conversation()
    for button in _buttons.get_children() + find_children("Choose", "Button", true, false):
        if not button.disabled and button.focus_mode != Control.FOCUS_NONE:
            button.grab_focus()
            break

## The party choice ("Who stays behind?"): a card per companion who may stay,
## with their portrait, and the button that leaves them behind.
func _party_cards(column: VBoxContainer) -> void:
    _scene = "party"
    var row := HBoxContainer.new()
    row.name = "Companions"
    row.alignment = BoxContainer.ALIGNMENT_CENTER
    row.add_theme_constant_override("separation", 36)
    column.add_child(row)
    column.move_child(row, 1)
    var newcomer := str(choice.get("newcomer", ""))
    for option in choice.options:
        var id := str(option.id)
        var card := VBoxContainer.new()
        card.name = "Card_" + id
        card.add_theme_constant_override("separation", 6)
        card.custom_minimum_size.x = 300
        var face := TextureRect.new()
        face.name = "Portrait"
        face.custom_minimum_size = Vector2(150, 150)
        face.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
        face.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
        face.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
        var name := str(option.label).trim_suffix(" stays behind")
        face.texture = Art.texture("portrait.companion." + id) if Art.has_art("portrait.companion." + id) else portrait_for(name)
        card.add_child(face)
        var caption := _label(name.to_upper() + ("  ·  JUST JOINED" if id == newcomer else ""), 16, Color("f0c870"))
        caption.name = "Name"
        card.add_child(caption)
        var button := Button.new()
        button.name = "Choose"
        button.text = str(option.label)
        if not str(option.get("detail", "")).is_empty(): button.text += "\n" + str(option.detail)
        button.custom_minimum_size = Vector2(300, 88)   # equal cards, even when a name wraps the detail
        button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
        button.disabled = bool(option.get("locked", false))
        button.pressed.connect(select.bind(id))
        card.add_child(button)
        row.add_child(card)

func _advance_conversation() -> void:
    _conversation_index += 1
    var lines: Array = choice.get("lines", [])
    if _conversation_index < lines.size():
        var line: Array = lines[_conversation_index]
        _speaker.text = str(line[0]).to_upper()
        _words.text = str(line[1])
        var speaking := str(line[0]) != "Commander"
        if speaking: _portrait.texture = portrait_for(str(line[0]))
        _portrait.modulate = Color.WHITE if speaking else Color(0.45, 0.55, 0.5)
        _commander.modulate.a = 1.0 if line[0] == "Commander" else 0.45
        return
    _speaker.text = "YOUR RESPONSE"
    _words.text = str(choice.text)
    _next_line.hide()
    for button in _buttons.get_children():
        if button != _next_line: button.show()
    for button in _buttons.get_children():
        if button != _next_line and not button.disabled:
            button.grab_focus()
            break

## The first speaker in the conversation who is not the commander ("" if none).
func _first_speaker() -> String:
    for line in choice.get("lines", []):
        if str(line[0]) != "Commander": return str(line[0])
    return ""

## portrait.<speaker> ("Northern Warrior" -> portrait.northern_warrior) when the
## art registry has one; otherwise a neutral silhouette.
static func portrait_for(speaker: String) -> Texture2D:
    var id := "portrait." + speaker.to_lower().replace(" ", "_")
    if not speaker.is_empty() and Art.has_art(id): return Art.texture(id)
    return silhouette()

static var _silhouette: Texture2D
## A hooded head and shoulders in slate, pixel-drawn: someone, not anyone.
static func silhouette() -> Texture2D:
    if _silhouette != null: return _silhouette
    var image := Image.create_empty(48, 48, false, Image.FORMAT_RGBA8)
    var ink := Color("3a4656")
    var rim := Color("5a6a7c")
    for y in 48:
        for x in 48:
            var head := Vector2(x - 23.5, (y - 17.5) * 0.9).length() <= 9.0
            var half := 6.0 + (y - 27) * 0.9
            var body := y >= 27 and y < 46 and absf(x - 23.5) <= half
            if head or body: image.set_pixel(x, y, ink)
    for y in 48:   # a 1px rim on the lit (left) edge
        for x in range(1, 48):
            if image.get_pixel(x, y).a > 0 and image.get_pixel(x - 1, y).a == 0: image.set_pixel(x, y, rim)
    _silhouette = ImageTexture.create_from_image(image)
    return _silhouette

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
    if _next_line != null and _next_line.visible: return
    if not choice.options.any(func(option): return str(option.id) == id and not option.get("locked", false)): return
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
    _scene = str(frames[frame].get("scene", ""))
    var speaker := str(frames[frame].get("speaker", ""))
    _speaker.text = speaker.to_upper()
    _words.text = "“%s”" % frames[frame].get("text", "") if not speaker.is_empty() else str(frames[frame].get("text", ""))
    _continue.text = close_label if frame == frames.size() - 1 else "Continue"
    _words.modulate.a = 0.0
    if _fade != null: _fade.kill()
    _fade = create_tween()
    _fade.tween_property(_words, "modulate:a", 1.0, 0.6)
    _elapsed = 0.0
    queue_redraw()

## A click anywhere on the picture turns to the next frame, like the button.
func _gui_input(event: InputEvent) -> void:
    if not choice.is_empty(): return
    if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
        advance()
        accept_event()

func _unhandled_key_input(event: InputEvent) -> void:
    if not choice.is_empty(): return
    if event is InputEventKey and event.pressed and not event.echo:
        if event.keycode == KEY_ESCAPE:
            if not _closed:
                _closed = true
                finished.emit({})
        elif event.keycode in [KEY_SPACE, KEY_ENTER, KEY_KP_ENTER]:
            advance()
        else: return
        get_viewport().set_input_as_handled()

func _process(delta: float) -> void:
    _elapsed += delta
    queue_redraw()

func _draw() -> void:
    draw_rect(Rect2(Vector2.ZERO, size), Color("070b13"))
    draw_set_transform(Vector2.ZERO, 0.0, size / Vector2(1280, 720))
    var still := SceneStills.texture(_scene)
    if still != null:
        draw_texture_rect(still, Rect2(0, 0, 1280, 464), false)
        draw_rect(Rect2(0, 465, 1280, 255), Color("070b13"))
        draw_set_transform(Vector2.ZERO)
        return
    if not (_scene in NIGHT_SCENES or _scene in PASSAGE_SCENES or _scene == "conversation"):
        # An unknown scene: a plain dark frame, so the words still carry the moment.
        draw_rect(Rect2(100, 60, 1080, 380), Color("0d121b"))
        draw_rect(Rect2(100, 60, 1080, 380), Color("2a3442"), false, 2.0)
        draw_rect(Rect2(0, 465, 1280, 255), Color("070b13"))
        draw_set_transform(Vector2.ZERO)
        return
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
    elif _scene in ["road", "road_alone", "road_together"]:
        draw_circle(Vector2(990, 110), 25, Color("a6b0b7"))
        draw_colored_polygon(PackedVector2Array([Vector2(0, 340), Vector2(160, 180), Vector2(310, 280), Vector2(450, 150), Vector2(660, 320), Vector2(950, 200), Vector2(1280, 320), Vector2(1280, 465), Vector2(0, 465)]), ink)
        draw_colored_polygon(PackedVector2Array([Vector2(440, 465), Vector2(650, 270), Vector2(690, 270), Vector2(790, 465)]), Color("34413f"))
        for i in 5:
            var at := Vector2(510 + i * 33, 430 - i * 30)
            draw_rect(Rect2(at, Vector2(10, 22)), rim)
        if _scene != "road": _person(Vector2(610, 410), true)
        if _scene == "road_together": _person(Vector2(700, 422), false)
    elif _scene == "conversation":
        draw_rect(Rect2(100, 65, 1080, 210), Color("101f22"))
        for y in range(66, 275, 4):
            draw_line(Vector2(100, y), Vector2(1180, y), Color(0.5, 0.8, 0.65, 0.035))
        draw_line(Vector2(100, 277), Vector2(1180, 277), Color("618578"), 2)
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
