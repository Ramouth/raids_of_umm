#!/usr/bin/env python3
"""chapter1_doc.py — the readable Chapter 1 story, for the story doc.

Reads the generated Chapter 1 data (data/campaign.json, data/maps/ch1_*.triggers.json
and *.encounters.json) and writes Markdown with the story doc's beat symbols:
💬 dialogue  ⚔️ battle  👁️ discovery  📜 quest  🔍 clue  💰 reward  🚩 capture
🐍 rival  🏆 victory  ☠️ defeat. Beats are numbered per map (1.2.5 = map 1.2, beat 5).

The story itself lives in the header of scripts/chapter1_story.py; this only
publishes what the game data says, so the doc never drifts from the game.

Usage:  python3 scripts/chapter1_doc.py [--out build/story/chapter1.md]
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def cond_text(c):
    """A condition tree in plain words."""
    if c is True:
        return "always"
    if c is False:
        return "never"
    if "all" in c:
        return " and ".join(cond_text(x) for x in c["all"])
    if "any" in c:
        return "(" + " or ".join(cond_text(x) for x in c["any"]) + ")"
    if "not" in c:
        return "not " + cond_text(c["not"])
    if "flag" in c:
        return f"{c['flag']} = {c['is']}" if "is" in c else c["flag"]
    if "item" in c:
        return f"carrying {c['item']}"
    if "companion" in c:
        return f"{c['companion']} in the party"
    if "trigger_fired" in c:
        return f"after {c['trigger_fired']}"
    if "quest" in c:
        return f"quest {c['quest']} {c.get('state', '')}".strip()
    if "cleared" in c:
        return f"{c['cleared']} cleared"
    return json.dumps(c)


def when_text(w):
    ev = w.get("event", "")
    if ev in ("visit", "engage", "encounter_won"):
        verb = {"visit": "Visit", "engage": "Engage", "encounter_won": "Win against"}[ev]
        return f"{verb} {w.get('name')}"
    if ev == "cleared":
        return "Cleared " + ", ".join(w.get("names", []))
    if ev in ("start", "night", "dawn"):
        return {"start": "Map start", "night": "End of day", "dawn": "Dawn"}[ev]
    if ev == "enter_q":
        return "Reach the road out"
    return ev


def lines_of(say):
    return [f"💬 **{s or 'Narrator'}:** “{t}”" for s, t in say]


def actions(acts, items, indent=""):
    out = []
    for a in acts:
        if "say" in a:
            out += [indent + l for l in lines_of(a["say"])]
        elif "lore" in a:
            out.append(f"{indent}🔍 *{a['lore'][0]}*: {a['lore'][1]}")
        elif "quest" in a:
            out.append(f"{indent}📜 New quest: {a['quest']}")
        elif "quest_done" in a:
            out.append(f"{indent}📜 Quest done: {a['quest_done']}")
        elif "quest_text" in a:
            out.append(f"{indent}📜 Quest update: {a['quest_text'][1]}")
        elif "item" in a:
            it = items.get(a["item"], {})
            out.append(f"{indent}💰 Item: **{it.get('name', a['item'])}**" + (f" — {it['text']}" if it.get("text") else ""))
        elif "take_item" in a:
            out.append(f"{indent}💰 Gives up: {items.get(a['take_item'], {}).get('name', a['take_item'])}")
        elif "join" in a:
            out.append(f"{indent}👁️ **{a['join'].title()} joins the party**")
        elif "leave" in a:
            out.append(f"{indent}👁️ **{a['leave'].title()} leaves the party**")
        elif "scene" in a:
            for fr in a["scene"].get("frames", []):
                out.append(f"{indent}👁️ *Scene:* {fr['text'].replace(chr(10), ' ')}")
        elif "xp" in a:
            out.append(f"{indent}💰 +{a['xp']} XP")
        elif "withdraw" in a:
            out.append(f"{indent}⚔️ {a['withdraw']} withdraws: no battle")
        elif "vanish" in a:
            out.append(f"{indent}👁️ {a['vanish']} is gone")
        elif "open" in a:
            out.append(f"{indent}👁️ The way opens")
        elif "if" in a:
            out.append(f"{indent}↳ If {cond_text(a['if'])}:")
            out += actions(a.get("then", []), items, indent + "  ")
            if a.get("else"):
                out.append(f"{indent}↳ Otherwise:")
                out += actions(a["else"], items, indent + "  ")
        elif "choice" in a or "conversation" in a:
            c = a.get("choice") or a.get("conversation")
            for s, t in c.get("lines", []):
                out.append(f"{indent}💬 **{s}:** “{t}”")
            out.append(f"{indent}💬 **Choice — {c.get('title', c.get('id'))}.** {c.get('text', '')}")
            for o in c.get("options", []):
                gate = f" *(only if {cond_text(o['if'])})*" if "if" in o else ""
                detail = f" — {o['detail']}" if o.get("detail") else ""
                out.append(f"{indent}  - **{o['label']}**{gate}{detail}")
                out += actions(o.get("then", []), items, indent + "    ")
        elif "ending" in a:
            e = a["ending"]
            out.append(f"{indent}🏆 **{e.get('heading')}** — {e.get('body')}")
            for fr in e.get("frames", []):
                out.append(f"{indent}  👁️ *{fr['text'].replace(chr(10), ' ')}*")
        elif "leave_offer" in a or "reveal" in a or "flag" in a:
            continue
    return out


def guard_text(name, enc, units):
    parts = [f"{g['count']} {units.get(g['id'], {}).get('name', g['id'])}" for g in enc.get("guards", [])]
    rw = enc.get("reward", {})
    reward = ", ".join(f"{v} {k}" for k, v in rw.items() if k != "items")
    if rw.get("items"):
        reward = ", ".join(filter(None, [reward, "item: " + ", ".join(rw["items"])]))
    return f"⚔️ **{name}**: " + " + ".join(parts) + (f" · 💰 {reward}" if reward else "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "build/story/chapter1.md"))
    args = ap.parse_args()
    campaign = json.load(open(ROOT / "data/campaign.json"))
    items = {i["id"]: i for i in json.load(open(ROOT / "data/items.json")).get("items", [])}
    raw_units = json.load(open(ROOT / "data/units.json"))
    unit_list = raw_units if isinstance(raw_units, list) else raw_units.get("units", list(raw_units.values()))
    units = {u["id"]: u for u in unit_list if isinstance(u, dict) and "id" in u}
    md = ["# Chapter 1 — The Marches", "",
          "Generated from the game data by `scripts/chapter1_doc.py`. The story's source is the header of "
          "`scripts/chapter1_story.py`. Symbols: 💬 dialogue · ⚔️ battle · 👁️ discovery · 📜 quest · "
          "🔍 clue · 💰 reward · 🏆 chapter beat.", ""]
    for ch in campaign["chapters"]:
        for m in ch["maps"]:
            trig = json.load(open(ROOT / "data" / m["triggers"]))
            enc = json.load(open(ROOT / "data" / m["encounters"]))
            md += [f"## {m['id']} {m['title']}", ""]
            party = ", ".join(c.title() for c in trig.get("start_companions", [])) or "none"
            md.append(f"Party at start: {party} · level cap {m.get('max_level', '?')}")
            md.append("")
            for q in trig.get("quests", []):
                md.append(f"📜 **{q['title']}**{' (main)' if q.get('main') else ''}: {q['text']}")
            md.append("")
            for name, e in enc.get("objects", {}).items():
                md.append(guard_text(name, e, units))
            md.append("")
            seen = set()
            for n, t in enumerate(trig["triggers"], 1):
                key = json.dumps(t.get("do", []), sort_keys=True)
                if key in seen:
                    continue  # the same beat offered again (after "Not yet"): shown once
                seen.add(key)
                gate = f" *(if {cond_text(t['if'])})*" if "if" in t else ""
                after = f" *(after {t['when']['after']})*" if "after" in t.get("when", {}) else ""
                body = actions(t.get("do", []), items)
                if not body:
                    continue
                md.append(f"**{m['id']}.{n} {when_text(t.get('when', {}))}**{after}{gate}")
                md.append("")
                for l in body:
                    stripped = l.lstrip(" ")
                    depth = (len(l) - len(stripped)) // 2
                    if stripped.startswith("- "):
                        stripped = stripped[2:]
                    md.append("    " * depth + "- " + stripped)
                md.append("")
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text("\n".join(md))
    print(f"Wrote {args.out}: {len(md)} lines")


if __name__ == "__main__":
    main()
