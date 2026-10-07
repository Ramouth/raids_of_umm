extends SceneTree

var failures := 0
const SAVE := "/tmp/umm_rookwatch_smoke.json"

func _initialize() -> void:
    create_timer(45).timeout.connect(func(): push_error("Rookwatch test timed out"); quit(1))
    _run.call_deferred()

func check(ok: bool, message: String) -> void:
    if not ok:
        failures += 1
        push_error(message)

func _run() -> void:
    var scene: Node2D = load("res://scenes/desert.tscn").instantiate()
    root.add_child(scene)
    await process_frame
    scene.dialogue.skip_all()
    var watch := Vector2i.ZERO
    var castle := Vector2i.ZERO
    for cell in scene.data.objects:
        if scene.data.objects[cell].name == "Rookwatch": castle = cell
        if scene.data.objects[cell].name == "Greyfang Watch": watch = cell
    check(castle != Vector2i.ZERO, "Rookwatch is in the playable map")
    check(scene.map_view.object_texture(scene.data.objects[castle]) == "object.town.north", "Rookwatch uses northern castle art")
    var count := 0
    for stack in scene._encounters_for(scene.data.objects[castle]): count += int(stack.count)
    check(count == 53, "Castle inspection includes the actual lord and garrison")
    check(scene.map_view.anchors[castle].get_node("GuardCount").text == "53 guards", "Castle badge uses live defenders")
    var saved: Dictionary = JSON.parse_string(scene.adventure.save_game(SAVE, "{}"))
    check(saved.ok, "Fixture begins from the production save format")
    var fixture: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(SAVE))
    var story: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://content/maps/old_passage.triggers.json"))
    for trigger in story.triggers:
        if trigger.id == "rook_passage": fixture.scenario.choice = trigger["do"][0].choice
    # Preserve real armies, town state and story definitions; stage the one-shot choice.
    fixture.scenario.fired.append("rook_passage")
    var approach := watch + Vector2i(-1, 0)
    fixture.hero = [approach.x, approach.y]
    fixture.encounters = []
    fixture.explored = []
    for cell in scene.data.tiles: fixture.explored.append([cell.x, cell.y])
    FileAccess.open(SAVE, FileAccess.WRITE).store_string(JSON.stringify(fixture))
    var reply: Dictionary = JSON.parse_string(scene.adventure.load_game(SAVE))
    check(reply.ok, "Pending request loads successfully")
    scene._apply_state(reply)
    scene.hero.place_at(Vector2i(fixture.hero[0], fixture.hero[1]))
    scene._present_story()
    await process_frame
    var choice: Control = scene.screens.top()
    check(choice != null and choice.choice.id == "rook_passage", "Passage request appears as an interactive choice")
    if choice == null:
        quit(1)
        return
    choice.select("ask_rook_passage")
    await process_frame
    check(scene.state.encounter == null, "Requesting passage schedules a meeting without an instant ambush")
    scene.dialogue.skip_all()
    await process_frame
    scene.travel_to(watch)
    for frame in 500:
        await process_frame
        if is_instance_valid(scene.popup): scene.popup.choose(0)
        if not scene.hero.moving and scene.state.encounter != null: break
    check(scene.state.encounter != null and scene.state.encounter.type == "ambush", "Arriving at Greyfang Watch triggers the promised escort")
    if scene.state.encounter == null:
        quit(1)
        return
    scene.dialogue.skip_all()
    await process_frame
    await process_frame
    check(is_instance_valid(scene.battle), "Finishing the escort dialogue opens tactical combat")
    var escort: Dictionary = scene._rival_at(Vector2i(scene.state.encounter.cell[0], scene.state.encounter.cell[1]))
    check(escort.get("name", "") == "Rook's escort" and escort.get("roster", "") == "cruths", "Escort has its northern identity on the map")
    count = 0
    for stack in scene._encounters_for(scene.data.objects[castle]): count += int(stack.count)
    check(count == 53, "The separate escort does not weaken the castle defense")
    scene.queue_free()
    await process_frame
    DirAccess.remove_absolute(SAVE)
    print("Rookwatch: ", "PASS" if failures == 0 else "FAIL")
    quit(failures)
