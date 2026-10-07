# Build plan: the odyssey, iteratively

The design lives in the Claude Doc "The Odyssey — Campaign Plan"
(https://claude.ai/code/artifact/163432cd-c6d8-4290-9762-2fee2d64ee09). This file is the
engineering side: what the code needs so the game can be built a chapter at a time, with
MVP graphics now and real art swapped in later without code changes.

## Where the code is today (scouted 2026-10-07)

- **One map per session.** `AdventureSession::start` (src/adventure/AdventureSession.cpp:75)
  resets the hero, army, items, companions, level, path and treasury. Nothing carries from map
  to map.
- **No campaign layer.** `ending` calls `completeChapter` and sets `won`; `next_chapter` in the
  triggers is never read; `user://chapter_one.json` is written and never read.
- **No story flags.** A choice leaves no record a later map can read. Only per-map trigger
  ids, lore and items exist.
- **One hero.** `m_hero`, one move pool, one army, one progress (AdventureSession.h:365-389),
  mirrored in the bridge and `desert.gd`.
- **Art is scattered.** About a dozen const dictionaries and naming conventions across
  map_view.gd, combat_board.gd, dialogue_panel.gd, party_screen.gd, hero.gd, desert.gd and
  others. Missing art sometimes falls back, sometimes shows nothing, and in seven places loads
  without a guard (hero.gd:17, hero_screen.gd:47, desert.gd:126/556/560, chapter_scene.gd:61,
  map_view.gd `_sprite`).
- **Hard-wired to `old_passage`**: map_view.gd:42, demo_bot.cpp:161, native tests.cpp:14 and
  most Godot smoke tests.

## Architecture to add

### A. Campaign layer (umm-engine)

- `data/campaign.json`: chapters, each a list of maps, each map naming its
  `map`, `encounters` and `triggers` files.
- `CampaignState`: what carries between maps: commander level, xp, path, skills; army;
  items and equipped; companions (with level, xp, morale); treasury; **flags**.
- `AdventureSession::start(..., const CampaignState&)` starts a map from carried state;
  `campaignState()` reads it back at the map's end.
- **Flags** in `Scenario`: action `"flag": {"name": value}`; `when.flag` / `when.unless_flag`;
  option `requires_flag`. Every resolved choice also records `choice.<id> = <option id>`
  automatically, so heavy decisions are remembered without extra authoring.
- `ending` gains `"next": "<map id>"` (or a chapter end).
- Campaign save beside the map save; the save version is bumped.
- Tests: state carries across two `start()` calls; a flag set in map A gates a trigger in map B;
  save/load keeps the campaign.

### B. Art registry (umm-godot)

- `godot/content_source/art.json` (staged with the other content): logical id to file, e.g.
  `unit.woad_runner`, `object.town.north`, `portrait.Ushari`, `terrain.forest`, `screen.victory`,
  `slide.07_howls`.
- One autoload, `Art.texture(id)`, that **never returns null**. A missing file returns a generated
  placeholder (a flat silhouette in the category colour with the id's initials), logged once.
- Every scattered dictionary and every unguarded load goes through `Art`. Swapping art
  becomes editing `art.json` or dropping in a file with the right name.
- Smoke test: point an id at a missing file. The game shows the placeholder and does not crash.
- MVP graphics = the existing art where it fits, placeholders everywhere else.

### C. Chapter 1 content (umm-story)

- Maps 1.0 The last hunt, 1.1 Varenhold, 1.2 The camps, 1.3 The east road, built from small
  generator specs (one script per chapter, like gen_north_map.py) to the Map design tab:
  resources, encounter density and shape tell the story.
- Triggers from a new `scripts/chapter1_story.py`, with dialogue only where a decision is made.
- `old_passage` stays in the repo as a test fixture until chapter 1 replaces it in the game.

### D. Campaign flow in Godot (umm-godot, after A)

- Menu → intro → map → ending → next map, reading `campaign.json` through the bridge.

### E. Verification

- `demo_bot` gains a campaign mode that plays chapter 1's maps in sequence.
- `scripts/verify.sh` gains a `campaign` stage.
- The blind player reads the chapter 1 transcript; target numbers come from the Rules section
  of the design doc.

## Order

1. **Iteration 1:** A (engine) and B (Godot) in parallel. Their paths do not overlap.
2. **Iteration 2:** C (story) on top of A's flags, and D (Godot flow).
3. **Iteration 3:** E: campaign route in demo_bot, blind player on chapter 1, user beta test.

Later iterations: several heroes (captains from the ranks; a large change across about 50
engine sites, the bridge and desert.gd), chapter 2 (the Under-road rule set), dungeons and the
relic race.
