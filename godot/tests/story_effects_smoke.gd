extends SceneTree

var failures := 0

func _initialize() -> void:
    create_timer(25).timeout.connect(func(): push_error("Story effects timed out"); quit(1))
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
    var day: int = scene.state.day
    scene.end_day()
    check(scene.turn_busy, "Calendar transition locks repeated turns")
    scene.end_day()
    check(int(scene.state.day) == day + 1, "Repeated End Day cannot advance twice")
    check(scene.get_node("HUD").has_node("DayTransition"), "New day has a visual announcement")
    while scene.turn_busy: await process_frame
    await process_frame
    check(not scene.get_node("HUD").has_node("DayTransition"), "Announcement cleans itself up")
    scene.state.day_of_week = 1
    scene.state.week = 2
    scene._day_transition()
    var card: ColorRect = scene.get_node("HUD/DayTransition")
    check(card.get_child(0).text.begins_with("WEEK 2"), "New week has a distinct announcement")
    await create_timer(1.1).timeout
    scene.queue_free()
    await process_frame
    var source: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://content/maps/old_passage.triggers.json"))
    var arrival: Dictionary
    for trigger in source.triggers:
        if trigger.id == "ushari_arrives":
            for action in trigger["do"]:
                if action.has("choice"): arrival = action.choice
    var conversation: Control = load("res://scripts/chapter_scene.gd").new()
    conversation.choice = arrival
    root.add_child(conversation)
    await process_frame
    check(conversation._speaker.text == "USHARI", "Ushari opens the night exchange")
    check(not conversation._buttons.get_child(1).visible, "Responses wait for her question")
    await create_timer(0.1).timeout
    check(conversation._conversation_index == 0, "Conversation never advances automatically")
    for line in arrival.lines: conversation._advance_conversation()
    check(conversation._buttons.get_child(1).visible, "Question reveals responses")
    if "--capture" in OS.get_cmdline_user_args() and DisplayServer.get_name() != "headless":
        await process_frame
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("/tmp/umm_arrival.png")
    var results: Array = []
    conversation.finished.connect(func(result): results.append(result))
    conversation.select("dismiss_aldren")
    conversation.select("stand_by_aldren")
    check(results.size() == 1 and results[0].choice == "dismiss_aldren", "A response commits only once")
    conversation.queue_free()
    await process_frame
    print("Story effects: ", "PASS" if failures == 0 else "FAIL")
    quit(failures)
