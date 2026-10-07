# The agents, and what each one is allowed to write

Adapted from Gleipnir's roster (`gleipnir/docs/agents.md`). There are ten parties
working on this game: eight model subagents, one script, the main Claude session
and the user. This document says what each may do. `scripts/agent_roster.py` is
the same statement as a table that code reads. Its `check` command fails if the
table, the agent files in `.claude/agents/` and the invariants below drift apart.
Its `guard` command is a PreToolUse hook that refuses a subagent's writes
outside its paths.

The procedure for using them is the `umm-crew` skill (`.claude/skills/umm-crew/SKILL.md`).

## The rule

**An agent may propose. Only code may conclude. Only the user signs off on feel.**

- "It passes" is the exit status of `scripts/verify.sh`, recorded in
  `build/verify/last.json`. An agent's report of a pass is a claim to check
  against that file.
- "It looks, reads and plays right" belongs to the user, who beta-tests. A
  screenshot an agent looked at is evidence for the user, not a verdict.
- Design decisions (plot, factions, names, balance, scope) belong to the user.
  An agent drafts options and does not choose between them.

## What defines a role

**What it may write, and nothing else.** Whether a role runs on Opus or Haiku is
an implementation fact. Whether it can change the rules, or declare a change
done, is a design fact, and that is what the roster records.

**Deny-by-default.** A role holds exactly the capabilities listed against it.
Each capability it conspicuously lacks comes with a stated reason.

## The capabilities

| capability | what it is |
|---|---|
| `read` | read code, data, docs and logs |
| `edit_engine` | write the C++ rules: `src/`, `tests/`, `godot/native/` |
| `edit_godot` | write the Godot presentation: scripts, scenes, shaders, smoke tests |
| `read_player_view` | read only what the game shows a player (`build/player_view/`) |
| `edit_plot_graph` | write the plot graph a blind player built |
| `edit_story` | write story and map generator sources and data JSON, and regenerate their outputs |
| `run_checks` | build and run tests, routes and captures |
| `conclude_pass` | decide that a change passes |
| `sign_off_feel` | decide that something looks, reads or plays right |
| `decide_design` | settle a design question |
| `commit` | create a git commit or push |

## The roster, in the order a change travels

| role | kind | may write | may not, and why |
|---|---|---|---|
| **umm-scout** | model (Sonnet), read-only | nothing | edit: finding and changing are separate, so the map can be checked before any code moves |
| **umm-engine** | model | `src/`, `tests/`, `godot/native/` sources, CMakeLists | edit Godot: rules must stay testable without a window. Conclude: it reports what verify.sh printed |
| **umm-godot** | model | `godot/scripts`, `scenes`, `tests`, `shaders`, `themes`, `content_source`, `project.godot` | edit engine: a rule hidden in GDScript escapes raids_tests and demo_bot. Sign off: its screenshot goes to the user |
| **umm-story** | model | story/map generators, `data/`, story docs | decide design: plot and balance belong to the user. Edit engine: a need the engine cannot express is reported, not faked in data |
| **umm-player** | model, veiled | its plot graph `build/player_view/*.graph.json` | read anything else: a player who has read the plot document cannot show where the game fails to tell it. Run checks: code grades its graph, so it does not grade its own understanding |
| **umm-verify** | model (Haiku), Bash + Read | nothing | conclude: it relays, and the verdict is the script's. Edit: a verifier that can fix what it checks has nothing between it and the reader |
| **umm-playtest** | model, Bash + Read | nothing | sign off: a missing flag is not approval. Edit: an observer who fixes what it sees stops reporting it |
| **umm-review** | model, read-only | nothing | edit: findings are proposals. Conclude: a clean review is not a passing build |
| **verify.sh** | code | `build/verify/` | it is the only party that can say "passes" |
| **orchestrator** | main session | everything a builder may write; commits when the user asks | conclude: it reads the verdict from last.json and does not issue one. Sign off: what the user has not seen is reported as unverified |
| **user** | human | anything | none: designer, QA, and the beta tester |

