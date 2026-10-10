#!/usr/bin/env python3
"""
chapter1_story.py — the story of Chapter 1, "The Marches": triggers, encounters and
data/campaign.json for the four maps gen_chapter1.py draws.

This docstring is the single source of the Chapter 1 story. Keep it true when a beat
changes. Play tells, talk decides: dialogue only where a choice is made, three lines
at most per beat. The next thing to do is never hidden; the mystery may hide the rest.

THE QUESTION: where did Aldren go? Every map moves it one step east.

CAST (only Aldren, Drest and Ushari are named in play; everyone else is a role)
    The hero          House Varen's younger son, mid-twenties. Never named.
    Aldren            The heir, early thirties. Never waits for anyone. In battle he is
                      reckless: the game plays his turns, he charges and chops through the
                      trees. Rides out at first light after the hunt and leaves the sword
                      of the forefathers on the wall: "Do not take it." Unseen after 1.0.
    Drest             Master-at-arms; taught both brothers. Rides through the whole chapter
                      (essential: never leaves over pay). Few words, calls the hero "boy".
                      Dies later, near the Walled Nation (not in this chapter).
    Your father       Lord of Varenhold, old. One line: the toast to his heir (told, not chosen).
    The hooded man    The king, your father's brother. Never named in play: a hooded man
                      among the dogs in 1.1 who knows Aldren and hands over a warm seed.
    Ushari            A witch of the realm's last witch tribe (the people the colonizing
                      houses conquered). Always joins in 1.2: at the Strangers' Camp, or, if
                      the player never went there, she rides up at the Red Banner bridge.
    Northern Warrior  The first of the Ice people (unnamed as a people). His tongue is never
                      translated; "Drusk" is his dog's name. He comes with kin (three stacks).
    The herdsman      One line in 1.0: the dogs bit his boy.
    Party: two companions at most (engine rule). 1.0 Aldren + Drest; 1.1 Drest;
    from 1.2 Drest + Ushari.

THREADS
    Dog tag      1.0 dropped by the Stream Dogs (marks no one can read) -> 1.1 the Vale Rune
                 Stone has the same marks -> 1.2 the Northern Warrior is the dog's owner;
                 handing him the tag ends the meeting without a fight.
    Rabies       1.0 the dogs look rabid but the bite does nothing (the promise); from 1.1
                 the rabid_hound bite carries the sickness (the threat). No town after 1.1;
                 the cure comes later with Ushari's ability (not built).
    Red hand     1.0 on the hilt of the sword (seen at dawn, either answer) -> 1.1 cut into the
                 Varen Barrow door (Drest) -> 1.2 on Aldren's shield (Ushari saw it; Drest
                 knows it) and on the Red Banner company's banner and shields (Drest, once)
                 -> 1.3 painted on the cart at the Broken Camp. The emperor's mark; never said.
    The sword    1.0 dawn: take it (he asked you not to) or leave it on the wall. Taken, it is
                 the strongest weapon of the chapter, and a gamble: it draws hunters who pay
                 in experience and gold. 1.1 the hooded man knows it ("That hand went east
                 before you."), the quest and the barrow door name it. 1.2 Ushari sees the
                 hand of Aldren's shield on it; a band of Sword Hunters rides out after it;
                 the Red Banner captain wants it, and it buys the bridge outright (handed
                 over, the hunters of 1.3 never come); at the road out Ushari says "He asked
                 me the way to the roots." (flag ushari_roots). 1.3 that flag shows the mine
                 and his cold fire from the start; a band waits at the far end of the
                 bridge while the sword is carried. Left on the wall: none of the hunters,
                 and his traces must be found.
    Aldren's road  1.1 the Bridge Keeper: he crossed at first light -> 1.2 Ushari: a man
                 with a red hand on his shield rode past her going east (Drest: "Aldren's
                 shield.") -> 1.3 his cold fire, his "A" on the bridge post, roots cut and
                 grown back over the mine mouth.
    His habits   1.0 he cuts an A into the bark after the Thicket Dogs, and banks the old
                 fire at the Poachers' Fire ("You taught me.") -> 1.3 the A on the Bridge
                 Post, the banked Cold Fire (Drest: "He was listening, then."). In battle he
                 rushes ahead (Drest: "Stay back behind the lines!") -> 1.1 when the hero's
                 stack rushes, Drest: "Your brother would have done that. Don't."

MAP BY MAP, and what the player must understand to act
    1.0 The Last Hunt   Two packs of mad dogs with Aldren and Drest; the tag drops from the
                        Stream Dogs. The woods are a labyrinth of narrow trails: the Thicket
                        Dogs wait in a tree corridor (an ambush battle), the Stream Dogs past
                        the one bridge. The Hunters' Lodge on the trail sells slingers; two
                        dead-end side paths hold the Poachers' Fire and cut willow to pay
                        for them. End the day: the toast to Aldren (no choice). At first
                        light Aldren is gone; the sword hangs in the hall with his note:
                        take it or leave it on the wall.
                        Understand: Aldren is the heir; the dogs are wrong; he has left.
    1.1 Varenhold       The home vale. Three rabid packs; the Bridge Keeper keeps the only
                        road out (east) shut until they are dead. The hooded man stands with
                        the third pack. Optional: the rune stone, the sealed barrow.
                        Understand: kill the packs, the keeper opens the bridge, go east.
    1.2 The Camps       Companies of other houses camp on your fields, marching east. Each
                        camp is a choice: let them stay, take a toll, turn them away. The
                        Strangers' Camp is Ushari's: a man with the red hand on his shield
                        passed her going east (Aldren), so she rides with you whatever you
                        answer; her three go on
                        ahead, and your answer colours what she says in 1.3. Her camp is off
                        the road: a player who skips it meets her at the Red Banner bridge,
                        where she rides up, names herself and joins. The Empty Camp and
                        the Northern Warrior show where the dogs came from: the north. The Red
                        Banner company holds the only bridge east; let them pass, make them
                        pay, drive them off, or (sword carried) hand over the sword. The road
                        out opens only after that. Sword carried: Sword Hunters ride out at
                        the start (they fall on the hero when half again his army's strength,
                        else they take his mills).
                        Understand: the way east runs through the Red Banner's bridge.
    1.3 The East Road   One road into the Greyfangs. Off it, Aldren's traces. At the Broken
                        Camp Ushari finds her company broken by the red hands and answers what
                        you did at the bridge and at her camp (a neutral line if she joined at
                        the bridge). A map started on its own (no 1.2 behind it) finds her here,
                        the last one standing, and you decide whether she comes; a campaign run
                        never does. Roots close the old mine mouth; someone cut them and they grew
                        back. Cut the roots (end of chapter) or turn back.
                        Understand: Aldren went down into the mine.

CHOICES. Every resolved choice records the flag choice.<id> = <option id>
(Scenario::choose), so heavy decisions carry forward without extra authoring:

    choice.sword        take | leave                    (1.0, dawn: the sword of your forefathers)
    choice.stranger     hear | attack                   (1.1, the hooded man)
    choice.camp_*       grant | tax | turn_away         (1.2, each camp; Ushari joins on all three;
                                                     unset for camp_strangers if she joined at the bridge)
    choice.red_banner   let_pass | make_pay | drive_off (1.2, the first heavy decision;
                        | hand_over                      hand_over only while the sword is carried)
    choice.warrior      give_tag | fight                (1.2, the dog's owner; give_tag needs the 1.0 dog tag)
    choice.ushari_road  take | send_home                (1.3, only on a map started on its own)
    choice.roots        cut | turn_back                 (1.3, the chapter's last decision)

Flags beyond choices: ushari_roots (1.2, she told you the way; 1.3 reveals the mine and
the Cold Fire), red_banner_broken, riverside_broken.

PLANNED, NOT BUILT
    The Varen Barrow dungeon (1.1): sealed today; when built it holds the clue to Aldren's
    road (sword left: the only place to learn it; taken: an extra clue).
    Three roads out of 1.1 (today only the east bridge); the hooded man's sword line then
    picks the road.
    Ushari's cure for the rabies.

Author knowledge (never told in Chapter 1): the hooded man is the king; Aldren went east
for the old passage and cut the roots at the mine mouth; the red hand is the emperor's mark.
Why Aldren left is the same thing as the fall of Umm'Natur (decided; how it is shown is open).
The dogs (decided 2026-10-10): the Northerners use dogs for everything; a disease spreads
among their dogs, and the people from Under spread it. Under works by curses and illness.
The sword, far future (decided 2026-10-10): handed over in 1.2, it is won back from the
Red Banner in 3.2. Left on the wall, Drest kept it all along; dying in chapter 4, near
the Walled Nation, he hands it over: "He said don't take it. He never said I couldn't
give it." (take or leave again, a second choice). Taken: Drest, dying: "Give it back to
him, if you find him." The hilt is the first link of the mark chain (hilt, banner, tomb
door, crown, 4.3).

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


def mad_dogs(n, gold=0, packs=1):
    """1.0 only: the dogs look sick but do not bite-infect (the prologue's promise).
    packs > 1 splits them into stacks, so an ambush layout can bring them in from several sides."""
    counts = [n // packs + (1 if i < n % packs else 0) for i in range(packs)]
    return {"guards": [{"id": "feral_dog", "count": c} for c in counts], "reward": {"Gold": gold} if gold else {}}


def dogs(n, gold=0):
    """1.1 on: the promise kept; their bite carries the rabies."""
    return {"guards": [{"id": "rabid_hound", "count": n}], "reward": {"Gold": gold} if gold else {}}


def conversation(id, title, lines, text, options):
    c = {"id": id, "title": title, "text": text, "options": options}
    if lines:
        c["presentation"] = "conversation"
        c["lines"] = [list(l) for l in lines]
    return {"choice": c}


def option(id, label, *then, detail=None, if_=None, requires=None, locked_text=None):
    """Conditions are trees (docs/story_conditions.md).
    if_:       offered only while the tree holds (hidden otherwise), e.g. {"item": "dog_tag"}.
    requires:  shown but locked until the tree holds; locked_text says why."""
    o = {"id": id, "label": label}
    if if_ is not None:
        o["if"] = if_
    if requires is not None:
        o["requires"] = requires
        if locked_text:
            o["locked_text"] = locked_text
    o["then"] = list(then)
    if detail:
        o["detail"] = detail
    return o


def chose(choice_id, answer):
    """The condition 'that choice was answered so': every choice records choice.<id>."""
    return {"flag": f"choice.{choice_id}", "is": answer}


class Story:
    """start_companions: who rides with the hero on day 1. In a campaign run the company
    also rides in from the last map (CampaignState carries companions, with their levels),
    and the engine skips anyone already riding, so a companion who stays with the hero is
    listed on every map: a map started on its own then has him too."""
    def __init__(self, stem, start_companions, essential=()):
        """essential: companions who never leave over unpaid upkeep (they still sulk)."""
        self.stem = stem
        self.root = {"start_companions": start_companions, "quests": [], "triggers": []}
        if essential:
            self.root["essential_companions"] = list(essential)
        self.encounters = {"defaults": {"guard": dogs(10)}, "objects": {}, "pickups": {}}

    def quest(self, id, title, text, main=False):
        self.root["quests"].append({"id": id, "title": title, "text": text, "main": main})

    def trig(self, id, when, *actions, if_=None):
        """when: the event (and story order: after / unless / wait). if_: a condition tree
        that must hold when the event comes up (docs/story_conditions.md)."""
        t = {"id": id, "when": when}
        if if_ is not None:
            t["if"] = if_
        t["do"] = list(actions)
        self.root["triggers"].append(t)

    def leave_by(self, exit_q, after, choice_id, title, text, go_label, ending):
        """The way out: offered when the hero reaches the exit; 'Not yet' asks again next time."""
        opts = lambda: [option("go", go_label, {"ending": ending}), option("not_yet", "Not yet")]
        prev = None
        for i in range(4):
            when = {"event": "enter_q", "q_min": exit_q, "after": prev or after}
            tid = f"{choice_id}_{i + 1}"
            self.trig(tid, when, conversation(choice_id, title, [], text, opts()),
                      if_=chose(choice_id, "not_yet") if prev else None)
            prev = tid


# ── 1.0 The Last Hunt ────────────────────────────────────────────────────────
SWORD = "red_hand_sword"
HAS_SWORD = {"item": SWORD}
ALDREN_NOTE = "I leave the sword of our forefathers. Do not take it. Do not come after me. — A."


def gone_at_dawn(body, frame):
    return {"id": "gone_at_dawn", "heading": "Gone at Dawn", "next": "1.1", "body": body,
            "frames": [{"scene": "home", "text": frame}]}


def hunt():
    s = Story("ch1_1_0_hunt", ["aldren", "drest"], essential=["drest"])
    # A small household party and a thin purse: the Hunters' Lodge slingers (110 gold +
    # 1 wood each) are paid for from the side paths (Poachers' Fire, Cut Willow).
    s.root["start_army"] = [{"id": "woad_runner", "count": 10}, {"id": "cruth_slinger", "count": 4}]
    s.root["start_resources"] = {"Gold": 200}
    s.quest("hunt", "The Last Hunt", "Two packs of dogs have been killing the herds. They foam at the mouth and do not run from people. Hunt them down with Aldren.", main=True)
    s.quest("hall", "The Hall", "The hunt is over. End the day: your father is waiting in the hall.")
    stream = mad_dogs(8)
    stream["reward"]["items"] = ["dog_tag"]   # found on the spoils screen; it sets up 1.1 and 1.2
    thicket = mad_dogs(6, packs=2)              # one pack from the trees above, one from below
    thicket["layout"] = "corridor_ambush"     # the battle is fought in the tree corridor they wait in
    s.encounters["objects"] = {"Thicket Dogs": thicket, "Stream Dogs": stream}

    s.trig("start", {"event": "start"},
           {"quest": "hunt"}, {"reveal": [0, 0, 10]},
           say(("Herdsman", "They came right into the fold, my lord. They bit my boy."),
               ("Aldren", "Two packs before dark. Keep up, little brother."),
               ("Drest", "I taught you both the same lessons. Only one of you waits for the old man.")))
    s.trig("hunt_done", {"event": "cleared", "names": ["Thicket Dogs", "Stream Dogs"]},
           {"quest_done": "hunt"}, {"quest": "hall"},
           say(("Aldren", "Dogs don't fight like that. Something is wrong with them."),
               ("Drest", "Burn the carcasses. Don't let the herdsmen touch them.")))
    # Aldren's habits, for 1.3 to pay off (the A on the Bridge Post, the banked Cold Fire).
    s.trig("carved_a", {"event": "encounter_won", "name": "Thicket Dogs"},
           {"scene": {"frames": [{"scene": "road", "text": "Aldren cuts an A into the bark with his knife."}]}},
           say(("Aldren", "So they know who cleared this wood.")),
           if_={"companion": "aldren"})
    # The Poachers' Fire stays a pickup (its gold and wood pay for the Lodge's slingers);
    # walking onto a pickup fires "visit" like any site.
    s.trig("banked_fire", {"event": "visit", "name": "Poachers' Fire"},
           {"scene": {"frames": [{"scene": "road", "text": "Aldren kneels and banks the old fire:\nash over the coals, stones round the edge."}]}},
           say(("Drest", "Every fire, every time. The rain would do it for you."),
               ("Aldren", "You taught me.")),
           if_={"companion": "aldren"})
    # The toast is told, not chosen: the choice of the night is the sword at dawn.
    s.trig("toast", {"event": "night", "after": "hunt_done"},
           {"quest_done": "hall"},
           {"scene": {"frames": [{"scene": "home", "text": "That night, in the hall,\nyour father stands and lifts his cup."}]}},
           say(("Your father", "To Aldren. To my heir.")),
           {"scene": {"frames": [{"scene": "home", "text": "Every cup in the hall goes up.\nAldren drinks, and does not look at you."}]}},
           {"event": "dawn"})
    # The chapter's first real choice. Following him breaks his second order, always (that
    # is the plot); his first order is the one the player may keep. The red hand on the
    # hilt is seen on both branches, in the frame.
    s.trig("dawn", {"event": "dawn"},
           {"leave": "aldren"},
           {"xp": 60},
           {"scene": {"frames": [{"scene": "home", "text": "First light. Aldren's horse is gone from the stable."},
                                 {"scene": "home", "text": "In the hall the sword of your forefathers hangs on its pegs.\n"
                                                           "A red hand on the hilt; a note pinned beneath."}]}},
           conversation("sword", "The Sword of Your Forefathers", [], f"“{ALDREN_NOTE}”",
                        [option("take", "Take the sword", {"item": "red_hand_sword"},
                                lore("Aldren's letter", f"“{ALDREN_NOTE}”"),
                                {"ending": gone_at_dawn("Aldren rode out at first light. You carry the sword he left on the wall.",
                                                        "You lift the sword from its pegs.\nThen you go after him.")},
                                detail="He asked you not to."),
                         option("leave", "Leave it on the wall",
                                lore("Aldren's letter", f"“{ALDREN_NOTE}”"),
                                {"ending": gone_at_dawn("Aldren rode out at first light. His sword stays on the wall.",
                                                        "You leave the sword on its pegs.\nThen you go after him.")})]))
    return s


# ── 1.1 Varenhold ────────────────────────────────────────────────────────────
PACKS = ["Orchard Dogs", "Ford Dogs", "Hollow Dogs"]
KEEPER = "Bridge Keeper"


def varenhold(land):
    s = Story("ch1_1_1_varenhold", ["drest"], essential=["drest"])
    s.quest("aldren", "Where did Aldren go?", "Aldren rode out at first light and left his sword on the wall. The only road out of the vale runs east, over the bridge.", main=True)
    s.quest("dogs", "Rabid Dogs in the Vale", "More of the mad dogs: three packs loose in the vale, and their bite carries the sickness. Kill them before they get at the herds.")
    s.encounters["objects"] = {"Orchard Dogs": dogs(12, 150), "Ford Dogs": dogs(14, 150),
                               "Hollow Dogs": dogs(18, 200)}
    s.encounters["pickups"] = {"Shepherd's Purse": {"reward": {"Gold": 300}}}

    s.trig("start", {"event": "start"}, {"quest": "aldren"}, {"quest": "dogs"}, {"reveal": [2, 0, 13]},
           {"if": HAS_SWORD, "then": [{"quest_text": ["aldren", "Aldren rode out at first light. You carry the sword he left on the wall. "
                                                                "The only road out of the vale runs east, over the bridge."]}]},
           say(("Drest", "He never waited for anyone. Not even me.")))
    # The hooded man stands with the third pack. Both answers end in the fight.
    s.trig("stranger", {"event": "engage", "name": "Hollow Dogs"},
           conversation("stranger", "The Hooded Stranger", [("Hooded Man", "Son of Varen.")],
                        "A hooded man stands among the dogs.",
                        [option("hear", "Hear him out",
                                say(("Hooded Man", "Your brother would not listen either. Keep this warm.")),
                                {"item": "hermits_seed"},
                                # He knows the sword. Today the quest text names the road; once the
                                # three roads out of 1.1 exist, this is the one that picks the road.
                                {"if": HAS_SWORD, "then": [
                                    say(("Hooded Man", "That hand went east before you. Follow it.")),
                                    {"quest_text": ["aldren", "The hooded man knew your sword: it went east before you. "
                                                              "The only road east runs over the bridge."]}]}),
                         option("attack", "Attack")]))
    s.trig("stranger_gone", {"event": "encounter_won", "name": "Hollow Dogs"},
           {"vanish": "Hooded Stranger"},
           say(("Drest", "He's gone. Only roots, pushed up through frozen ground. I've never seen that.")))
    s.trig("rune_stone", {"event": "visit", "name": "Vale Rune Stone"},
           lore("A rubbing of the rune stone", "Charcoal on cloth. No one at Varenhold can read it. The marks are like the ones on the dog's tag."))
    s.trig("barrow", {"event": "visit", "name": "Varen Barrow"},
           {"if": HAS_SWORD,
            "then": [say(("Drest", "A red hand is cut into the door. Like the one on your sword."))],
            "else": [say(("Drest", "A red hand is cut into the door. Like the one on the sword Aldren left."))]})
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


# ── 1.2 The Camps ────────────────────────────────────────────────────────────
CAMPS = [  # (choice id, object, speaker, line, toll, what turning them away means)
    ("camp_riverside", "Riverside Camp", "Captain",
     "Every house that can lift a spear is marching for Umm'Natur. One night on your fields, my lord.",
     400, "fight"),
    ("camp_meadow", "Meadow Camp", "Captain",
     "We are a small house and a long way from home. We will be gone by morning.",
     300, "leave"),
    ("camp_strangers", "Strangers' Camp", "Ushari",
     "I am Ushari. A man with a red hand on his shield rode past us going east.",
     150, "leave"),
]
# Ushari's camp: she has seen Aldren (the red hand on his shield) ride east, so she rides
# with you whatever you answer. Her three go on ahead (and are broken on the east road, 1.3).
# Your answer sets the terms, and she remembers them at the Broken Camp. If you took the
# sword, she sees the same hand on it.
USHARI_SEEN = CAMPS[2][3]
USHARI_SEEN_SWORD = USHARI_SEEN + " The same hand is on your sword."
DREST_SHIELD = ("Drest", "Aldren's shield.")
STRANGERS_TEXT = "Four riders camp by the burn, and offer to pay their way."
# If the player never rode to her camp, she rides up at the Red Banner bridge instead
# (the only way east, so it cannot be missed). Exactly one of the two join beats fires.
USHARI_BRIDGE_GO = ("Ushari", "My people have gone on ahead. I ride with you.")
USHARI_TERMS = {
    "grant": "Then I owe you. The others go on east in the morning. I ride with you.",
    "tax": "Paid, then. The others go on ahead. I ride with you, and owe you nothing.",
    "turn_away": "Then the others go tonight. You are going where he went, and so am I.",
}
# The dogs' owner (the Ice people's first meeting; unnamed). He speaks only his own
# tongue, never translated: "Drusk" is the dog's name. Fighting him pays; peace costs that.
WARRIOR = "Northern Warrior"
# Three stacks (he and his kin): one slow stack dies before it acts; three hold ground and
# cost a level-3 army about what the Field Brigands do (battle_sim), for more experience.
WARRIOR_BAND = {"guards": [{"id": "northern_warrior", "count": 3}, {"id": "northern_warrior", "count": 3},
                           {"id": "northern_warrior", "count": 2}], "reward": {"Gold": 300}}
RIVERSIDE_BAND = [{"id": "woad_runner", "count": 12}, {"id": "cruth_slinger", "count": 4}]
RED_BAND = [{"id": "woad_runner", "count": 16}, {"id": "cruth_slinger", "count": 6}, {"id": "painted_blade", "count": 3}]
CAMP_XP = 35
# The hunters the sword draws (taking it is a gamble: they pay experience and spoils).
# Working names; the user names them. 1.2: a band rides out from a camp behind you
# (betray). The engine pays a broken war-band its experience but no purse, so the
# story gives one. 1.3: a band waits at a choke on the east road (a guard camp).
HUNT_SITE = "Riders' Camp"
HUNT_BAND = "Sword Hunters"
HUNTERS_12 = [{"id": "gododdin_rider", "count": 3}, {"id": "cruth_slinger", "count": 6}, {"id": "woad_runner", "count": 10}]
HUNTERS_12_PURSE = {"Gold": 400}
HUNTERS_13 = "Sword Hunters"
HUNTERS_13_GUARD = {"guards": [{"id": "gododdin_rider", "count": 4}, {"id": "painted_blade", "count": 4},
                               {"id": "cruth_slinger", "count": 6}], "reward": {"Gold": 500}}


def camps(land):
    s = Story("ch1_1_2_camps", ["drest"], essential=["drest", "ushari"])
    s.quest("aldren", "Where did Aldren go?", "Aldren's road runs east across the river fields. Follow it.", main=True)
    s.quest("camps", "Camps on Your Fields",
            "Companies from other houses have camped on your fields on their way east. Ride to each camp and decide: let them stay, take a toll, or turn them away.")
    s.quest("hunted", "Riders on Your Trail", "Riders have followed you out of the vale. Drest thinks they are after the sword.")
    s.encounters["objects"] = {"Copse Dogs": dogs(12, 200),
                               "Field Brigands": {"guards": [{"id": "brigand", "count": 12}], "reward": {"Gold": 300}},
                               WARRIOR: WARRIOR_BAND}
    s.encounters["pickups"] = {"Brigand Purse": {"reward": {"Gold": 500}}}

    s.trig("start", {"event": "start"}, {"quest": "aldren"}, {"quest": "camps"}, {"reveal": [-4, 2, 9]})
    # Where the dogs came from: northern settlers on the road south (not named yet; the
    # user names them). Only what is seen; no faction name.
    s.trig("empty_camp", {"event": "visit", "name": "Empty Camp"},
           lore("An empty camp", "Dog-pens of split pine, their gates hanging open, and sled tracks coming down out of the north. "
                "A cut leash lies in the mud, on a studded collar no one in the Marches makes. "
                "Snagged on a gatepost: a scrap of white fur, and a few strands of pale hair."),
           say(("Drest", "Read the ground, boy. Someone came down the burn after those dogs, and not long ago.")))
    # The dogs' owner, looking for them in the snow above the camp. The tag from 1.0
    # ends it without a fight; withdraw means no battle, no experience, no spoils.
    s.trig("warrior", {"event": "engage", "name": WARRIOR},
           conversation("warrior", "A Man in the Snow", [(WARRIOR, "Drusk! Hrakh, Drusk!")],
                        "A big man in white furs stands in the snow above the burn, the end of a cut leash in his fist. "
                        "Further up the burn, others in white furs wait. "
                        "He whistles, waits, and whistles again. Then he sees you, and his hand goes to his axe.",
                        [option("give_tag", "Hand him the dog tag",
                                {"take_item": "dog_tag"},
                                say((WARRIOR, "Drusk... Grath vorrn uthk."),
                                    ("Drest", "Well done. Your brother would have reached for his sword.")),
                                {"scene": {"frames": [{"scene": "dog_tag", "text": "He closes his fist on the tag\nand says the dog's name once more, softly."},
                                                      {"scene": "dog_tag", "text": "Then he looks north, and goes."}]}},
                                {"withdraw": WARRIOR}, if_={"item": "dog_tag"}),
                         option("fight", "Fight", detail="His kin will come down the burn with him.")]))
    s.trig("warrior_fought", {"event": "encounter_won", "name": WARRIOR},
           say(("Drest", "Northmen, this far south. Not in my lifetime.")))
    for cid, name, speaker, line, toll, refused in CAMPS:
        gone = {"vanish": name}
        turn = ([{"betray": {"at": name, "band": "Riverside Company", "army": RIVERSIDE_BAND}}, gone]
                if refused == "fight" else [gone])
        joins = {a: [] for a in ("grant", "tax", "turn_away")}
        if cid == "camp_strangers":               # Ushari joins on every answer; the terms differ
            joins = {a: [say(("Ushari", reply)), {"join": "ushari"}] for a, reply in USHARI_TERMS.items()}

        def ask(lines, text=""):
            return conversation(cid, name, lines, text,
                                [option("grant", "Let them stay", gone, {"xp": CAMP_XP}, {"event": "camp_decided"},
                                        *joins["grant"]),
                                 option("tax", "Take a toll", {"give": {"Gold": toll}}, gone, {"xp": CAMP_XP},
                                        {"event": "camp_decided"}, *joins["tax"]),
                                 option("turn_away", "Turn them away", *turn, {"xp": CAMP_XP}, {"event": "camp_decided"},
                                        *joins["turn_away"],
                                        detail="They will not go quietly." if refused == "fight" else None)])
        if cid != "camp_strangers":
            s.trig(cid, {"event": "visit", "name": name}, ask([(speaker, line)]))
            continue
        # Her bridge beat comes first: then the camp is gone. Three lines either way.
        s.trig("ushari_bridge", {"event": "visit", "name": "Red Banner Camp"},
               {"scene": {"frames": [{"scene": "road", "text": "As you near the bridge, a rider comes up the road behind you.\n"
                                                                "A woman, riding alone."}]}},
               {"if": HAS_SWORD,
                "then": [say(("Ushari", USHARI_SEEN_SWORD), DREST_SHIELD, USHARI_BRIDGE_GO)],
                "else": [say(("Ushari", USHARI_SEEN), DREST_SHIELD, USHARI_BRIDGE_GO)]},
               {"join": "ushari"}, {"vanish": name},
               if_={"all": [{"not": {"companion": "ushari"}}, {"not": {"trigger_fired": cid}}]})
        # Two lines before the answer, her terms after it.
        s.trig(cid, {"event": "visit", "name": name},
               {"if": HAS_SWORD,
                "then": [ask([("Ushari", USHARI_SEEN_SWORD), DREST_SHIELD], STRANGERS_TEXT)],
                "else": [ask([("Ushari", USHARI_SEEN), DREST_SHIELD], STRANGERS_TEXT)]},
               if_={"not": {"trigger_fired": "ushari_bridge"}})
    # The sword draws hunters. Taken: a band rides out from a camp behind you (it hunts the
    # hero once it is half again the army's strength, else it raids your mills; see
    # HUNTERS_12). Left on the wall: the camp is gone before the map starts.
    s.trig("hunters", {"event": "start"},
           {"if": HAS_SWORD,
            "then": [{"betray": {"at": HUNT_SITE, "band": HUNT_BAND, "army": HUNTERS_12, "goal": "hunt"}}, {"vanish": HUNT_SITE},
                     {"quest": "hunted"},
                     say(("Drest", "Riders on our trail since the vale. Keep that sword close, boy."))],
            "else": [{"vanish": HUNT_SITE}]})
    s.trig("hunters_broken", {"event": "rival_beaten", "name": HUNT_BAND},
           {"quest_done": "hunted"}, {"give": HUNTERS_12_PURSE},
           say(("Drest", "They knew what they came for. There will be more.")))
    # The first heavy decision. The camp holds the only east bank of the burn, at the bridge.
    # Drest sees the red hand once (the emperor's mark; never said). With the sword, the
    # captain wants it, and it buys the bridge outright.
    red_lines = [("Captain", "We hold your bridge tonight, and we take what we need from your fields. You may watch.")]

    def red_banner(lines, hand_over=()):
        return conversation("red_banner", "The Red Banner", lines,
                            "A red hand is painted on their banner, and on every shield.",
                            [option("let_pass", "Let them pass", {"vanish": "Red Banner Camp"}, {"xp": CAMP_XP},
                                    {"event": "camp_decided"}),
                             option("make_pay", "Make them pay", {"give": {"Gold": 600}}, {"vanish": "Red Banner Camp"},
                                    {"xp": CAMP_XP}, {"event": "camp_decided"}),
                             *hand_over,
                             option("drive_off", "Drive them off",
                                    {"betray": {"at": "Red Banner Camp", "band": "Red Banner Company", "army": RED_BAND}},
                                    {"vanish": "Red Banner Camp"}, {"xp": CAMP_XP}, {"event": "camp_decided"},
                                    detail="They will fight for the bridge.")])
    s.trig("red_banner", {"event": "visit", "name": "Red Banner Camp"},
           {"if": HAS_SWORD,
            "then": [red_banner(red_lines + [("Drest", "Look at their shields, boy. That is the hand on your sword."),
                                             ("Captain", "That hand is not yours to carry.")],
                                [option("hand_over", "Hand over the sword", {"take_item": SWORD},
                                        {"disband": HUNT_BAND}, {"quest_done": "hunted"},
                                        {"vanish": "Red Banner Camp"}, {"xp": CAMP_XP}, {"event": "camp_decided"},
                                        detail="They give up the bridge for it.", if_=HAS_SWORD)])],
            "else": [red_banner(red_lines + [("Drest", "Look at their shields, boy. That is the hand on the sword Aldren left.")])]})
    s.trig("red_banner_broken", {"event": "rival_beaten", "name": "Red Banner Company"},
           {"flag": {"red_banner_broken": True}})
    s.trig("riverside_broken", {"event": "rival_beaten", "name": "Riverside Company"},
           {"flag": {"riverside_broken": True}})
    s.trig("camps_done", {"event": "camp_decided"},
           {"quest_done": "camps"},
           if_={"all": [{"flag": f"choice.{c[0]}"} for c in CAMPS if c[0] != "camp_strangers"]
                + [{"any": [{"flag": "choice.camp_strangers"}, {"trigger_fired": "ushari_bridge"}]},
                   {"flag": "choice.red_banner"}]})
    # At the road out, with the sword: Ushari tells where Aldren was going. 1.3 then starts
    # with the mine and his cold fire on the map. (Handed over at the bridge: she keeps it.)
    s.trig("ushari_roots", {"event": "enter_q", "q_min": land.exit_q, "after": "red_banner"},
           say(("Ushari", "He asked me the way to the roots.")), {"flag": {"ushari_roots": True}},
           if_={"all": [{"companion": "ushari"}, HAS_SWORD]})
    s.leave_by(land.exit_q, "red_banner", "east_road", "The East Road",
               "The road climbs east toward the hills.", "Take the east road",
               {"id": "the_east_road", "heading": "The East Road", "next": "1.3",
                "body": "The camps are behind you. The road climbs east toward the hills.",
                "frames": [{"scene": "road", "text": "The river fields fall behind.\nThe road climbs."}]})
    return s


# ── 1.3 The East Road ────────────────────────────────────────────────────────
def east_road(land):
    s = Story("ch1_1_3_east_road", ["drest"], essential=["drest", "ushari"])
    s.quest("aldren", "Where did Aldren go?", "The east road climbs toward the mountains. Follow it to its end.", main=True)
    s.encounters["objects"] = {
        "Road Dogs": dogs(16, 200),
        "Ridge Bears": {"guards": [{"id": "highland_bear", "count": 4}], "reward": {"Gold": 300}},
        "Stone Trolls": {"guards": [{"id": "forest_troll", "count": 2}], "reward": {"Gold": 400}},
        "Root Wardens": {"guards": [{"id": "ancient_treant", "count": 1}]},
        HUNTERS_13: HUNTERS_13_GUARD,
    }
    s.encounters["pickups"] = {"Fallen Pine": {"reward": {"Wood": 4}}, "Scree Stone": {"reward": {"Stone": 4}}}

    mine, fire = (next((o["q"], o["r"]) for o in land.objects if o["name"] == n) for n in ("Root-Grown Mine", "Cold Fire"))
    s.trig("start", {"event": "start"}, {"quest": "aldren"},
           # Ushari told you (1.2, sword carried): the mine and his camp are on the map from the start.
           {"if": {"flag": "ushari_roots"},
            "then": [{"reveal": [*mine, 2]}, {"reveal": [*fire, 2]},
                     {"quest_text": ["aldren", "Ushari showed Aldren the way to the roots: an old mine where this road ends, "
                                               "under the mountains. Follow it."]},
                     say(("Ushari", "The roots are where this road ends. He would have slept in the dell above it."))]},
           # The hunters wait only for the sword.
           {"if": {"not": HAS_SWORD}, "then": [{"vanish": HUNTERS_13}],
            "else": [say(("Drest", "Riders hold the bridge ahead, boy. They want that sword."))]})
    s.trig("hunters_wait", {"event": "engage", "name": HUNTERS_13},
           say(("Drest", "They were waiting for us, boy. For that sword.")))
    # Aldren's traces, off the road, easy to miss. No one explains them.
    s.trig("cold_fire", {"event": "visit", "name": "Cold Fire"},
           lore("A cold fire", "Ashes banked the careful way Aldren always banked them. Grass flattened for one bedroll."),
           say(("Drest", "I taught him to bank a fire like that. He was listening, then.")))
    s.trig("bridge_post", {"event": "visit", "name": "Bridge Post"},
           lore("The bridge post", "Cut into the post with a knife: an arrow pointing east, and an A, in the hand of his letter."))
    # The Broken Camp is Ushari's company, gone on ahead of her from 1.2 and broken by the
    # red hands. She sees it with you: first the camp (scenes play before lines), then her
    # answer to what you did at the bridge, then to the terms you gave her at her camp.
    camp = [{"scene": {"frames": [
        {"scene": "road", "text": "Ushari's people camped here.\nTheir tents are cut down, their packs slit open."},
        {"scene": "road", "text": "No one is left.\nOn the cart, someone has painted a red hand."}]}}]
    bridge = [(chose("red_banner", a), line) for a, line in [
        ("drive_off", "The red hands. You drove them from your bridge, and they came this way."),
        ("let_pass", "The red hands. You let them over your bridge, and they came this way."),
        ("make_pay", "The red hands. They paid you for your bridge, and took it back from my people.")]]
    terms = [(chose("camp_strangers", a), line) for a, line in [
        ("grant", "You let us rest on your fields. I should have kept them there one more day."),
        ("tax", "We paid your toll for a safe road. This is your road."),
        ("turn_away", "You turned us off your fields. They had nowhere to wait for me.")]]

    def first_of(cases, otherwise):
        """One branch per case, the first that holds; otherwise when none does."""
        out = otherwise
        for cond, line in reversed(cases):
            out = [{"if": cond, "then": [say(("Ushari", line))], "else": out}]
        return out
    s.trig("broken_camp", {"event": "visit", "name": "Broken Camp"},
           *camp, *first_of(bridge, [say(("Ushari", "Red hands. They came in the night."))]),
           # She joined at the bridge and never camped on your fields: no terms to remember.
           *first_of(terms, [say(("Ushari", "I sent them on ahead. I should have ridden with them."))]),
           if_={"all": [{"companion": "ushari"}, {"not": {"trigger_fired": "ushari_road"}}]})
    # In a campaign run she always rides in from 1.2 (her camp, or the bridge). Only a map
    # started on its own has no 1.2 behind it: she is found here, the last one standing.
    # choice.red_banner is set on every campaign run (1.2's exit waits for that choice),
    # so this never fires in one, even if she has since left over pay or fallen.
    s.trig("ushari_road", {"event": "visit", "name": "Broken Camp"},
           conversation("ushari_road", "The Broken Camp",
                        [("Ushari", "I am Ushari. Red hands came in the night. I am the last of us.")],
                        "A camp lies broken by the road: tents cut down, packs slit open, a red hand painted on the cart. "
                        "One woman is still standing.",
                        [option("take", "Take her with you", {"join": "ushari"}),
                         option("send_home", "Send her home")]),
           if_={"all": [{"not": {"companion": "ushari"}}, {"not": {"trigger_fired": "broken_camp"}},
                        {"not": {"flag": "choice.red_banner"}}]})
    s.trig("wardens_won", {"event": "encounter_won", "name": "Root Wardens"})
    # The last decision of the chapter; "Turn back" asks again on the next visit.
    prev = None
    for i in range(4):
        when = {"event": "visit", "name": "Root-Grown Mine", "after": prev or "wardens_won"}
        tid = f"roots_{i + 1}"
        s.trig(tid, when, conversation(
            "roots", "The Roots", [],
            "Roots have grown across the mine mouth. Someone cut through them not long ago, and they have closed again.",
            [option("cut", "Cut the roots", {"ending": {
                "id": "the_roots", "heading": "The Roots",
                "body": "You cut through the roots and go down into the dark after him.",
                "frames": [{"scene": "road", "text": "The roots part under the axe.\nCold air rises out of the dark."},
                           {"scene": "road", "text": "Aldren went this way.\nSo do you."}]}}),
             option("turn_back", "Turn back")]),
            if_=chose("roots", "turn_back") if prev else None)
        prev = tid
    return s


# ── campaign ─────────────────────────────────────────────────────────────────
TITLES = {"ch1_1_0_hunt": ("1.0", "The Last Hunt", 2), "ch1_1_1_varenhold": ("1.1", "Varenhold", 3),
          "ch1_1_2_camps": ("1.2", "The Camps", 4), "ch1_1_3_east_road": ("1.3", "The East Road", 5)}


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


def most(actions, what):
    """The most dialogue lines ("lines") or story experience ("xp") one way through an
    action list can give: each choice adds its longest answer, each branch its longer side."""
    total = 0
    for a in actions:
        if what == "lines":
            total += len(a.get("say", [])) + len(a.get("choice", {}).get("lines", []))
        else:
            total += a.get("xp", 0)
        total += max([most(o.get("then", []), what) for o in a.get("choice", {}).get("options", [])] or [0])
        if "if" in a:
            total += max(most(a.get("then", []), what), most(a.get("else", []), what))
    return total


def walk_actions(actions):
    for a in actions:
        yield a
        for o in a.get("choice", {}).get("options", []):
            yield from walk_actions(o.get("then", []))
        if "if" in a:                                   # a branch: {"if", "then", "else"}
            yield from walk_actions(a.get("then", []))
            yield from walk_actions(a.get("else", []))


# The leaves of docs/story_conditions.md: the keys each may carry beside its own.
COND_LEAVES = {"flag": {"is"}, "item": set(), "companion": {"present"}, "quest": {"state"},
               "cleared": set(), "lore": set(), "trigger_fired": {"days"}, "level": set(),
               "day": set(), "resource": {">=", ">", "<=", "<", "=="}}
COND_OPS = {">=", ">", "<=", "<", "=="}
RESOURCES = {"Gold", "Wood", "Stone", "Obsidian", "Crystal"}   # src/world/Resources.h
# Single-purpose fields the trees replace; chapter 1 writes "if" / "requires" instead.
LEGACY_WHEN = ("flag", "unless_flag", "delay")
LEGACY_OPTION = ("requires_item", "requires_flag", "requires_lore", "requires_route")


def check_cond(c, where, known):
    """Problems with one condition tree. known: what the names in its leaves may be
    (items, companions, quests, cleared, triggers, lore, choices {id: option ids}, flags)."""
    if isinstance(c, bool):
        return []
    if not isinstance(c, dict) or not c:
        return [f"{where}: a condition is an object (or true / false)"]
    for op in ("all", "any"):
        if op in c:
            if len(c) != 1:
                return [f"{where}: '{op}' stands alone"]
            if not isinstance(c[op], list):
                return [f"{where}.{op}: wants a list"]
            return [p for i, sub in enumerate(c[op]) for p in check_cond(sub, f"{where}.{op}[{i}]", known)]
    if "not" in c:
        return [f"{where}: 'not' stands alone"] if len(c) != 1 else check_cond(c["not"], f"{where}.not", known)
    leaf = next((k for k in COND_LEAVES if k in c), None)
    if leaf is None:
        return [f"{where}: unknown condition '{next(iter(c))}'"]
    extra = set(c) - {leaf} - COND_LEAVES[leaf]
    if extra:
        return [f"{where}: unknown key '{sorted(extra)[0]}' beside '{leaf}'"]
    v = c[leaf]

    def cmp_ok(x):
        return isinstance(x, dict) and len(x) == 1 and next(iter(x)) in COND_OPS \
            and isinstance(next(iter(x.values())), int) and not isinstance(next(iter(x.values())), bool)
    if leaf in ("level", "day"):
        return [] if cmp_ok(v) else [f"{where}.{leaf}: wants exactly one of >=, >, <=, <, == with an integer"]
    if not isinstance(v, str) or not v:
        return [f"{where}.{leaf}: wants a name"]
    if leaf == "resource":
        out = [] if v in RESOURCES else [f"{where}.resource: unknown resource '{v}'"]
        return out + ([] if cmp_ok({k: x for k, x in c.items() if k != "resource"})
                      else [f"{where}: wants exactly one comparison with an integer"])
    if leaf == "flag":
        if v.startswith("choice."):
            cid = v[len("choice."):]
            if cid not in known["choices"]:
                return [f"{where}: no choice '{cid}' is asked on this map or before it"]
            if "is" in c and c["is"] not in known["choices"][cid]:
                return [f"{where}: choice '{cid}' has no answer '{c['is']}' (it has {sorted(known['choices'][cid])})"]
        elif v not in known["flags"]:
            return [f"{where}: flag '{v}' is never set by a flag action on this map or before it"]
        return []
    if leaf == "item" and v not in known["items"]:
        return [f"{where}: item '{v}' is not in items.json"]
    if leaf == "companion":
        if v not in known["companions"]:
            return [f"{where}: companion '{v}' is not a companion in units.json"]
        if "present" in c and not isinstance(c["present"], bool):
            return [f"{where}.present: wants true or false"]
    if leaf == "quest":
        if v not in known["quests"]:
            return [f"{where}: quest '{v}' is not defined on this map"]
        if c.get("state") not in ("active", "done"):
            return [f"{where}.state: wants 'active' or 'done'"]
    if leaf == "cleared" and v not in known["cleared"]:
        return [f"{where}: '{v}' is not a guard on this map"]
    if leaf == "lore" and v not in known["lore"]:
        return [f"{where}: no lore '{v}' is given on this map or before it"]
    if leaf == "trigger_fired":
        if v not in known["triggers"]:
            return [f"{where}: no trigger '{v}' on this map"]
        if "days" in c and not cmp_ok(c["days"]):
            return [f"{where}.days: wants exactly one of >=, >, <=, <, == with an integer"]
    return []


def check_conditions(s, known):
    """Every 'if' / 'requires' tree in a story, and no single-purpose fields left over."""
    problems = []
    for i, t in enumerate(s.root["triggers"]):
        at = f"{s.stem}: triggers[{i}] '{t['id']}'"
        for key in LEGACY_WHEN:
            if key in t["when"]:
                problems.append(f"{at}: when.{key} is the old spelling; write \"if\"")
        if "if" in t:
            problems += check_cond(t["if"], f"{at}.if", known)
        for a in walk_actions(t["do"]):
            if "if" in a:
                if set(a) - {"if", "then", "else"}:
                    problems.append(f"{at}: a branch carries keys besides if / then / else")
                problems += check_cond(a["if"], f"{at} branch.if", known)
            for o in a.get("choice", {}).get("options", []):
                oat = f"{at} option '{o['id']}'"
                for key in LEGACY_OPTION:
                    if key in o:
                        problems.append(f"{oat}: {key} is the old spelling; write \"if\" or \"requires\"")
                for key in ("if", "requires"):
                    if key in o:
                        problems += check_cond(o[key], f"{oat}.{key}", known)
                if "locked_text" in o and "requires" not in o:
                    problems.append(f"{oat}: locked_text without requires")
    return problems


def combat_layout_ids():
    """The battle layout ids data/combat_layouts.json defines, or None before it exists.
    Accepts {"layouts": [{"id": ...}]}, {"layouts": {id: ...}} or {id: ...}."""
    path = ROOT / "data/combat_layouts.json"
    if not path.exists():
        return None
    doc = json.loads(path.read_text())
    body = doc.get("layouts", doc) if isinstance(doc, dict) else doc
    if isinstance(body, list):
        return {e["id"] for e in body if isinstance(e, dict) and "id" in e}
    return {k for k in body if not k.startswith("_")}


def check(stories, lands, out, data_out):
    units = {u["id"]: u for u in json.loads((ROOT / "data/units.json").read_text())["units"]}
    items = {i["id"] for i in json.loads((ROOT / "data/items.json").read_text())["items"]}
    problems, report = [], []
    layouts = combat_layout_ids()
    camp = json.loads((data_out / "campaign.json").read_text())
    for entry in camp["chapters"][0]["maps"]:
        for key in ("map", "encounters", "triggers"):
            if not (data_out / entry[key]).exists():
                problems.append(f"campaign.json: {entry['id']} {key} {entry[key]} does not exist")
    choice_ids = {}
    companions = {u["id"] for u in units.values() if u.get("faction") == "companion"}
    # What earlier maps leave behind for the conditions of later ones: choices, flags, lore.
    so_far = {"choices": {}, "flags": set(), "lore": set()}
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
                for key in ("item", "take_item"):
                    if key in a and a[key] not in items:
                        problems.append(f"{s.stem}: {key} {a[key]} is not in items.json")
                if "withdraw" in a:
                    named.add(a["withdraw"])
                    if t["when"].get("event") != "engage":
                        problems.append(f"{s.stem}: trigger {t['id']} withdraws {a['withdraw']} outside an engage")
                if "join" in a and a["join"] not in units:
                    problems.append(f"{s.stem}: companion {a['join']} is not in units.json")
                if "choice" in a:
                    choice_ids.setdefault(a["choice"]["id"], set()).add(s.stem)
        for t in s.root["triggers"]:
            for a in walk_actions(t["do"]):
                if "choice" in a:
                    so_far["choices"].setdefault(a["choice"]["id"], set()).update(
                        o["id"] for o in a["choice"]["options"])
                if isinstance(a.get("flag"), dict):
                    so_far["flags"].update(a["flag"])
                if "lore" in a:
                    so_far["lore"].add(a["lore"][0])
        known = dict(so_far, items=items, companions=companions,
                     quests={q["id"] for q in s.root["quests"]},
                     cleared=guards | {n for n in s.encounters["objects"] if n in objs},
                     triggers={t["id"] for t in s.root["triggers"]})
        problems += check_conditions(s, known)
        for n, e in s.encounters["objects"].items():
            for i in e.get("reward", {}).get("items", []):
                if i not in items:
                    problems.append(f"{s.stem}: {n} rewards item {i}, not in items.json")
        # A battle layout ("layout": id) must be a string, and one data/combat_layouts.json
        # defines once that file exists (the engine reads the key per battle).
        for n, e in s.encounters["objects"].items():
            if "layout" in e:
                if not isinstance(e["layout"], str) or not e["layout"]:
                    problems.append(f"{s.stem}: {n} layout must be a layout id")
                elif layouts is not None and e["layout"] not in layouts:
                    problems.append(f"{s.stem}: {n} layout '{e['layout']}' is not in combat_layouts.json")
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
        # Three lines a beat at most: the longest way through it (lines before a choice
        # plus the longest answer; the longer side of every branch).
        for t in s.root["triggers"]:
            n = most(t["do"], "lines")
            if n > 3:
                problems.append(f"{s.stem}: trigger {t['id']} has {n} lines")
        # Experience: what the guards and the story pay against the level cap.
        guard_xp = {n: max(20, int(power(units, e["guards"]) / 3)) for n, e in s.encounters["objects"].items()}
        story_xp = sum(most(t["do"], "xp") for t in s.root["triggers"])
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
    problems += check_tips(stories, units)
    return problems, report


TIP_WHEN = {"battle_start", "first_turn_shooter", "enemy_adjacent", "flanked", "low_stack",
            "defend_available", "victory_first", "companion_hit", "rushes", "obstacle_broken"}
TIP_ACTOR = {"rushes", "obstacle_broken", "first_turn_shooter", "defend_available"}   # where "actor" applies
TIP_REPEAT = {"campaign", "battle"}


def check_tips(stories, units):
    """data/combat_tips.json: fixed 'when' keys, unique ids, and a speaker who rides on every listed map."""
    path = ROOT / "data/combat_tips.json"
    if not path.exists():
        return []
    tips = json.loads(path.read_text())
    problems, seen = [], set()
    if set(tips.get("when_keys", {})) != TIP_WHEN:
        problems.append("combat_tips.json: when_keys differ from the fixed set Godot implements")
    by_name = {u["name"]: u["id"] for u in units.values() if u.get("faction") == "companion"}
    party = {TITLES[s.stem][0]: set(s.root["start_companions"]) for s in stories}
    for tip in tips["tips"]:
        tid = tip.get("id")
        if tid in seen:
            problems.append(f"combat_tips.json: tip id {tid} is used twice")
        seen.add(tid)
        if tip.get("when") not in TIP_WHEN:
            problems.append(f"combat_tips.json: {tid} has unknown when '{tip.get('when')}'")
        speaker = by_name.get(tip.get("speaker"))
        if not speaker:
            problems.append(f"combat_tips.json: {tid} speaker {tip.get('speaker')} is not a companion")
        for m in tip.get("maps", []):
            if m not in party:
                problems.append(f"combat_tips.json: {tid} names map {m}, not in the campaign")
            elif speaker and speaker not in party[m]:
                problems.append(f"combat_tips.json: {tid} on map {m}, where {tip['speaker']} does not start in the party")
        if not tip.get("text") or any(ch.isdigit() for ch in tip["text"]):
            problems.append(f"combat_tips.json: {tid} text is empty or shows a number")
        if tip.get("repeat", "campaign") not in TIP_REPEAT:
            problems.append(f"combat_tips.json: {tid} repeat '{tip['repeat']}' is not one of {sorted(TIP_REPEAT)}")
        if "actor" in tip:
            if tip.get("when") not in TIP_ACTOR:
                problems.append(f"combat_tips.json: {tid} has an actor, which '{tip.get('when')}' does not read")
            if tip["actor"] not in units and tip["actor"] not in {u["name"] for u in units.values()}:
                problems.append(f"combat_tips.json: {tid} actor {tip['actor']} is not a unit id or name")
        if "kind" in tip:
            kinds = set(json.loads((ROOT / "data/combat_layouts.json").read_text()).get("obstacle_kinds", {})) \
                if (ROOT / "data/combat_layouts.json").exists() else set()
            if tip.get("when") != "obstacle_broken":
                problems.append(f"combat_tips.json: {tid} has a kind, which only obstacle_broken reads")
            elif kinds and tip["kind"] not in kinds:
                problems.append(f"combat_tips.json: {tid} kind '{tip['kind']}' is not an obstacle kind in combat_layouts.json")
        reply = tip.get("reply", [])
        if not isinstance(reply, list) or len(reply) > 2:
            problems.append(f"combat_tips.json: {tid} reply wants a list of at most two lines")
            reply = []
        for line in reply:
            who = by_name.get(line.get("speaker")) if isinstance(line, dict) else None
            if not who:
                problems.append(f"combat_tips.json: {tid} reply speaker {line.get('speaker') if isinstance(line, dict) else line} is not a companion")
                continue
            if not line.get("text") or any(ch.isdigit() for ch in line["text"]):
                problems.append(f"combat_tips.json: {tid} reply text is empty or shows a number")
            for m in tip.get("maps", []):
                if m in party and who not in party[m]:
                    problems.append(f"combat_tips.json: {tid} reply on map {m}, where {line['speaker']} does not start in the party")
    return problems


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
    n_conds = sum(("if" in t) + sum(("if" in o) + ("requires" in o) for a in walk_actions(t["do"])
                                     for o in a.get("choice", {}).get("options", [])) + sum("if" in a for a in walk_actions(t["do"]))
                  for s in stories for t in s.root["triggers"])
    print(f"Check passed: every named object exists, every site is reachable, chokes hold, campaign paths exist, "
          f"{n_conds} condition trees are well formed and name only things that exist.")


if __name__ == "__main__":
    main()
