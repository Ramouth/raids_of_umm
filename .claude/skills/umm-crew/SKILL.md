---
name: umm-crew
description: How the main session runs work on Raids of Umm'Natur with the umm-* subagents (scout, engine, godot, story, player, verify, playtest, review). Use for any multi-step game change, roadmap item or autonomous batch, so that work is split by role, verified by scripts/verify.sh rather than by an agent's word, and handed to the user with what is and is not verified.
---

# Working on the game with the crew

The roster is `docs/agents.md` (prose) and `scripts/agent_roster.py` (the enforced table).
One rule carries everything: **an agent may propose; only code may conclude, and only the
user signs off on feel.** "It passes" means `build/verify/last.json` says `"passed": true`
for the stages that cover the change. An agent's "all tests pass" is a claim to check, not
a fact.

## 1. Frame before delegating

Write the frame in your notes (or show it to the user when the task is a design call):

- **Goal**, as the player experiences it, in one or two sentences.
- **Beat**: which row of docs/demo_flow.md it touches, and what the player must understand to act.
- **Roles**: which of engine / godot / story the change spans. Rules → umm-engine; what is
  seen and clicked → umm-godot; dialogue, quests, maps, data → umm-story.
- **What would show it works**, decided now: the C++ test, the smoke test, the demo_bot
  route, the capture. If nothing could show it, say so: that part goes to the user.
- **Design questions** the task leaves open. These go to the user before building, not after.

If you do not yet know where the task lives, send **umm-scout** first, with the frame.

## 2. Dispatch

- Give each builder the frame, the scout's map, its exact slice, and the stages it must run.
- Builders' paths are disjoint, so engine, godot and story builders can run in parallel.
  But they share `build/` and `godot/native/build/`: **only one verify.sh at a time.** When
  builders run in parallel, tell them to skip verify and run it yourself once they are all back.
- Order across a boundary: engine first (the bridge exposes the rule), then godot and story.
- Do not use `isolation: worktree` for builders: a worktree branches from `main`, not from
  this working tree, and loses the uncommitted work in it.
- A hook refuses a builder that writes outside its paths. When one reports "I need X in
  another role's files", that is the system working: route X to that role.

## 3. Conclude with code

- Run `scripts/verify.sh <stages>` yourself, or via **umm-verify** to keep logs out of
  context. Either way, read `build/verify/last.json` before saying anything passed.
- Stages: `engine` (SDL build + raids_tests), `native` (bridge + regressions ctest), `godot`
  (headless smoke tests), `routes` (demo_bot conquest/bypass/slow/escort), `captures`
  (screenshots, need a display), `quick` = engine+native, `all` = everything but captures.
- A failure has one address: the step name and its log in `build/verify/`. Send the builder
  that owns the failing path back with that log tail, not a description of it.
- Before a commit or a hand-off: `scripts/verify.sh all` and `python3 scripts/agent_roster.py check`.

## 4. Look, then hand to the user

- **umm-review** on the diff for correctness and boundary breaks. Its findings are
  proposals: accept or reject each one, then send accepted ones to the owning builder.
- **umm-playtest** with `captures` and routes for anything player-facing. Its observations
  are questions for the user, never approval; an empty report means it saw nothing, not
  that nothing is wrong.
- Hand-off to the user (the beta tester): what changed per role, the verify.sh lines,
  screenshots worth their eye by path, the playtester's questions, and plainly what was
  **not verified** (feel, pacing, visuals no capture shows). Commit only when the user asks.

## 5. The blind player, for anything that touches story

Story is told through play: the user judges a beat by whether a first-time player gets
it, and whether the late game points back to the beginning.

1. `python3 scripts/player_view.py transcript <mode> <seed>` (modes: escort, bypass,
   slow, conquest) writes `build/player_view/<mode>-<seed>.txt`.
2. Send **umm-player** with only the transcript path. **Never put plot, intent, names
   or what the change was meant to achieve in its prompt**: that lifts the veil.
3. `python3 scripts/player_view.py check build/player_view/<mode>-<seed>.graph.json`.
   Quote defects mean the graph is not checked: send the player back to fix them.
4. Give the user the report's numbers (story through play, late-to-early callbacks, load
   peaks) and the player's own words where it got lost. Compare with the previous run
   on the same route when a change was meant to fix something.

Keep it simple for the user: the headline is where the player got lost and whether the
ending reached back to the beginning, with nothing more in front of it.

## Choosing models

- Exact copying and judgement on long material are not small-model jobs (measured in
  Gleipnir: Haiku 15/35 exact quotes against 34/35). Haiku only relays (umm-verify) or flags.
- A small model's flag reaches you as a question to check yourself; no flag is not a pass.
