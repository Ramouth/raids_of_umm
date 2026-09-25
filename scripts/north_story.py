#!/usr/bin/env python3
"""north_story.py — writes data/maps/old_passage.triggers.json (stage 1 story).

Kept as a script so the beats read top to bottom like the story doc.
Speakers with portraits: Ushari, Kharim, Aldren, Corvin, Inscription, Hooded Druid.

Level 1 opens without Ushari: the Steward sends you after the vale's wolves,
a hooded druid (Hul'rik; the player does not learn his name yet) stands with
the last pack, and Ushari rides in once every pack is dead. Any beat she
speaks in waits for her arrival (see trig()).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "data" / "maps" / "old_passage.triggers.json"

def say(*lines):
    return {"say": [list(l) for l in lines]}

def lore(title, text):
    return {"lore": [title, text]}

HALE_HOUSEHOLD = [{"id": "levy_spearman", "count": 24}, {"id": "desert_archer", "count": 10},
                  {"id": "armoured_warrior", "count": 4}]

VALE_WOLVES = ["Hill Wolves", "Den Wolves", "Hermit's Wolves"]

quests = [
    {"id": "wolves", "title": "Wolves in the Vale",
     "text": "A guest of the Compact is on the road to Varenhold, but no escort will cross the vale while the "
             "wolves hunt it. Clear the Hill Wolves, the Den Wolves and the Hermit's Wolves."},
    {"id": "main", "title": "The Old Passage", "main": True,
     "text": "Somewhere under the Greyfang mountains runs the old passage: a road older than the families, "
             "leading south to the lost desert of Umm'Natur. Aldren rode ahead to find it. Follow him."},
    {"id": "aldren", "title": "Where Is Aldren?",
     "text": "Your brother Aldren left Varenhold three days ago with a letter from the scholar Kharim. "
             "He has not written since."},
    {"id": "bridge", "title": "Cousin Corvin's Request",
     "text": "Corvin Hale asks you to clear the brigands and wolves holding the Coldwater bridge."},
    {"id": "tribute", "title": "A Scholar's Price",
     "text": "Kharim will share what his maps show, for 3000 gold."},
    {"id": "crown", "title": "The Drowned Crown",
     "text": "Bring Kharim the Drowned Crown from the Barrow of the Drowned King, south of Hallow Mere."},
    {"id": "hold", "title": "Hold the Marches",
     "text": "Hold three mines at once to prove the Compact can protect Kharim's work."},
]

offers = [
    {"id": "pay_tribute", "label": "Pay Kharim 3000 gold", "at": "Kharim's Camp",
     "cost": {"Gold": 3000}, "quest": "tribute",
     "then": [say(("Kharim", "Gold buys ink, and ink buys truth. Listen closely.")), {"clue": True}]},
    {"id": "give_crown", "label": "Hand over the Drowned Crown", "at": "Kharim's Camp",
     "item": "drowned_crown", "quest": "crown",
     "then": [say(("Kharim", "Look. The inside of the band is engraved with a map, and the map is of the tunnels. "
                             "The king wore the road on his skull."),
                  ("Kharim", "Here. And here. This one is a dead end. I would stake my maps on it.")),
              {"clue": True}]},
]

T = []
def trig(id, when, *do):
    # Ushari joins in level 1; a beat she speaks in before then waits for her.
    speaks = any(line[0] == "Ushari" for a in do for line in a.get("say", []))
    if speaks and id != "ushari_arrives" and not ({"after", "wait"} & when.keys()):
        if when["event"] in ("day", "enter_q"):     # these come round again: just gate them
            when = {**when, "after": "ushari_arrives"}
        else:                                       # one-off: what others say plays now, hers waits
            lines = [l for a in do for l in a.get("say", [])]
            cut = next(i for i, l in enumerate(lines) if l[0] == "Ushari")
            rest = [a for a in do if "say" not in a]
            if cut:
                T.append({"id": id, "when": when, "do": [{"say": lines[:cut]}] + rest})
                T.append({"id": id + "_ushari", "when": {**when, "wait": "ushari_arrives"},
                          "do": [{"say": lines[cut:]}]})
                return
            when = {**when, "wait": "ushari_arrives"}
    T.append({"id": id, "when": when, "do": list(do)})

# ── Level 1 — wolves in the vale ────────────────────────────────────────────
trig("intro", {"event": "start"},
     say(("Steward", "Varenhold is yours while Lord Aldren is away, commander. He rode east three days ago, and took the good horses."),
         ("Steward", "A rider came in at dawn. The Compact is sending someone to Varenhold. Someone important; the rider would not say who."),
         ("Steward", "But the wolves came down from the hills this winter. Three packs between us and the Coldwater, "
                     "and no escort will cross the vale while they hunt it."),
         ("Steward", "Clear the wolves, commander, and our guest can ride in.")),
     {"quest": "wolves"},
     lore("The Families",
          "The northern marches are held by old families sworn to the Ivory Compact: House Varen at Varenhold in the west, "
          "House Hale at Hallowmere by the mere. They share blood, borders, and a long habit of smiling at each other."))

trig("druid_seen", {"event": "see", "name": "Hooded Stranger"},
     say(("Scout", "Commander. Up by the hermit's hollow. There is a man standing in the middle of that wolf pack."),
         ("Scout", "Hooded. Leaning on a stick. The wolves lie around his feet like dogs at a hearth.")))

# The druid is Hul'rik; the player only meets a hooded stranger for now.
trig("druid_speaks", {"event": "engage", "name": "Hermit's Wolves"},
     say(("Hooded Druid", "That is far enough, son of Varen. These wolves were old in these hills before your walls were stone."),
         ("Hooded Druid", "I know what your brother went east to find. Hear me: a door swings both ways."),
         ("Hooded Druid", "My forefathers did not grow the roots across the old passage to keep your kind out. "
                          "They grew them thick, and deep, to keep something in."),
         ("Hooded Druid", "Your family's blood was always hot. Your grandfather's. Your brother's, hottest of all."),
         ("Hooded Druid", "I wish talking were enough. With Varens it never is. Teeth, then.")))

trig("druid_gone", {"event": "encounter_won", "name": "Hermit's Wolves"},
     say(("Scout", "The hooded man is gone, commander. No tracks in the frost. Nothing, where he stood."),
         ("Scout", "Only roots. Fresh ones, pushed up through ground that has been frozen since the autumn.")),
     {"vanish": "Hooded Stranger"},
     lore("The Hooded Man",
          "A druid stood with the wolves above the hermit's hollow and warned you off the old passage. "
          "His forefathers, he said, grew roots across it: not to keep the families out, but to keep something in."))

trig("ushari_arrives", {"event": "cleared", "names": VALE_WOLVES},
     say(("Steward", "Riders on the west road, commander! Compact colours. The vale is quiet enough for them now."),
         ("Ushari", "Ushari, of the Ivory Compact. So you are the Varen who cleared the road for me. Good. I dislike waiting."),
         ("Ushari", "The Compact did not send me for the view. Aldren rode east with Kharim's letter. The old passage, he said: "
                    "a road under the Greyfangs that runs all the way south to Umm'Natur."),
         ("Ushari", "Whoever holds that road reaches the desert crown first. Every house in the marches has heard the rumour."),
         ("Ushari", "Your cousin Corvin holds Hallowmere across the Coldwater. House Hale has promised us help. Let us see what a promise weighs.")),
     {"join": "ushari"}, {"quest_done": "wolves"}, {"quest": "main"}, {"quest": "aldren"})

trig("ushari_druid", {"event": "cleared", "names": VALE_WOLVES, "after": "druid_speaks"},
     say(("Ushari", "A hooded man who talks to wolves and warns you away from a door. "
                    "The north is full of old men with warnings, commander. The trouble is, most of them are right about something.")))

# Corvin's letter comes the morning after, so Ushari's arrival has the stage.
trig("corvin_gift", {"event": "day", "day": 2, "after": "ushari_arrives"},
     say(("Corvin", "Cousin! Word reached me that Aldren left you the keys. Good. You were always the steadier of the two."),
         ("Corvin", "I am sending twelve spears and a wagon of timber. Consider it an apology for the state of the bridge."),
         ("Corvin", "Brigands and wolves hold the Coldwater crossing. Clear it and Hallowmere is yours to recruit from, as if it flew your banner.")),
     {"troops": [{"id": "levy_spearman", "count": 12}]}, {"give": {"Wood": 5, "Gold": 500}},
     {"quest": "bridge"})

# ── Act 1 — the families ────────────────────────────────────────────────────
trig("bridge_seen", {"event": "see", "name": "Bridge Wardens"},
     say(("Scout", "The Coldwater bridge. Crossbows on the far bank and wolves in the reeds. They are not stopping travellers; they are counting them."),
         ("Scout", "Clear the bridge and we have a straight road to Hallowmere.")))

# Waits for Corvin's request, so a bridge cleared early still closes his quest.
trig("bridge_won", {"event": "encounter_won", "name": "Bridge Wardens", "wait": "corvin_gift"},
     say(("Ushari", "The brigands carried Hale coin. Fresh-struck. Someone paid them to sit on that bridge."),
         ("Ushari", "Probably nothing. Brigands rob everyone, including cousins.")),
     {"quest_done": "bridge"})

trig("hallowmere", {"event": "visit", "name": "Hallowmere", "unless": "betrayal_late"},
     say(("Corvin", "Welcome to Hallowmere, cousin. My gate is yours. My levies are yours. My wine is mostly yours."),
         ("Corvin", "Take this. The Hale signet. Show it anywhere in the lowlands and doors will open."),
         ("Corvin", "Aldren came through here four days ago. He asked about the old mines. Which ones, how deep. He seemed... excited."),
         ("Corvin", "If you find him, tell him his cousin still waits for that letter.")),
     {"item": "hale_signet"},
     {"troops": [{"id": "armoured_warrior", "count": 4}]})

trig("shariw_warning", {"event": "day", "day": 3},
     say(("Scout", "Riders in the east, commander. Veiled, on scorpions. Scorpions, commander. In the snow."),
         ("Ushari", "Shariw. Desert people, a thousand leagues from home. They know about the passage. They want it sealed, "
                    "and they will bury anyone standing in it.")))

# ── Act 2 — the brother's fall ──────────────────────────────────────────────
trig("aldren_horse", {"event": "day", "day": 5},
     say(("Messenger", "Commander. Lord Aldren's horse came back to Varenhold this morning. Saddled. No rider."),
         ("Ushari", "Horses bolt. Wolves. Weather. It means nothing yet.")),
     {"quest_text": ["aldren", "Aldren's horse came home without him. He was last seen at Hallowmere, asking about the old mines."]})

trig("aldren_found", {"event": "day", "day": 7},
     say(("Scout", "We found Lord Aldren, commander. At the foot of the Greyfangs, by the pass."),
         ("Scout", "He is dead. Three days, maybe four. The wolves had not touched him."),
         ("Ushari", "Wolves do not leave a body alone for four days. And these are not claw wounds. These are crossbow bolts."),
         ("Ushari", "He had a letter on him. Unsealed. It says only: 'I have found the right mine. Tell no one at the mere.'")),
     {"quest_text": ["aldren", "Aldren is dead, killed by crossbow bolts at the foot of the Greyfangs. His last letter "
                               "warns: 'Tell no one at the mere.' Hallowmere lies on the mere."]},
     lore("The Brother's Fall",
          "Lord Aldren Varen rode east to find the old passage and did not come back. He was killed at the foot of the Greyfangs "
          "by crossbow bolts. Wolves do not use crossbows."))

trig("aldren_corvin", {"event": "day", "day": 8, "after": "aldren_found"},
     say(("Corvin", "Cousin. I heard. I am so sorry. Aldren was reckless, but he did not deserve the pass."),
         ("Corvin", "Brigands, they say. I will hang every one I find. Hallowmere mourns with you.")))

trig("shariw_fear", {"event": "day", "day": 10},
     say(("Scout", "We caught a Shariw rider, commander. He would not say where the war camp is."),
         ("Scout", "Only this, over and over: 'You do not open a door to a house that is still awake.' Then he laughed until the cold took him."),
         ("Ushari", "They are not racing us for the passage. They are racing us to bury it.")),
     lore("The Shariw",
          "Desert raiders a thousand leagues from home. They do not want the passage. They want it sealed, "
          "because they believe something under the sand is still awake."))

trig("week_two", {"event": "day", "day": 9},
     say(("Ushari", "A week gone. The Shariw grow bolder, and whoever killed Aldren is still out there with a head start.")))

# ── The mythos: obelisks, relics, the barrows ──────────────────────────────
trig("obelisk_first", {"event": "see_type", "type": "obelisk"},
     say(("Scout", "Commander, there is a stone out there. Black. Taller than a man. Nobody on the maps put it there."),
         ("Scout", "The grass around it is dead in a perfect circle. The horses won't go near. Harl says it hums. Harl also says a lot of things.")))

trig("tarn_obelisk", {"event": "visit", "name": "Tarn Obelisk"},
     say(("Inscription", "WE WERE ASKED TO SLEEP. WE ARE SLEEPING. DO NOT COUNT THE STEPS."),
         ("Ushari", "It is not carved. Look at the edges. It grew, like ice on a window."),
         ("Ushari", "And the veins in it... they run downward. Toward the mountains.")),
     lore("The Obelisks",
          "Black stones stand across the marches. They were not raised by the families; the oldest songs already call them old. "
          "Violet veins run through them like blood through a wrist, and every vein points east, under the Greyfangs."))

trig("quarry_obelisk", {"event": "visit", "name": "Quarry Obelisk"},
     say(("Inscription", "THE KINGS BROUGHT SALT AND GRAIN AND THEIR SECOND SONS. WE BROUGHT THE DARK THAT KEEPS."),
         ("Ushari", "The quarrymen say the stone is warm in winter. They cut around it. Nobody has ever cut into it.")),
     lore("The Ones Beneath",
          "The barrow-kings of the north did not only bury their dead. They traded with something under the mountains: grain and salt "
          "and people sent down, and in return 'the dark that keeps'. What was kept, and for whom, the stones do not say."))

trig("drowned_obelisk", {"event": "visit", "name": "Drowned Obelisk"},
     say(("Inscription", "THE KING WENT INTO THE WATER SO THAT HE WOULD NOT HAVE TO GO DOWN."),
         ("Ushari", "Half of it is under the bog. The half we can see is dry. Completely dry."),
         ("Ushari", "Commander... I think the Drowned King was not escaping an enemy. I think he was escaping the bargain.")),
     lore("The Drowned King",
          "The last barrow-king walked into Hallow Mere wearing his crown rather than answer a summons from below. His barrow stands "
          "south of the mere. The dead there are said to keep the crown, and the crown to keep a map."))

trig("greyfang_obelisk", {"event": "visit", "name": "Greyfang Obelisk"},
     say(("Inscription", "SOUTH OF THE MOUNTAIN THE SAND IS A LID. UNDER THE LID, WE ARE STILL AWAKE."),
         ("Ushari", "South of the mountain is the desert. Pha'raxh. The empire that fell... or that went down, if this stone is honest."),
         ("Ushari", "These veins are not decoration, commander. They are a road. And roads lead somewhere that is expecting you.")),
     lore("The Veined",
          "Under the mountains and under the desert runs one people, older than the families and older than Pha'raxh: the Veined. "
          "Their roads are the old passages. Their veins run through stone and, the stones imply, through anything that stays below long enough."))

trig("warm_stone", {"event": "item", "item": "warm_stone"},
     say(("Ushari", "The men drew lots for who carries it. The loser asked to be flogged instead."),
         ("Ushari", "It is warm, commander. Always. And if you hold it long enough, you can feel it wait between beats.")))

trig("listening_shard", {"event": "item", "item": "listening_shard"},
     say(("Scout", "Held it to my ear. Nothing. Put it on the ground to sleep. Heard it all night."),
         ("Scout", "Not voices. Counting. Somebody down there counting.")))

trig("barrow_seen", {"event": "see", "name": "Barrow of the Drowned King"},
     say(("Ushari", "A barrow on the shore. The mist sits on it and does not move with the wind.")))

trig("crown_found", {"event": "item", "item": "drowned_crown"},
     say(("Ushari", "The crown. Wet, though the barrow was dry. Something is engraved inside the band. Lines. Branching lines."),
         ("Ushari", "Kharim will want this. I would like to be rid of it.")))

trig("first_mine", {"event": "see_type", "type": "old_mine"},
     say(("Ushari", "An old mine, sealed with fitted stone. Dwarf-work, the songs say, but no dwarf ever cut like that."),
         ("Ushari", "Aldren's letter spoke of four such shafts under the mountains. The barrow dead guard them. Only one goes down.")),
     {"quest_text": ["main", "Search the four old mines under the mountains. One of them hides the passage; the others hold only "
                             "dust or danger. The obelisks and Kharim can rule out the false ones."]})

# ── Act 3 — Kharim, and the cousin ──────────────────────────────────────────
trig("far_marches", {"event": "enter_q", "q_min": 7},
     say(("Messenger", "A rider from the scholar Kharim's camp, commander. He has seen your banners and begs you to come.")),
     {"reveal": [8, -9, 2]})

trig("kharim", {"event": "visit", "name": "Kharim's Camp"},
     say(("Kharim", "The Compact! Varen colours... then you are Aldren's kin. He was here, you know. Nine days ago."),
         ("Kharim", "He was so pleased. He said his cousin had given him the right mine. Which is strange, because I never told anyone which mine is right. I do not know it."),
         ("Kharim", "The histories say Pha'raxh fell. They lie. Its people went down. Beneath. The passages are their roads."),
         ("Kharim", "The old texts call the ones beneath the Veined. Not a people you conquer. A people you visit, once, and come back from changed."),
         ("Kharim", "My maps can rule out the false mines, if you help me. Gold, the Drowned Crown, and proof you can hold this country.")),
     {"quest": "tribute"}, {"quest": "crown"}, {"offer": "pay_tribute"}, {"offer": "give_crown"},
     lore("Kharim's Theory",
          "Pha'raxh did not fall; it went down. The old passages are the roads of the Veined, the ones beneath, "
          "and every barrow-king in the north once sent tribute along them."),
     {"quest_text": ["aldren", "Kharim says Aldren believed his cousin had told him 'the right mine'. Only House Hale could have sent him there."]})

trig("hold_three", {"event": "mines_held", "count": 3, "after": "kharim"},
     say(("Kharim", "Three mines under Varen banners! You can hold these marches after all. A promise is a promise.")),
     {"quest": "hold"}, {"quest_done": "hold"}, {"clue": True})

trig("kharim_joins", {"event": "quests_done", "count": 5},   # the vale's wolves + four of Kharim's era
     say(("Kharim", "My debts are paid and my maps are thinner. You have earned more than a clue, commander."),
         ("Kharim", "I am coming with you. Someone must read the walls when we go down.")),
     {"join": "kharim"})

BETRAYAL = [say(("Corvin", "Cousin. I had hoped the Greyfangs would take you the way they took Aldren. They were so close to doing it."),
         ("Corvin", "Yes. My bolts, my brigands, my bridge. He found the right shaft and he would have given it to the Compact. The Compact! "
                    "A thousand leagues away, deciding who rules the marches."),
         ("Corvin", "The Shariw offer something better. They seal the passage and go home, and House Hale holds the north. All of it."),
         ("Corvin", "Hallowmere's gate is closed to you. My household rides tonight. Do not make me bury two Varens.")),
     {"betray": {"at": "Hallowmere", "band": "Hale household", "army": HALE_HOUSEHOLD}},
     {"quest_text": ["aldren", "Corvin Hale murdered Aldren for the passage and has sided with the Shariw. Hallowmere is closed. Break the Hale household."]},
     lore("The Cousin",
          "Corvin Hale smiled, sent spears, gave you his signet, and had your brother shot at the foot of the pass. "
          "He has sold the north to the Shariw for the promise of ruling it.")]
# Corvin turns once you have eaten at his table (day 12+), or on day 16 regardless.
trig("betrayal", {"event": "day", "day": 12, "after": "hallowmere"}, *BETRAYAL)
trig("betrayal_late", {"event": "day", "day": 16, "unless": "betrayal"}, *BETRAYAL)

trig("hale_broken", {"event": "rival_beaten", "name": "Hale household"},
     say(("Corvin", "...Tell the Compact the north was never theirs."),
         ("Ushari", "He is dead, commander. The signet still fits your hand. Hallowmere will need a new lord.")),
     {"quest_done": "aldren"})

trig("recap", {"event": "enter_q", "q_min": 5},
     say(("Ushari", "Before the pass, commander, let us be clear about what we know."),
         ("Ushari", "Aldren found the right mine and was killed for it. The stones say the old kings sold their sons to something below the mountains."),
         ("Ushari", "And the Shariw would rather bury the door than see it opened. Whatever is down there, we will be the first to knock.")))

trig("war_camp", {"event": "see", "name": "Shariw War Camp"},
     say(("Scout", "The Shariw war camp. Sand-coloured tents on frozen ground. They brought their scorpions and their fires and their patience.")))

OUT.write_text(json.dumps({"start_companions": [], "quests": quests, "offers": offers, "triggers": T}, indent=2, ensure_ascii=False))
print(f"Wrote {OUT.name}: {len(quests)} quests, {len(offers)} offers, {len(T)} triggers")
