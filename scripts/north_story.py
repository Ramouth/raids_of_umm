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

HALE_HOUSEHOLD = [{"id": "woad_runner", "count": 24}, {"id": "cruth_slinger", "count": 10},
                  {"id": "painted_blade", "count": 4}]

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
    {"id": "bridge", "title": "The Road to Hallowmere",
     "text": "Your cousin Corvin Hale holds Hallowmere across the Coldwater. House Hale has promised help. "
             "Brigands hold the bridge; get past them and ride to meet him."},
    {"id": "tribute", "title": "A Scholar's Price",
     "text": "Kharim will share what his maps show, for 3000 gold."},
    {"id": "crown", "title": "The Drowned Crown",
     "text": "Bring Kharim the Drowned Crown from the Barrow of the Drowned King, south of Hallow Mere."},
    {"id": "hold", "title": "Hold the Marches",
     "text": "Hold three mines at once to prove the Compact can protect Kharim's work."},
]

offers = [
    # Corvin's men are cheap and good. They are also his (see "turncoats" in the betrayal).
    {"id": "hire_hale", "label": "Hire 16 Hale men-at-arms (1000 gold)", "at": "Hallowmere",
     "cost": {"Gold": 1000},
     "then": [{"troops": [{"id": "hale_man_at_arms", "count": 16}]},
              say(("Corvin", "Sixteen of my best, and I charge you only for their boots. Family is family."),
                  ("Ushari", "Men who wear another house's colours fight for that house, commander. Remember whose heron is on those shields."))]},
    {"id": "pay_tribute", "label": "Pay Kharim 3000 gold", "at": "Kharim's Camp",
     "cost": {"Gold": 3000}, "quest": "tribute",
     "then": [say(("Kharim", "Gold buys ink, and ink buys truth. Listen closely.")), {"clue": True}, {"xp": 75}]},
    {"id": "give_crown", "label": "Hand over the Drowned Crown", "at": "Kharim's Camp",
     "item": "drowned_crown", "quest": "crown",
     "then": [say(("Kharim", "Look. The inside of the band is engraved with a map, and the map is of the tunnels. "
                             "The king wore the road on his skull."),
                  ("Kharim", "Here. And here. This one is a dead end. I would stake my maps on it.")),
              {"clue": True}, {"xp": 100}]},
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

# Her arrival is about her: she can feel the passage. The politics wait for the morning.
trig("ushari_arrives", {"event": "cleared", "names": VALE_WOLVES},
     say(("Steward", "Riders on the west road, commander! Compact colours. The vale is quiet enough for them now."),
         ("Ushari", "Ushari, of the Ivory Compact. So you are the Varen who cleared the road for me. Good. I dislike waiting."),
         ("Ushari", "Your brother went looking for the old passage with a scholar's letter. He will not find it with a letter."),
         ("Ushari", "I can feel it, commander. East, under the mountains. The way you feel a hearth with your eyes closed."),
         ("Ushari", "Your hooded man felt it too, I think. Nobody grows roots that thick over nothing.")),
     {"xp": 40},   # the vale cleared: with the three packs, the commander reaches level 2
     {"join": "ushari"}, {"quest_done": "wolves"}, {"quest": "main"}, {"quest": "aldren"},
     lore("Ushari",
          "The Compact sent Ushari north because she can feel the old passage, faintly, the way you feel heat on your face. "
          "She says it lies east, under the Greyfangs. Closer than that, she cannot say. Yet."))

trig("ushari_road", {"event": "day", "day": 2, "after": "ushari_arrives", "unless": "hallowmere"},
     say(("Ushari", "Whoever holds that road reaches the desert crown first, and every house in the marches has heard the rumour."),
         ("Ushari", "Your cousin Corvin holds Hallowmere across the Coldwater. House Hale has promised us help. Let us see what a promise weighs.")),
     {"quest": "bridge"})

# ── Act 1 — the families ────────────────────────────────────────────────────
trig("bridge_seen", {"event": "see", "name": "Bridge Wardens"},
     say(("Scout", "The Coldwater bridge. Crossbows on the far bank and wolves in the reeds. They are not stopping travellers; they are counting them."),
         ("Scout", "Clear the bridge and we have a straight road to Hallowmere.")))

trig("bridge_won", {"event": "encounter_won", "name": "Bridge Wardens"},
     say(("Ushari", "The brigands carried Hale coin. Fresh-struck. Someone paid them to sit on that bridge."),
         ("Ushari", "Probably nothing. Brigands rob everyone, including cousins.")))

# The cousin's scene: the first time you ride in with Ushari. He sells you his men.
trig("hallowmere", {"event": "visit", "name": "Hallowmere", "unless": "betrayal_late", "after": "ushari_arrives"},
     say(("Corvin", "Cousin! Welcome to Hallowmere. My gate is yours. My wine is mostly yours."),
         ("Corvin", "And this must be the Compact's famous bloodhound. Ushari, is it? They say you can smell gold through a mountain."),
         ("Ushari", "Not gold, my lord."),
         ("Corvin", "Take this. The Hale signet. Show it anywhere in the lowlands and doors will open."),
         ("Corvin", "Aldren came through here four days ago. He asked about the old mines. Which ones, how deep. He seemed... excited."),
         ("Corvin", "You will want soldiers where you are going. My household men-at-arms are yours, for what their kit cost me.")),
     {"item": "hale_signet"}, {"quest_done": "bridge"}, {"xp": 50}, {"offer": "hire_hale"})

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
     {"quest": "hold"}, {"quest_done": "hold"}, {"clue": True}, {"xp": 75})

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
     # The men you bought from him were always his.
     {"turncoats": {"unit": "hale_man_at_arms", "band": "Hale men-at-arms",
                    "say": [["Scout", "Commander! The Hale men-at-arms, the ones we paid for. They have drawn steel inside the camp!"],
                            ["Ushari", "The heron was on their shields the whole time. To arms!"]]}},
     {"quest_text": ["aldren", "Corvin Hale murdered Aldren for the passage and has sided with the Shariw. Hallowmere is closed. Break the Hale household."]},
     lore("The Cousin",
          "Corvin Hale smiled, sold you his own soldiers, gave you his signet, and had your brother shot at the foot of the pass. "
          "He has sold the north to the Shariw for the promise of ruling it.")]
