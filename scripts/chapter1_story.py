#!/usr/bin/env python3
"""
chapter1_story.py — the story of chapter 1, "The Marches": triggers, encounters and
data/campaign.json for the four maps gen_chapter1.py draws.

The chapter asks one question: where did Aldren go? Three names to remember: Aldren,
the hooded man (never named), Ushari. Everyone else is a role. Play tells, talk
decides: dialogue only where a choice is made, three lines at most per beat. Every
resolved choice records the flag choice.<id> = <option id> (Scenario::choose), so the
heavy decisions carry forward without extra authoring:

    choice.toast        raise_cup | leave_hall          (1.0, your father's toast)
    choice.stranger     hear | attack                   (1.1, the hooded man)
    choice.camp_*       grant | tax | turn_away         (1.2, each camp)
    choice.red_banner   let_pass | make_pay | drive_off (1.2, the first heavy decision)
    choice.ushari_road  take | send_home                (1.3)
    choice.roots        cut | turn_back                 (1.3, the chapter's last decision)

Author knowledge (not for the player): the hooded man is Hul'rik; Aldren went east for
the old passage and cut the roots at the mine mouth; the red hand is the emperor's mark.

Writes DIR/ch1_*.triggers.json and DIR/ch1_*.encounters.json, and campaign.json in the
data dir (default: the parent of DIR), then checks the result against the maps in DIR.

Usage:  python3 scripts/chapter1_story.py [--out DIR] [--data-out DIR]
        (defaults data/maps and data; run gen_chapter1.py first)
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_chapter1 as gen  # noqa: E402  (object names, exits and chokes)

ROOT = gen.ROOT


def say(*lines):
    return {"say": [list(line) for line in lines]}


def lore(title, text):
    return {"lore": [title, text]}


def mad_dogs(n, gold=0):
    """1.0 only: the dogs look sick but do not bite-infect (the prologue's promise)."""
    return {"guards": [{"id": "feral_dog", "count": n}], "reward": {"Gold": gold} if gold else {}}


def dogs(n, gold=0):
    """1.1 on: the promise kept; their bite carries the rabies."""
    return {"guards": [{"id": "rabid_hound", "count": n}], "reward": {"Gold": gold} if gold else {}}


def conversation(id, title, lines, text, options):
    c = {"id": id, "title": title, "text": text, "options": options}
    if lines:
        c["presentation"] = "conversation"
        c["lines"] = [list(l) for l in lines]
    return {"choice": c}


def option(id, label, *then, detail=None):
    o = {"id": id, "label": label, "then": list(then)}
    if detail:
        o["detail"] = detail
    return o


class Story:
    def __init__(self, stem, start_companions):
        self.stem = stem
        self.root = {"start_companions": start_companions, "quests": [], "triggers": []}
        self.encounters = {"defaults": {"guard": dogs(10)}, "objects": {}, "pickups": {}}

    def quest(self, id, title, text, main=False):
        self.root["quests"].append({"id": id, "title": title, "text": text, "main": main})

    def trig(self, id, when, *actions):
        self.root["triggers"].append({"id": id, "when": when, "do": list(actions)})

    def leave_by(self, exit_q, after, choice_id, title, text, go_label, ending):
        """The way out: offered when the hero reaches the exit; 'Not yet' asks again next time."""
        opts = lambda: [option("go", go_label, {"ending": ending}), option("not_yet", "Not yet")]
        prev = None
        for i in range(4):
            when = {"event": "enter_q", "q_min": exit_q, "after": prev or after}
            if prev:
                when["flag"] = {f"choice.{choice_id}": "not_yet"}
            tid = f"{choice_id}_{i + 1}"
            self.trig(tid, when, conversation(choice_id, title, [], text, opts()))
            prev = tid


# ── 1.0 The last hunt ────────────────────────────────────────────────────────
def hunt():
    s = Story("ch1_1_0_hunt", ["aldren"])
    # Not read by the engine yet: a new campaign starts with the Godot default army
    # (24 runners, 10 slingers, 3 blades). The hunt wants a small household party.
    s.root["_comment"] = "start_army is not read by the engine yet; see the chapter 1 report."
    s.root["start_army"] = [{"id": "woad_runner", "count": 10}, {"id": "cruth_slinger", "count": 4}]
    s.quest("hunt", "The Last Hunt", "Two packs of dogs have been killing the herds. They foam at the mouth and do not run from people. Hunt them down with Aldren.", main=True)
    s.quest("hall", "The Hall", "The hunt is over. End the day: your father is waiting in the hall.")
    s.encounters["objects"] = {"Thicket Dogs": mad_dogs(6), "Stream Dogs": mad_dogs(8)}

    s.trig("start", {"event": "start"},
           {"quest": "hunt"}, {"reveal": [0, 0, 10]},
           say(("Herdsman", "They came right into the fold, my lord. They bit my boy."),
               ("Aldren", "Two packs before dark. Keep up, little brother.")))
    s.trig("hunt_done", {"event": "cleared", "names": ["Thicket Dogs", "Stream Dogs"]},
           {"quest_done": "hunt"}, {"quest": "hall"},
           say(("Aldren", "Dogs don't fight like that. Something is wrong with them.")))
    s.trig("toast", {"event": "night", "after": "hunt_done"},
           {"quest_done": "hall"},
           conversation("toast", "Varenhold · The hall · That night",
                        [("Your father", "To Aldren. To my heir.")],
                        "Every cup in the hall goes up.",
                        [option("raise_cup", "Raise your cup to him", {"event": "dawn"}),
                         option("leave_hall", "Leave the hall", {"event": "dawn"})]))
    s.trig("dawn", {"event": "dawn"},
           {"leave": "aldren"},
           {"item": "red_hand_sword"},
           lore("Aldren's letter", "“The sword is yours now. Do not come after me. — A.”"),
           {"xp": 60},
           {"ending": {"id": "gone_at_dawn", "heading": "Gone at Dawn", "next": "1.1",
                       "body": "Aldren rode out before first light. His sword is yours now.",
                       "frames": [{"scene": "home", "text": "Dawn. Aldren's horse is gone from the stable."},
                                  {"scene": "home", "text": "On your bed lies his sword,\nand a letter in his hand."}]}})
    return s


# ── 1.1 Varenhold ────────────────────────────────────────────────────────────
PACKS = ["Orchard Dogs", "Ford Dogs", "Hollow Dogs"]
KEEPER = "Bridge Keeper"


def varenhold(land):
    s = Story("ch1_1_1_varenhold", [])
    s.quest("aldren", "Where did Aldren go?", "Aldren rode out at dawn and left you his sword. The only road out of the vale runs east, over the bridge.", main=True)
    s.quest("dogs", "Rabid Dogs in the Vale", "More of the mad dogs: three packs loose in the vale, and their bite carries the sickness. Kill them before they get at the herds.")
    s.encounters["objects"] = {"Orchard Dogs": dogs(12, 150), "Ford Dogs": dogs(14, 150),
                               "Hollow Dogs": dogs(18, 200)}
    s.encounters["pickups"] = {"Shepherd's Purse": {"reward": {"Gold": 300}}}

    s.trig("start", {"event": "start"}, {"quest": "aldren"}, {"quest": "dogs"}, {"reveal": [2, 0, 13]})
    # The hooded man stands with the third pack. Both answers end in the fight.
    s.trig("stranger", {"event": "engage", "name": "Hollow Dogs"},
           conversation("stranger", "The Hooded Stranger", [("Hooded Man", "Son of Varen.")],
                        "A hooded man stands among the dogs.",
                        [option("hear", "Hear him out",
                                say(("Hooded Man", "Your brother would not listen either. Keep this warm.")),
                                {"item": "hermits_seed"}),
                         option("attack", "Attack")]))
    s.trig("stranger_gone", {"event": "encounter_won", "name": "Hollow Dogs"},
           {"vanish": "Hooded Stranger"},
           say(("Scout", "He is gone. Only roots, pushed up through frozen ground.")))
    s.trig("rune_stone", {"event": "visit", "name": "Vale Rune Stone"},
           lore("A rubbing of the rune stone", "Charcoal on cloth. No one at Varenhold can read it."))
    s.trig("barrow", {"event": "visit", "name": "Varen Barrow"},
           say(("Scout", "The door will not open.")))
    # The east bridge stays shut while the dogs run loose. Each trigger fires once,
    # so the keeper says his piece on the first visit; the quest text keeps the reason.
    s.trig("gate_shut", {"event": "visit", "name": KEEPER, "unless": "vale_cleared"},
           say(("Bridge Keeper", "We won't open until the rabid dogs are killed.")))
    s.trig("vale_cleared", {"event": "cleared", "names": PACKS},
           {"xp": 85}, {"quest_done": "dogs"},
           {"quest_text": ["aldren", "The rabid dogs are dead. The Bridge Keeper will open the east bridge now."]})
    s.trig("gate_open", {"event": "visit", "name": KEEPER, "after": "vale_cleared"},
           say(("Bridge Keeper", "Lord Aldren crossed at first light, before we shut it.")),
           {"open": list(land.gate)},
           {"quest_text": ["aldren", "The bridge is open. Aldren crossed it at first light; follow his road east."]})
    s.leave_by(land.exit_q, "gate_open", "leave_vale", "The East Road",
               "The road leaves the vale here.", "Ride east",
               {"id": "out_of_the_vale", "heading": "Out of the Vale", "next": "1.2",
                "body": "You ride out of the vale by the east road, the way Aldren went.",
                "frames": [{"scene": "road", "text": "The hills close behind you.\nAhead, the river fields."}]})
    return s


# ── 1.2 The camps ────────────────────────────────────────────────────────────
CAMPS = [  # (choice id, object, speaker, line, toll, what turning them away means)
    ("camp_riverside", "Riverside Camp", "Captain",
     "Every house that can lift a spear is marching for Umm'Natur. One night on your fields, my lord.",
     400, "fight"),
    ("camp_meadow", "Meadow Camp", "Captain",
     "We are a small house and a long way from home. We will be gone by morning.",
     300, "leave"),
    ("camp_strangers", "Strangers' Camp", "Ushari",
     "I am Ushari. We are four, and we pay our way.",
     150, "leave"),
]
RIVERSIDE_BAND = [{"id": "woad_runner", "count": 12}, {"id": "cruth_slinger", "count": 4}]
RED_BAND = [{"id": "woad_runner", "count": 16}, {"id": "cruth_slinger", "count": 6}, {"id": "painted_blade", "count": 3}]
CAMP_XP = 35


def camps(land):
    s = Story("ch1_1_2_camps", [])
    s.quest("aldren", "Where did Aldren go?", "Aldren's road runs east across the river fields. Follow it.", main=True)
    s.quest("camps", "Camps on Your Fields",
            "Companies from other houses have camped on your fields on their way east. Ride to each camp and decide: let them stay, take a toll, or turn them away.")
    s.encounters["objects"] = {"Copse Dogs": dogs(12, 200),
                               "Field Brigands": {"guards": [{"id": "brigand", "count": 12}], "reward": {"Gold": 300}}}
    s.encounters["pickups"] = {"Brigand Purse": {"reward": {"Gold": 500}}}

    s.trig("start", {"event": "start"}, {"quest": "aldren"}, {"quest": "camps"}, {"reveal": [-4, 2, 9]})
    # Where the dogs came from: northern settlers on the road south (not named yet; the
    # user names them). Only what is seen; no faction name.
    s.trig("empty_camp", {"event": "visit", "name": "Empty Camp"},
           lore("An empty camp", "Dog-pens of split pine, their gates hanging open, and sled tracks coming down out of the north. "
                "A cut leash lies in the mud, on a studded collar no one in the Marches makes. "
                "Snagged on a gatepost: a scrap of white fur, and a few strands of pale hair."))
    for cid, name, speaker, line, toll, refused in CAMPS:
        gone = {"vanish": name}
        turn = ([{"betray": {"at": name, "band": "Riverside Company", "army": RIVERSIDE_BAND}}, gone]
                if refused == "fight" else [gone])
        s.trig(cid, {"event": "visit", "name": name},
               conversation(cid, name, [(speaker, line)], "",
                            [option("grant", "Let them stay", gone, {"xp": CAMP_XP}, {"event": "camp_decided"}),
                             option("tax", "Take a toll", {"give": {"Gold": toll}}, gone, {"xp": CAMP_XP},
                                    {"event": "camp_decided"}),
                             option("turn_away", "Turn them away", *turn, {"xp": CAMP_XP}, {"event": "camp_decided"},
                                    detail="They will not go quietly." if refused == "fight" else None)]))
    # The first heavy decision. The camp holds the only east bank of the burn.
    s.trig("red_banner", {"event": "visit", "name": "Red Banner Camp"},
           conversation("red_banner", "The Red Banner",
                        [("Captain", "We cross your bridge tonight, and we take what we need from your fields. You may watch.")],
                        "A red hand is painted on their banner.",
                        [option("let_pass", "Let them pass", {"vanish": "Red Banner Camp"}, {"xp": CAMP_XP},
                                {"event": "camp_decided"}),
                         option("make_pay", "Make them pay", {"give": {"Gold": 600}}, {"vanish": "Red Banner Camp"},
                                {"xp": CAMP_XP}, {"event": "camp_decided"}),
                         option("drive_off", "Drive them off",
                                {"betray": {"at": "Red Banner Camp", "band": "Red Banner Company", "army": RED_BAND}},
                                {"vanish": "Red Banner Camp"}, {"xp": CAMP_XP}, {"event": "camp_decided"},
                                detail="They will fight for the bridge.")]))
    s.trig("red_banner_broken", {"event": "rival_beaten", "name": "Red Banner Company"},
           {"flag": {"red_banner_broken": True}})
    s.trig("riverside_broken", {"event": "rival_beaten", "name": "Riverside Company"},
           {"flag": {"riverside_broken": True}})
    s.trig("camps_done", {"event": "camp_decided",
                          "flag": [f"choice.{c[0]}" for c in CAMPS] + ["choice.red_banner"]},
           {"quest_done": "camps"})
    s.leave_by(land.exit_q, "red_banner", "east_road", "The East Road",
               "The road climbs east toward the hills.", "Take the east road",
               {"id": "the_east_road", "heading": "The East Road", "next": "1.3",
                "body": "The camps are behind you. The road climbs east toward the hills.",
                "frames": [{"scene": "road", "text": "The river fields fall behind.\nThe road climbs."}]})
    return s


# ── 1.3 The east road ────────────────────────────────────────────────────────
def east_road(land):
    s = Story("ch1_1_3_east_road", [])
    s.quest("aldren", "Where did Aldren go?", "The east road climbs toward the mountains. Follow it to its end.", main=True)
    s.encounters["objects"] = {
        "Road Dogs": dogs(16, 200),
        "Ridge Bears": {"guards": [{"id": "highland_bear", "count": 4}], "reward": {"Gold": 300}},
        "Stone Trolls": {"guards": [{"id": "forest_troll", "count": 2}], "reward": {"Gold": 400}},
        "Root Wardens": {"guards": [{"id": "ancient_treant", "count": 1}]},
    }
    s.encounters["pickups"] = {"Fallen Pine": {"reward": {"Wood": 4}}, "Scree Stone": {"reward": {"Stone": 4}}}

    s.trig("start", {"event": "start"}, {"quest": "aldren"})
    # Aldren's traces, off the road, easy to miss. No one explains them.
    s.trig("cold_fire", {"event": "visit", "name": "Cold Fire"},
           lore("A cold fire", "Ashes banked the careful way Aldren always banked them. Grass flattened for one bedroll."))
    s.trig("bridge_post", {"event": "visit", "name": "Bridge Post"},
           lore("The bridge post", "Cut into the post with a knife: an arrow pointing east, and an A, in the hand of his letter."))
    # Ushari's company met the red banner on the road. What she says first depends on 1.2.
    for tid, cond, line in [
        ("ushari_drove", {"choice.red_banner": "drive_off"}, "The ones you drove from your bridge. They found us instead."),
        ("ushari_passed", {"choice.red_banner": "let_pass"}, "The ones you let across your bridge. Red hands on their shields."),
        ("ushari_paid", {"choice.red_banner": "make_pay"}, "The ones who paid for your bridge. They took it back from us."),
    ]:
        s.trig(tid, {"event": "visit", "name": "Broken Camp", "flag": cond}, say(("Ushari", line)))
    s.trig("ushari_night", {"event": "visit", "name": "Broken Camp", "unless_flag": "choice.red_banner"},
           say(("Ushari", "They came in the night. Red hands on their shields.")))
    for tid, answer, line in [
        ("ushari_rested", "grant", "You let us rest on your land. They did not ask."),
        ("ushari_taxed", "tax", "We paid you for the road. They took the rest."),
        ("ushari_turned", "turn_away", "You turned us off your land. They did not bother."),
    ]:
        s.trig(tid, {"event": "visit", "name": "Broken Camp", "flag": {"choice.camp_strangers": answer}},
               say(("Ushari", line)))
    s.trig("ushari_road", {"event": "visit", "name": "Broken Camp"},
           conversation("ushari_road", "The Broken Camp", [], "Her company is broken. She is the last one standing.",
                        [option("take", "Take her with you", {"join": "ushari"}),
                         option("send_home", "Send her home")]))
    s.trig("wardens_won", {"event": "encounter_won", "name": "Root Wardens"})
    # The last decision of the chapter; "Turn back" asks again on the next visit.
    prev = None
    for i in range(4):
        when = {"event": "visit", "name": "Root-Grown Mine", "after": prev or "wardens_won"}
        if prev:
            when["flag"] = {"choice.roots": "turn_back"}
        tid = f"roots_{i + 1}"
        s.trig(tid, when, conversation(
            "roots", "The Roots", [],
            "Roots have grown across the mine mouth. Someone cut through them not long ago, and they have closed again.",
            [option("cut", "Cut the roots", {"ending": {
                "id": "the_roots", "heading": "The Roots",
                "body": "You cut through the roots and go down into the dark after him.",
                "frames": [{"scene": "road", "text": "The roots part under the axe.\nCold air rises out of the dark."},
                           {"scene": "road", "text": "Aldren went this way.\nSo do you."}]}}),
             option("turn_back", "Turn back")]))
        prev = tid
    return s


# ── campaign ─────────────────────────────────────────────────────────────────
TITLES = {"ch1_1_0_hunt": ("1.0", "The last hunt", 2), "ch1_1_1_varenhold": ("1.1", "Varenhold", 3),
          "ch1_1_2_camps": ("1.2", "The camps", 4), "ch1_1_3_east_road": ("1.3", "The east road", 5)}


def campaign(stories, maps_rel):
    maps = []
    for s in stories:
        id, title, cap = TITLES[s.stem]
        maps.append({"id": id, "title": title, "map": f"{maps_rel}/{s.stem}.json",
                     "encounters": f"{maps_rel}/{s.stem}.encounters.json",
                     "triggers": f"{maps_rel}/{s.stem}.triggers.json", "max_level": cap})
    return {"chapters": [{"id": "marches", "title": "The Marches", "maps": maps}]}


# ── checks ───────────────────────────────────────────────────────────────────
XP_TABLE = [0, 0, 100, 250, 450, 700, 1500]


def power(units, army):
    total = 0.0
    for st in army:
        u = units[st["id"]]
        hits = (u["minDamage"] + u["maxDamage"]) * 0.5 + 1.0
        big = 0.35 if u["hitPoints"] >= 100 else 1.0
        total += st["count"] * u["hitPoints"] * (u["attack"] + u["defense"] + 5) * hits * (1.3 if u["shots"] > 0 else 1.0) * big
    return total / 100.0


def lines_of(actions):
    out = []
    for a in actions:
        out += [tuple(l) for l in a.get("say", [])]
        if "choice" in a:
            out += [tuple(l) for l in a["choice"].get("lines", [])]
    return out


def walk_actions(actions):
    for a in actions:
        yield a
        for o in a.get("choice", {}).get("options", []):
            yield from walk_actions(o.get("then", []))


def check(stories, lands, out, data_out):
    units = {u["id"]: u for u in json.loads((ROOT / "data/units.json").read_text())["units"]}
    items = {i["id"] for i in json.loads((ROOT / "data/items.json").read_text())["items"]}
    problems, report = [], []
    camp = json.loads((data_out / "campaign.json").read_text())
    for entry in camp["chapters"][0]["maps"]:
        for key in ("map", "encounters", "triggers"):
            if not (data_out / entry[key]).exists():
                problems.append(f"campaign.json: {entry['id']} {key} {entry[key]} does not exist")
    choice_ids = {}
    for s, land in zip(stories, lands):
        m = json.loads((out / f"{s.stem}.json").read_text())
        tiles = {(t["q"], t["r"]): t for t in m["tiles"]}
        objs = {o["name"]: o for o in m["objects"]}
        cell = {n: (o["q"], o["r"]) for n, o in objs.items()}
        guards = {n for n, o in objs.items() if o["type"] == "guard"}
        opened = {tuple(a["open"]) for t in s.root["triggers"] for a in walk_actions(t["do"]) if "open" in a}
        for c in opened:
            if c not in tiles or tiles[c]["passable"]:
                problems.append(f"{s.stem}: 'open' {list(c)} is not a closed tile on the map")
        ok = lambda c: c in tiles and (tiles[c]["passable"] or c in opened)
        start = (m["start"]["q"], m["start"]["r"])

        def reach(blocked=frozenset()):
            seen, stack = {start}, [start]
            while stack:
                c = stack.pop()
                for n in gen.nbrs(c):
                    if n not in seen and n not in blocked and ok(n):
                        seen.add(n)
                        stack.append(n)
            return seen
        seen = reach()
        for n, c in cell.items():
            if c not in seen:
                problems.append(f"{s.stem}: {n} cannot be reached from the start")
        if land.exit_q is not None and not any(c[0] >= land.exit_q for c in seen):
            problems.append(f"{s.stem}: no walkable cell at the exit (q >= {land.exit_q})")
        if opened and land.exit_q is not None and any(c[0] >= land.exit_q for c in reach(frozenset(opened))):
            problems.append(f"{s.stem}: the exit can be reached without opening {sorted(opened)}")
        # Sites stood next to a guard are guarded on purpose; list them.
        guarded = sorted(f"{n} (by {g})" for n, c in cell.items() if n not in guards
                         for g in guards if gen.dist(c, cell[g]) == 1)
        # Chokes: block the site (and a guard's zone) and the target falls out of reach.
        for name, target in land.chokes.items():
            blocked = {cell[name]} | ({x for x in gen.nbrs(cell[name])} if name in guards else set())
            blocked.discard(cell.get(target))
            after = reach(frozenset(blocked))
            if target == "exit":
                if any(c[0] >= land.exit_q for c in after):
                    problems.append(f"{s.stem}: the exit can be reached around {name}")
            elif cell[target] in after:
                problems.append(f"{s.stem}: {target} can be reached around {name}")
        # Every name the triggers and encounters use is on the map.
        named = set()
        for t in s.root["triggers"]:
            w = t["when"]
            if "name" in w and w["event"] not in ("rival_beaten",):
                named.add(w["name"])
            named.update(w.get("names", []))
            for a in walk_actions(t["do"]):
                if "vanish" in a:
                    named.add(a["vanish"])
                if "betray" in a:
                    named.add(a["betray"]["at"])
                if "item" in a and a["item"] not in items:
                    problems.append(f"{s.stem}: item {a['item']} is not in items.json")
                if "join" in a and a["join"] not in units:
                    problems.append(f"{s.stem}: companion {a['join']} is not in units.json")
                if "choice" in a:
                    choice_ids.setdefault(a["choice"]["id"], set()).add(s.stem)
        named.update(s.encounters["objects"])
        named.update(s.encounters["pickups"])
        for n in sorted(named - set(objs)):
            problems.append(f"{s.stem}: '{n}' is named in the story but not on the map")
        for n in s.encounters["objects"]:
            if n in objs and objs[n]["type"] != "guard":
                problems.append(f"{s.stem}: encounter '{n}' is not a guard")
        for n in sorted(guards - set(s.encounters["objects"])):
            problems.append(f"{s.stem}: guard '{n}' has no encounter (would use the default)")
        for c in s.root["start_companions"]:
            if c not in units:
                problems.append(f"{s.stem}: start companion {c} is not in units.json")
        # Three lines a beat at most (lines before a choice plus the longest answer).
        for t in s.root["triggers"]:
            before = lines_of(t["do"])
            answer = max([len(lines_of(o.get("then", []))) for a in t["do"] if "choice" in a
                          for o in a["choice"]["options"]] or [0])
            if len(before) + answer > 3:
                problems.append(f"{s.stem}: trigger {t['id']} has {len(before) + answer} lines")
        # Experience: what the guards and the story pay against the level cap.
        guard_xp = {n: max(20, int(power(units, e["guards"]) / 3)) for n, e in s.encounters["objects"].items()}
        story_xp = sum(a.get("xp", 0) + max([sum(b.get("xp", 0) for b in o.get("then", []))
                                             for o in a.get("choice", {}).get("options", [])] or [0])
                       for t in s.root["triggers"] for a in t["do"])
        types = {}
        for o in m["objects"]:
            types[o["type"]] = types.get(o["type"], 0) + 1
        walkable = sum(1 for t in m["tiles"] if t["passable"])
        report.append(f"  {s.stem}: {len(m['tiles'])} tiles ({walkable} walkable), objects "
                      + ", ".join(f"{k} {v}" for k, v in sorted(types.items()))
                      + f"; guard xp {sum(guard_xp.values())} ({', '.join(f'{k} {v}' for k, v in guard_xp.items())}), "
                      + f"story xp up to {story_xp}; cap {TITLES[s.stem][2]} at {XP_TABLE[TITLES[s.stem][2]]} xp"
                      + (f"; guarded sites: {', '.join(guarded)}" if guarded else ""))
    for cid, stems in choice_ids.items():
        if len(stems) > 1:
            problems.append(f"choice id {cid} is used on several maps: {sorted(stems)}")
    return problems, report


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--out", type=Path, default=ROOT / "data" / "maps")
    ap.add_argument("--data-out", type=Path, default=None)
    args = ap.parse_args()
    data_out = args.data_out or args.out.parent
    args.out.mkdir(parents=True, exist_ok=True)
    lands = gen.build_all()
    by_stem = {l.stem: l for l in lands}
    stories = [hunt(), varenhold(by_stem["ch1_1_1_varenhold"]), camps(by_stem["ch1_1_2_camps"]),
               east_road(by_stem["ch1_1_3_east_road"])]
    for s in stories:
        (args.out / f"{s.stem}.triggers.json").write_text(json.dumps(s.root, indent=2, ensure_ascii=False) + "\n")
        enc = {"_comment": f"Guard armies and rewards for {s.stem}.json, keyed by object name. Generated by scripts/chapter1_story.py."}
        enc.update(s.encounters)
        (args.out / f"{s.stem}.encounters.json").write_text(json.dumps(enc, indent=2, ensure_ascii=False) + "\n")
        n_lines = sum(len(lines_of([a])) for t in s.root["triggers"] for a in walk_actions(t["do"]))
        print(f"Wrote {s.stem}.triggers.json: {len(s.root['quests'])} quests, {len(s.root['triggers'])} triggers, "
              f"{n_lines} dialogue lines; {s.stem}.encounters.json: {len(s.encounters['objects'])} guards")
    maps_rel = Path(args.out).resolve().relative_to(Path(data_out).resolve()).as_posix()
    (data_out / "campaign.json").write_text(json.dumps(campaign(stories, maps_rel), indent=2) + "\n")
    print(f"Wrote campaign.json: chapter 'The Marches', {len(stories)} maps")
    problems, report = check(stories, lands, args.out, data_out)
    print("\n".join(report))
    if problems:
        print("CHECK FAILED:\n  " + "\n  ".join(problems))
        sys.exit(1)
    print("Check passed: every named object exists, every site is reachable, chokes hold, campaign paths exist.")


if __name__ == "__main__":
    main()