`check` fails unless these hold:

- **Who can say it passes?** `verify.sh`, and only it.
- **Who can sign off on feel?** The user, and only the user.
- No model role holds `conclude_pass`, `sign_off_feel`, `decide_design` or `commit`.
- A role has Edit/Write tools exactly when it has an `edit_*` capability, and it names its paths.
- No two builders share a path, so engine, godot and story builders can work in parallel.
- Every roster subagent has a `.claude/agents/<name>.md` whose `name`, `tools` and `model` match the table, and every such file has a roster entry.

## The player behind the veil

Story as code: the story is told through play, not laid on top of it. The
user's test is whether a first-time player understands it, and whether late
beats point back to the beginning. Only a player who does not already know the
plot can check that.

1. `python3 scripts/player_view.py transcript <mode> <seed>` plays a scripted
   route (`demo_bot --transcript`) and writes everything the game shows, in
   order, to `build/player_view/<mode>-<seed>.txt`: intro slides, dialogue,
   quests, lore, choices, visits, battles, companions, items and endings. Ids,
   coordinates and numbers are left out.
2. **umm-player** reads only that file. The guard refuses its reads anywhere
   else (source, docs, memory) and it gets no CLAUDE.md. It writes a plot
   graph: nodes, edges, the threads it carried with what it expected, the
   callbacks it felt, and where it got lost. Every claim cites a line and the
   exact words.
3. `python3 scripts/player_view.py check <graph>` is the code that concludes.
   It refuses quotes the transcript does not contain, and it measures:
   - **story through play**: the share of threads opened, moved and closed by a
     visit, battle, choice, item or companion rather than by dialogue;
   - **late to early**: callbacks from the last third of the run back to the first;
   - **load**: new names per day (limit 4) and threads open at once (limit 5),
     set deliberately low because too much at once loses the user;
   - names that never connect to anything, threads still open at the end, and
     where the player was lost.

The orchestrator never tells the player agent what the plot is. Its prompt
names the transcript and nothing else.

## Why each denial exists

**`conclude_pass`.** When a builder says "1143/1143 passing", the claim could be
stale, from a different stage, or just wrong. `verify.sh` writes the commit,
the dirty-file count, the stages run and every step's exit code to
`last.json`. The orchestrator reads that file and does not rely on the builder's report.

**`sign_off_feel`.** The user's QA rule is that visuals and feel cannot be
verified without a human at the window. Self-verified batches (memory:
feedback-autonomy) shift *when* the user looks. They do not change *who* decides.

**`decide_design`.** Story, factions and balance are the user's. A model that
picks a plot direction does it silently, while a model that writes out three
options makes the choice visible.

**`commit`.** The working tree routinely holds uncommitted work from several
sessions. The guard refuses `git commit/push/reset/restore/clean/stash/rebase/
merge/cherry-pick/checkout --` inside any roster subagent.

**Small models flag; they do not copy or judge.** Gleipnir measured exact copying
from long text at Haiku 15/35 against 34/35 for the main model. So Haiku is used
only for umm-verify, whose output the orchestrator checks against `last.json`
anyway.

## What the guard does not enforce

The hook checks `Edit`/`Write` paths and destructive git commands. A subagent
with Bash could still write a file with `sed -i` or a redirect. The agent prompts
forbid this, and the review and diff catch it. The hook will not. Read-only
roles have no Edit/Write tools at all. Hooks apply only after the workspace
trust dialog is accepted for this folder.

## Adding a role

1. Add it to `ROLES` in `scripts/agent_roster.py` with a mandate, capabilities,
   tools, paths and a reason for every capability it conspicuously lacks.
2. Write `.claude/agents/<name>.md` with matching `name`, `tools` and `model`.
3. Run `python3 scripts/agent_roster.py check`. If a model role is given a
   forbidden capability, the check fails, and that failure is the mechanism working.
