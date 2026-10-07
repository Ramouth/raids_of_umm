#!/usr/bin/env python3
"""Who may do what: the agent roster as an enforced table, not prose.

Modelled on Gleipnir's `agents.py`. A role is defined by what it may write, not
by what it is made of. Deny-by-default: a role holds exactly what is listed
against it, and every conspicuous denial states its reason.

The rule this table exists for: **a model may propose; only code may conclude.**
"It passes" is said by `scripts/verify.sh` (its exit status and
build/verify/last.json), never by an agent's report. "It looks and feels right"
is said by the user, never by an agent looking at a screenshot.

Used three ways:
  python3 scripts/agent_roster.py check   -- roster, agent files and invariants agree
  python3 scripts/agent_roster.py show    -- the table, one screen
  python3 scripts/agent_roster.py guard   -- PreToolUse hook (reads hook JSON on stdin)
"""
from __future__ import annotations

import fnmatch
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
AGENTS_DIR = ROOT / ".claude" / "agents"

# The consequential acts. Nothing outside this list is granted to anyone.
CAPABILITIES = {
    "read": "read code, data, docs and logs",
    "read_player_view": "read only what the game shows a player (build/player_view/)",
    "edit_plot_graph": "write the plot graph a blind player built (build/player_view/*.graph.json)",
    "edit_engine": "write the C++ rules: src/, tests/, godot/native/",
    "edit_godot": "write the Godot presentation: scripts, scenes, shaders, smoke tests",
    "edit_story": "write story and map sources, data JSON, and regenerate their outputs",
    "run_checks": "build and run tests, routes and captures",
    "conclude_pass": "decide that a change passes",
    "sign_off_feel": "decide that something looks, reads or plays right",
    "decide_design": "settle a design question (story, balance, factions, scope)",
    "commit": "create a git commit or push",
}

# No model-backed role may ever hold these. Each is a lesson, see docs/agents.md.
MODEL_FORBIDDEN = {"conclude_pass", "sign_off_feel", "decide_design", "commit"}

EDIT_TOOLS = {"Edit", "Write", "NotebookEdit"}


@dataclass(frozen=True)
class Role:
    name: str
    kind: str  # "model" | "code" | "human" | "orchestrator"
    mandate: str
    capabilities: frozenset[str]
    tools: tuple[str, ...] = ()          # frontmatter `tools` for a subagent; () = not a subagent
    writes: tuple[str, ...] = ()         # glob patterns this role may Edit/Write
    reads: tuple[str, ...] = ()          # if set, the only paths it may Read/Grep/Glob (a veil)
    forbidden: dict[str, str] = field(default_factory=dict)  # capability -> why not
    model: str = ""


ENGINE_PATHS = ("src/*", "tests/*", "godot/native/*.cpp", "godot/native/*.h",
                "godot/native/CMakeLists.txt", "godot/native/build_profile.json", "CMakeLists.txt")
GODOT_PATHS = ("godot/scripts/*", "godot/scenes/*", "godot/tests/*", "godot/shaders/*",
               "godot/themes/*", "godot/content_source/*", "godot/project.godot")
STORY_PATHS = ("scripts/north_story.py", "scripts/gen_north_map.py", "scripts/north_vale.json",
               "scripts/gen_old_passage_map.py", "data/*.json", "data/maps/*",
               "docs/demo_flow.md", "docs/world_design.md", "docs/map_story_handoff.md")