# Corvin turns a few days after you have eaten at his table (day 12 at the earliest),
# long enough for his men to feel like yours. Never visited by day 16: he turns anyway.
trig("betrayal", {"event": "day", "day": 12, "after": "hallowmere", "delay": 4}, *BETRAYAL)
trig("betrayal_late", {"event": "day", "day": 16, "unless": "hallowmere"}, *BETRAYAL)

trig("hale_broken", {"event": "rival_beaten", "name": "Hale household"},
     say(("Corvin", "...Tell the Compact the north was never theirs."),
         ("Ushari", "He is dead, commander. The signet still fits your hand. Hallowmere will need a new lord.")),
     {"quest_done": "aldren"}, {"xp": 150})

# The third companion: Corvin's sister comes over once his household is broken.
# She and Ushari do not trust each other (the heron). In battle she punishes
# anyone who turns their back on her (attack of opportunity).
trig("maerwen_joins", {"event": "rival_beaten", "name": "Hale household", "after": "hale_broken"},
     say(("Maerwen", "Hold. I am Maerwen Hale, his sister. I held his gate while he sold the passage to the Shariw. I did not know. Or I did not want to."),
         ("Maerwen", "The heron should still stand for something. Let me carry it where he would not: into the dark, with you."),
         ("Ushari", "A Hale at our backs. Forgive me if I keep my shield on that side, my lady."),
         ("Maerwen", "Keep it there. Anyone who turns their back on me learns why herons stand so still.")),
     {"join": "maerwen"},
     lore("Maerwen Hale",
          "Corvin's sister and the shield-captain of Hallowmere. She does not chase an enemy; she waits for it to turn away. "
          "Ushari does not trust her, and says so."))

trig("recap", {"event": "enter_q", "q_min": 5},
     say(("Ushari", "Before the pass, commander, let us be clear about what we know."),
         ("Ushari", "Aldren found the right mine and was killed for it. The stones say the old kings sold their sons to something below the mountains."),
         ("Ushari", "And the Shariw would rather bury the door than see it opened. Whatever is down there, we will be the first to knock.")))

trig("war_camp", {"event": "see", "name": "Shariw War Camp"},
     say(("Scout", "The Shariw war camp. Sand-coloured tents on frozen ground. They brought their scorpions and their fires and their patience.")))

OUT.write_text(json.dumps({"start_companions": [], "quests": quests, "offers": offers, "triggers": T}, indent=2, ensure_ascii=False))
print(f"Wrote {OUT.name}: {len(quests)} quests, {len(offers)} offers, {len(T)} triggers")
