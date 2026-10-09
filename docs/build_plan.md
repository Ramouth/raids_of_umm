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
- **A level cap per map**: `"max_level"` on each map entry. The commander cannot level past it on
  that map, and XP past the cap is lost but counted. The session records the day the cap was
  first reached and the XP lost to it.
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

- Maps 1.0 The Last Hunt, 1.1 Varenhold, 1.2 The Camps, 1.3 The East Road, built from small
  generator specs (one script per chapter, like gen_north_map.py) to the Map design tab:
  resources, encounter density and shape tell the story.
- **Organic outlines, no frame.** Today `gen_north_map.py` cuts every map to a hexagon (`RADIUS`,
  line 47) and rings it with mountains ("frame the map", line 216). New maps take the land's own
  outline (a bowl, a ribbon, corridors), with edges of varied natural barriers, and mountains only
  where the story puts them. umm-godot draws the land beyond the edge, out of reach and faded.
- Triggers from a new `scripts/chapter1_story.py`, with dialogue only where a decision is made.
- `old_passage` stays in the repo as a test fixture until chapter 1 replaces it in the game.

### D. Campaign flow in Godot (umm-godot, after A)

- Menu → intro → map → ending → next map, reading `campaign.json` through the bridge.

### E. Verification

- `demo_bot` gains a campaign mode that plays chapter 1's maps in sequence.
- **The level cap must not be trivial.** Per map, the route reports `CAP map=<id> cap=<n>
  reached_day=<d or never> map_days=<D> xp_lost=<x> xp_earned=<y>`. The eval flags the cap as
  trivial when a route reaches it before two-thirds of the map's days have passed, or loses
  more than a quarter of the map's XP to it. A thorough route should reach the cap near the
  end of the map, and a hasty one should not reach it.
- `scripts/verify.sh` gains a `campaign` stage.
- The blind player reads the chapter 1 transcript; target numbers come from the Rules section
  of the design doc.

## The loop (after iteration 1)

Once the art registry is in, each slice runs a Gleipnir-style loop: build, judge in code
(verifier, route caps, the graph check), judge as a player (the blind player and the
playtester), change, run again. Every run is logged in `docs/logbook/` with its numbers and the
count of changes it led to. We expect many changes in the first runs and fewer each run after,
and the log shows whether that happens.

## Order and gates (agreed 2026-10-07)

Depth before breadth: a chapter becomes good before the next one starts.

| Phase | What | Gate to move on |
|---|---|---|
| 0 | Iteration 1: campaign layer, flags, level cap (A) and the art registry (B) | `verify.sh` passes, apart from the known old-demo failures |
| 1 | Chapter 1 with a single hero (C, D, E), run as the loop | Changes per run falling; lost <= 3; at least half of threads opened by play; at least one late-to-early callback; cap not trivial; **the user has played it and it feels right** |
| 2 | Captains: several heroes and promotion at level-up; rerun the chapter 1 loop | Same gate |
| 3+ | One chapter at a time: 2 (the Under-road rules), 3 (dungeons and the relic race), 4 | Same gate, plus the user's beta test at each chapter's end |

- The blind player's numbers guide the work; the user's play decides. A loop that only
  optimises the numbers drifts toward pleasing them.
- `demo_bot` reads its objectives from the story data, not hand-written per-map routes, before
  chapter 1's loop starts.
- The user plays chapter 1 at loop runs 1 and 3, and whenever the numbers stop improving.

## Order

1. **Iteration 1:** A (engine) and B (Godot) in parallel. Their paths do not overlap.
2. **Iteration 2:** C (story) on top of A's flags, and D (Godot flow).
3. **Iteration 3:** E: campaign route in demo_bot, blind player on chapter 1, user beta test.

Later iterations: several heroes (captains from the ranks; a large change across about 50
engine sites, the bridge and desert.gd), chapter 2 (the Under-road rule set), dungeons and the
relic race.