ROLES: dict[str, Role] = {r.name: r for r in [
    Role("umm-scout", "model",
         "Map where a task lives: files, functions, data, tests, design notes. Returns file:line facts.",
         frozenset({"read"}), tools=("Read", "Grep", "Glob", "Bash"), model="sonnet",
         forbidden={"edit_engine": "finding and changing are separate, so the map is checked before code moves"}),
    Role("umm-engine", "model",
         "Change game rules in C++ (AdventureSession, Scenario, CombatEngine, saves) with tests.",
         frozenset({"read", "edit_engine", "run_checks"}),
         tools=("Read", "Grep", "Glob", "Bash", "Edit", "Write"), writes=ENGINE_PATHS,
         forbidden={"edit_godot": "rules stay testable without a window; presentation is another role",
                    "conclude_pass": "it reports what verify.sh printed; the orchestrator reads last.json"}),
    Role("umm-godot", "model",
         "Change what the player sees and clicks in Godot, and the smoke tests that drive it.",
         frozenset({"read", "edit_godot", "run_checks"}),
         tools=("Read", "Grep", "Glob", "Bash", "Edit", "Write"), writes=GODOT_PATHS,
         forbidden={"edit_engine": "a rule hidden in GDScript is untested by raids_tests and demo_bot",
                    "sign_off_feel": "a screenshot it took is evidence for the user, not a verdict"}),
    Role("umm-story", "model",
         "Change story, quests, dialogue and maps through their generator sources, then regenerate.",
         frozenset({"read", "edit_story", "run_checks"}),
         tools=("Read", "Grep", "Glob", "Bash", "Edit", "Write"), writes=STORY_PATHS,
         forbidden={"decide_design": "plot, factions and balance are the user's; it drafts options",
                    "edit_engine": "a story need that the engine cannot express is reported, not hacked in"}),
    Role("umm-player", "model",
         "Play blind: read only the player-view transcript, build a plot graph, say where it got lost.",
         frozenset({"read_player_view", "edit_plot_graph"}), tools=("Read", "Write", "Glob"),
         writes=("build/player_view/*.graph.json",), reads=("build/player_view/*",),
         forbidden={"read": "the veil of ignorance: a player who has read the plot document cannot tell you where the game fails to tell it",
                    "run_checks": "code checks its graph; it would otherwise grade its own understanding"}),
    Role("umm-verify", "model",
         "Run scripts/verify.sh and relay its PASS/FAIL lines and the failing log tails verbatim.",
         frozenset({"read", "run_checks"}), tools=("Bash", "Read"), model="haiku",
         forbidden={"conclude_pass": "it relays; the verdict is the script's exit status in last.json",
                    "edit_engine": "a verifier that can fix what it checks has nothing between it and the reader"}),
    Role("umm-playtest", "model",
         "Run captures and demo_bot routes, look at the screenshots, and raise questions for the user.",
         frozenset({"read", "run_checks"}), tools=("Bash", "Read", "Glob"),
         forbidden={"sign_off_feel": "no flag is not approval; the user beta-tests",
                    "edit_godot": "an observer who fixes what it sees stops reporting it"}),
    Role("umm-review", "model",
         "Read the diff against the project's design rules and report findings with file:line.",
         frozenset({"read"}), tools=("Read", "Grep", "Glob", "Bash"),
         forbidden={"edit_engine": "findings are proposals; a builder applies the ones the orchestrator accepts",
                    "conclude_pass": "a clean review is not a passing build"}),
    Role("verify.sh", "code",
         "Build both games, run raids_tests, native ctest, Godot smokes and demo_bot routes; exit 0 only if all pass.",
         frozenset({"run_checks", "conclude_pass"})),
    Role("orchestrator", "orchestrator",
         "The main Claude session: frames the task, delegates, reads last.json, reports honestly, commits when asked.",
         frozenset({"read", "edit_engine", "edit_godot", "edit_story", "run_checks", "commit"}),
         forbidden={"conclude_pass": "it reads the verdict from last.json, it does not issue one",
                    "sign_off_feel": "what was not seen by the user is reported as unverified"}),
    Role("user", "human",
         "Designer and QA: decides design questions, beta-tests feel and visuals, approves commits.",
         frozenset({"read", "sign_off_feel", "decide_design", "commit"})),
]}


# ---------------------------------------------------------------- frontmatter

def frontmatter(path: Path) -> dict[str, str]:
    text = path.read_text()
    m = re.match(r"^---\n(.*?)\n---\n", text, re.S)
    if not m:
        return {}
    out = {}
    for line in m.group(1).splitlines():
        if ":" in line and not line.startswith(" "):
            k, v = line.split(":", 1)
            out[k.strip()] = v.strip()
    return out


