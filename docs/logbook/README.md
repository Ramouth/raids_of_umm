# The loop logbook

Modelled on Gleipnir's logbook. One entry per run of the loop: build, judge, change, run again.
We expect many changes in the first runs and fewer each run after. The log shows whether
that is true.

- `runs.jsonl` has one row per run, so runs can be compared over time.
- `YYYY-MM-DD-<slice>-runN.md` is the entry for that run, written from the template below.

## A run

1. Build or change the slice (chapter, map, rule).
2. Judge it in code: `scripts/verify.sh all`, then the campaign route with its CAP lines, then
   `scripts/player_view.py check` on the blind player's graph.
3. Judge it as a player: umm-player reads the transcript blind, and umm-playtest looks at the
   captures.
4. Decide what to change. The user decides anything about design or feel.
5. Log the run, then start the next one.

## Row fields (`runs.jsonl`)

`date`, `slice`, `run`, `commit`, `verified` (last.json passed), `lost` (places the blind
player was lost), `play_opened` / `play_closed` (share of threads opened and closed by play),
`callbacks_late_to_early`, `peak_open_threads`, `max_new_names_per_day`, `cap` (per map: cap,
reached_day, map_days, xp_lost, trivial yes/no), `changes` (count of design and code changes
decided this run, by kind: story, map, rule, art, bug), `notes`.

Convergence is `changes` falling run over run while `lost` falls and `verified` stays true.

## Entry template

```
# <slice>, run <n> (<date>)

Commit:            Verified:
Blind player:      lost N · opened by play x/y · closed by play x/y · callbacks late→early N · peak open N
Level cap:         per map: cap, reached day / map days, XP lost, trivial?
Where the player got lost (their words, with line numbers):
What the playtester and the user saw:
Changes decided (kind: what, why):
Compared with the last run:
```
