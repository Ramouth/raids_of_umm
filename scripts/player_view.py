#!/usr/bin/env python3
"""The player's view of the story, and a check of the plot graph a blind player built from it.

Story as code: the story is told through play, not laid on top of it. This tool
lets that be measured. The player agent (umm-player) sees only the transcript
made here, the way a player would, and builds a plot graph. The agent proposes
the graph. This code concludes what can be concluded from it: whether every
quoted line is real, how much the player had to hold in mind at once, which
threads were opened or closed by play rather than by talk, and whether the late
game points back to the beginning.

  python3 scripts/player_view.py transcript [mode] [seed]   -> build/player_view/<mode>-<seed>.txt
  python3 scripts/player_view.py check <graph.json>         -> the report; exit 1 if the graph cites lines that do not say it

Graph format (written by the player agent; every number is a transcript line number):
{
  "nodes":     [{"id": "aldren", "kind": "person|place|group|thing|event", "name": "Aldren", "first": 12}],
  "edges":     [{"from": "aldren", "rel": "brother of", "to": "me", "line": 12, "words": "Your brother Aldren"}],
  "threads":   [{"id": "where-aldren", "question": "Where did Aldren go?", "opened": 13, "words": "He has not written since",
                 "progress": [140, 388], "closed": null, "close_words": null, "expect": "he found the passage and is trapped"}],
  "callbacks": [{"line": 902, "points_to": 4, "words": "...", "why": "the walled-in emperor returns as ..."}],
  "confusions":[{"line": 210, "what": "Who is Corvin, and why should I trust him?"}],
  "summary":   "The plot in at most five sentences, as I understood it."
}
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "player_view"
BOT = ROOT / "godot" / "native" / "build" / "demo_bot"

# What a player can hold at once. Deliberately low: the user struggles when it gets complicated.
MAX_NEW_NAMES_PER_DAY = 4
MAX_OPEN_THREADS = 5
# Lines that are play (the player did or met something) rather than told (read or listened).
PLAYED = ("VISIT", "BATTLE", "CHOSE", "ACCEPT", "ITEM", "JOINS", "LEAVES", "QUEST✓", "PATH", "LEVEL")


# ------------------------------------------------------------------ transcript

def _slide_lines(block: str) -> list[str]:
    return [s for s in re.findall(r'^\s*"((?:[^"\\]|\\.)*)"[,\]]', block, re.M) if not s.startswith("intro/")]


def intro_lines() -> list[str]:
    """Slides that exist only in Godot: the title intro, then the new-expedition cutscene."""
    intro = (ROOT / "godot" / "scripts" / "intro.gd").read_text()
    game = (ROOT / "godot" / "scripts" / "game.gd").read_text()
    start = game[game.index("func _start_new_expedition"):]
    blocks = [intro[intro.index("const SLIDES"):intro.index("const TITLE")],
              start[start.index("cutscene.slides"):start.index("cutscene.show_title")]]
    return ["INTRO " + l for b in blocks for l in _slide_lines(b)]


def transcript(mode: str = "escort", seed: str = "1") -> Path:
    if not BOT.exists():
        subprocess.run([str(ROOT / "scripts" / "build_godot_combat.sh")], check=True)
    run = subprocess.run([str(BOT), str(ROOT), mode, seed, "--transcript"], capture_output=True, text=True)
    lines = intro_lines()
    for l in (l for l in run.stdout.splitlines() if l.strip()):
        if l.startswith("DAY ") and lines and lines[-1].startswith("DAY "):
            first = lines[-1].split()[-1].rstrip(")") if "(" in lines[-1] else lines[-1].split()[1]
            l = f"{l} (days pass from {first})"
            lines[-1] = l  # the bot waiting out days is one beat to a player, not many
        else:
            lines.append(l)
    if len(lines) <= len(intro_lines()):
        sys.exit(f"demo_bot printed no transcript (exit {run.returncode}); is --transcript built?\n{run.stderr[-800:]}")
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{mode}-{seed}.txt"
    path.write_text("".join(f"{i:04d} {l}\n" for i, l in enumerate(lines, 1)))
    print(f"{path.relative_to(ROOT)}: {len(lines)} lines (bot exit {run.returncode})")
    return path


def load_transcript(path: Path) -> dict[int, str]:
    out = {}
    for l in path.read_text().splitlines():
        n, _, text = l.partition(" ")
        out[int(n)] = text
    return out


# ------------------------------------------------------------------ check

def norm(s: str) -> str:
    return re.sub(r"\s+", " ", s.replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')).strip().lower()


SCHEMA = {  # section -> (required fields, field -> type)
    "nodes": {"id": str, "kind": str, "name": str, "first": int},
    "edges": {"from": str, "rel": str, "to": str, "line": int, "words": str},
    "threads": {"id": str, "question": str, "opened": int, "words": str},
    "callbacks": {"line": int, "points_to": int, "words": str, "why": str},
    "confusions": {"line": int, "what": str},
}
OPTIONAL = {"threads": {"progress": list, "closed": (int, type(None)), "close_words": (str, type(None)), "expect": str}}


def schema_defects(g: dict) -> list[str]:
    out = []
    for section, fields in SCHEMA.items():
        items = g.get(section, [])
        if not isinstance(items, list):
            out.append(f"{section}: must be a list")
            continue
        for i, x in enumerate(items):
            where = f"{section}[{i}]" + (f" ({x.get('id') or x.get('question') or ''})" if isinstance(x, dict) else "")
            if not isinstance(x, dict):
                out.append(f"{where}: must be an object")
                continue
            for f, t in fields.items():
                if f not in x:
                    out.append(f"{where}: missing {f!r} ({t.__name__})")
                elif not isinstance(x[f], t) or isinstance(x[f], bool):
                    out.append(f"{where}: {f!r} must be {t.__name__}, got {type(x[f]).__name__}: {json.dumps(x[f])[:60]}")
            for f, t in OPTIONAL.get(section, {}).items():
                if f in x and not isinstance(x[f], t):
                    out.append(f"{where}: {f!r} has the wrong type ({type(x[f]).__name__})")
            extra = set(x) - set(fields) - set(OPTIONAL.get(section, {}))
            if extra:
                out.append(f"{where}: unknown fields {sorted(extra)}")
    if not isinstance(g.get("summary", ""), str):
        out.append("summary: must be a string")
    return out


def check(graph_path: Path) -> int:
    g = json.loads(graph_path.read_text())
    bad = schema_defects(g)
    if bad:
        print(f"GRAPH NOT CHECKED: {len(bad)} schema defect(s); fix these, then check again")
        for b in bad:
            print(f"  {b}")
        return 1
    tpath = graph_path.with_name(graph_path.name.replace(".graph.json", ".txt"))
    if not tpath.exists():
        sys.exit(f"No transcript beside the graph: expected {tpath}")
    T = load_transcript(tpath)
    last = max(T)
    defects: list[str] = []

    def says(line, words, what):
        if line is None:
            return
        if line not in T:
            defects.append(f"{what}: line {line} does not exist")
        elif words and norm(words) not in norm(T[line]):
            defects.append(f"{what}: line {line} does not contain {words!r}")

    day_of = {}
    day = 0
    for n in sorted(T):
        if T[n].startswith("DAY "):
            day = int(T[n].split()[1])
        day_of[n] = day
    kind = lambda n: T.get(n, "").split(" ", 1)[0]
    played = lambda n: n is not None and kind(n) in PLAYED

    nodes = {x["id"]: x for x in g.get("nodes", [])}
    for x in nodes.values():
        says(x.get("first"), None if x["id"] == "me" else x.get("name"), f"node {x['id']}")
    linked = set()
    for e in g.get("edges", []):
        says(e.get("line"), e.get("words"), f"edge {e.get('from')} -{e.get('rel')}-> {e.get('to')}")
        for end in (e.get("from"), e.get("to")):
            if end not in nodes and end != "me":
                defects.append(f"edge names unknown node {end!r}")
            linked.add(end)
    threads = g.get("threads", [])
    for t in threads:
        says(t.get("opened"), t.get("words"), f"thread {t['id']} opened")
        says(t.get("closed"), t.get("close_words"), f"thread {t['id']} closed")
    for c in g.get("callbacks", []):
        says(c.get("line"), c.get("words"), f"callback at {c.get('line')}")
        if c.get("points_to") not in T:
            defects.append(f"callback at {c.get('line')} points to missing line {c.get('points_to')}")
    for c in g.get("confusions", []):
        if c.get("line") not in T:
            defects.append(f"confusion at missing line {c.get('line')}")

    # Load: new names per day, and threads open at once.
    per_day: dict[int, list[str]] = {}
    for x in nodes.values():
        if x.get("first") in T and x.get("kind") != "event":
            per_day.setdefault(day_of[x["first"]], []).append(x["name"])
    heavy_days = {d: ns for d, ns in sorted(per_day.items()) if len(ns) > MAX_NEW_NAMES_PER_DAY}
    open_at = []
    for n in sorted(T):
        k = sum(1 for t in threads if t.get("opened", 10**9) <= n and (t.get("closed") is None or t["closed"] > n))
        open_at.append((n, k))
    peak_line, peak = max(open_at, key=lambda p: p[1]) if open_at else (0, 0)
    over = [n for n, k in open_at if k > MAX_OPEN_THREADS]

    # Story through play.
    opened_by_play = [t["id"] for t in threads if played(t.get("opened"))]
    closed = [t for t in threads if t.get("closed")]
    closed_by_play = [t["id"] for t in closed if played(t["closed"])]
    progress = [p for t in threads for p in t.get("progress", [])]
    progress_by_play = [p for p in progress if played(p)]

    # Late points back to early.
    third = last / 3
    callbacks = g.get("callbacks", [])
    long_arcs = [c for c in callbacks if c.get("line", 0) > 2 * third and c.get("points_to", last) <= third]
    orphans = [x["name"] for x in nodes.values() if x["id"] not in linked]

    pct = lambda a, b: f"{len(a)}/{len(b)}" + (f" ({100 * len(a) // len(b)}%)" if b else "")
    print(f"PLOT GRAPH  {graph_path.relative_to(ROOT) if graph_path.is_relative_to(ROOT) else graph_path}")
    print(f"  transcript {last} lines, {max(day_of.values()) if day_of else 0} days; "
          f"{len(nodes)} nodes, {len(g.get('edges', []))} edges, {len(threads)} threads")
    print("\nSTORY THROUGH PLAY")
    print(f"  threads opened by play   {pct(opened_by_play, threads)}")
    print(f"  threads closed by play   {pct(closed_by_play, closed)}")
    print(f"  progress steps by play   {pct(progress_by_play, progress)}")
    print("\nLATE POINTS BACK TO EARLY")
    print(f"  callbacks {len(callbacks)}; last third -> first third: {len(long_arcs)}")
    for c in long_arcs:
        print(f"    {c['line']} -> {c['points_to']}: {c.get('why', '')}")
    if not long_arcs:
        print("    none: nothing late in the run reaches back to the opening")
    print("\nLOAD (what the player must hold in mind)")
    print(f"  peak open threads {peak} at line {peak_line} (limit {MAX_OPEN_THREADS})"
          + (f"; over the limit on {len(over)} lines" if over else ""))
    for d, ns in heavy_days.items():
        print(f"  {'intro' if d == 0 else f'day {d}'}: {len(ns)} new names (limit {MAX_NEW_NAMES_PER_DAY}): {', '.join(ns)}")
    if orphans:
        print(f"  names that never connect to anything ({len(orphans)}): {', '.join(orphans)}")
    print("\nOPEN AT THE END")
    for t in threads:
        if not t.get("closed"):
            print(f"  {t['question']}" + (f"  [expects: {t['expect']}]" if t.get("expect") else ""))
    if g.get("confusions"):
        print("\nWHERE THE PLAYER WAS LOST")
        for c in g["confusions"]:
            print(f"  {c['line']} (day {day_of.get(c['line'], '?')}): {c['what']}")
    if g.get("summary"):
        print(f"\nTHE PLOT AS UNDERSTOOD\n  {g['summary']}")
    if defects:
        print(f"\nDEFECTS ({len(defects)}): the graph cites what the transcript does not say")
        for d in defects:
            print(f"  {d}")
    print("\n" + ("GRAPH CHECKED" if not defects else "GRAPH NOT CHECKED: fix the defects"))
    return 1 if defects else 0


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "transcript":
        transcript(*sys.argv[2:4])
    elif cmd == "check" and len(sys.argv) > 2:
        sys.exit(check(Path(sys.argv[2]).resolve()))
    else:
        print(__doc__)
        sys.exit(2)