def check() -> list[str]:
    errors: list[str] = []
    for r in ROLES.values():
        unknown = r.capabilities - CAPABILITIES.keys()
        if unknown:
            errors.append(f"{r.name}: unknown capabilities {sorted(unknown)}")
        if r.kind == "model" and r.capabilities & MODEL_FORBIDDEN:
            errors.append(f"{r.name}: model role holds {sorted(r.capabilities & MODEL_FORBIDDEN)}")
        edits = any(c.startswith("edit_") for c in r.capabilities)
        if r.tools and edits != bool(EDIT_TOOLS & set(r.tools)):
            errors.append(f"{r.name}: edit capability and Edit/Write tools disagree")
        if r.tools and edits and not r.writes:
            errors.append(f"{r.name}: may edit but names no paths")
        if r.reads and ("read" in r.capabilities or "Bash" in r.tools):
            errors.append(f"{r.name}: a veiled role may not hold read or Bash (Bash reads anything)")
        if "read_player_view" in r.capabilities and not r.reads:
            errors.append(f"{r.name}: read_player_view needs a reads veil")
        for cap in r.forbidden:
            if cap in r.capabilities:
                errors.append(f"{r.name}: '{cap}' is both granted and forbidden")
    # Who may say it passes? verify.sh, and only verify.sh.
    if [r.name for r in ROLES.values() if "conclude_pass" in r.capabilities] != ["verify.sh"]:
        errors.append("conclude_pass must belong to verify.sh alone")
    if [r.name for r in ROLES.values() if "sign_off_feel" in r.capabilities] != ["user"]:
        errors.append("sign_off_feel must belong to the user alone")
    # Builders' paths are disjoint, so two builders never write the same file.
    builders = [r for r in ROLES.values() if r.writes]
    for i, a in enumerate(builders):
        for b in builders[i + 1:]:
            clash = set(a.writes) & set(b.writes)
            if clash:
                errors.append(f"{a.name} and {b.name} both write {sorted(clash)}")
    # Agent files and the roster agree.
    subagents = {r.name: r for r in ROLES.values() if r.tools}
    files = {p.stem: p for p in AGENTS_DIR.glob("umm-*.md")}
    for name in subagents.keys() - files.keys():
        errors.append(f"{name}: in the roster but .claude/agents/{name}.md is missing")
    for name in files.keys() - subagents.keys():
        errors.append(f"{name}: .claude/agents/{name}.md has no roster entry")
    for name in subagents.keys() & files.keys():
        fm = frontmatter(files[name])
        r = subagents[name]
        if fm.get("name") != name:
            errors.append(f"{name}: frontmatter name is {fm.get('name')!r}")
        tools = {t.strip() for t in fm.get("tools", "").split(",") if t.strip()}
        if tools != set(r.tools):
            errors.append(f"{name}: frontmatter tools {sorted(tools)} != roster {sorted(r.tools)}")
        if r.model and fm.get("model") != r.model:
            errors.append(f"{name}: frontmatter model {fm.get('model')!r} != roster {r.model!r}")
    return errors


def show() -> None:
    for r in ROLES.values():
        print(f"{r.name:14} {r.kind:12} {', '.join(sorted(r.capabilities))}")
        print(f"{'':14} {r.mandate}")
        if r.writes:
            print(f"{'':14} writes: {' '.join(r.writes)}")
        for cap, why in r.forbidden.items():
            print(f"{'':14} not {cap}: {why}")
        print()


# ---------------------------------------------------------------- the guard

GIT_FORBIDDEN = re.compile(r"\bgit\s+(commit|push|reset|checkout\s+--|restore|clean|stash|rebase|merge|cherry-pick)\b")


def deny(reason: str) -> None:
    print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse",
                                             "permissionDecision": "deny",
                                             "permissionDecisionReason": reason}}))
    sys.exit(0)


def guard() -> None:
    """PreToolUse hook. Acts only inside a roster subagent; the main session passes through."""
    try:
        event = json.load(sys.stdin)
    except json.JSONDecodeError:
        return
    role = ROLES.get(event.get("agent_type") or "")
    if not event.get("agent_id") or role is None or role.kind != "model":
        return
    tool, args = event.get("tool_name", ""), event.get("tool_input") or {}
    if role.reads and tool in {"Read", "Grep", "Glob"}:
        raw = args.get("file_path") or args.get("path") or ""
        rel = None
        if raw:
            try:
                rel = Path(raw if Path(raw).is_absolute() else ROOT / raw).resolve().relative_to(ROOT).as_posix()
            except ValueError:
                pass
        if rel is None or not any(fnmatch.fnmatch(rel, p) or fnmatch.fnmatch(rel + "/x", p) for p in role.reads):
            deny(f"{role.name} sees only what a player sees: {', '.join(role.reads)}. "
                 f"{raw or '(no path)'} is behind the veil.")
        if tool == "Glob" and ".." in args.get("pattern", ""):
            deny(f"{role.name}: patterns may not climb out of the player's view.")
    if tool in EDIT_TOOLS:
        raw = args.get("file_path") or args.get("notebook_path") or ""
        try:
            rel = Path(raw).resolve().relative_to(ROOT).as_posix()
        except ValueError:
            rel = None
        if rel is not None and not any(fnmatch.fnmatch(rel, p) for p in role.writes):
            owner = next((r.name for r in ROLES.values() if r.writes and any(fnmatch.fnmatch(rel, p) for p in r.writes)), None)
            deny(f"{role.name} may not write {rel}. "
                 + (f"It belongs to {owner}: report the change you need instead." if owner else
                    "No agent owns it: report the change you need to the orchestrator."))
    elif tool == "Bash" and GIT_FORBIDDEN.search(args.get("command", "")):
        deny(f"{role.name} may not change git history or discard work; commits are the orchestrator's, with the user's approval.")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "guard":
        guard()
    elif cmd == "show":
        show()
    elif cmd == "check":
        errs = check()
        for e in errs:
            print("ROSTER:", e)
        print("roster ok" if not errs else f"{len(errs)} roster error(s)")
        sys.exit(1 if errs else 0)
    else:
        print(__doc__)
        sys.exit(2)
