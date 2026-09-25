#!/usr/bin/env python3
"""The Old Passage: clear the road, search for Aldren, choose whom to trust.

Aldren is already dead, but this chapter does not reveal it. Nobody has used
this entrance. Corvin remains an ally; Kharim and the betrayal belong later.
Both endings take place on the night of the player's decision. On the home
route Ushari enters alone, with her progression retained for a later reunion.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "data/maps/old_passage.triggers.json"


def say(*lines):
    return {"say": [list(line) for line in lines]}


def lore(title, text):
    return {"lore": [title, text]}


quests = [
    {"id": "wolves", "title": "A Guest at Varenhold",
     "text": "Clear the Hill Wolves, Den Wolves and Hermit's Wolves so Aldren's guest can reach Varenhold."},
    {"id": "aldren", "title": "Find Aldren", "main": True,
     "text": "Aldren left before Ushari arrived. Ask your cousin Corvin at Hallowmere where he was heading. Your father expects you to bring your brother home."},
    {"id": "bridge", "title": "The Road to Hallowmere",
     "text": "Clear the Coldwater bridge and reach Corvin at Hallowmere."},
]

offers = [
    {"id": "hire_hale", "label": "Hire 16 Hale men-at-arms (1000 gold)", "at": "Hallowmere",
     "cost": {"Gold": 1000},
     "then": [{"troops": [{"id": "hale_man_at_arms", "count": 16}]},
              say(("Corvin", "Bring them home with your brother. I will need them for the harvest."))]},
]

T = []


def trig(id, when, *actions):
    T.append({"id": id, "when": when, "do": list(actions)})


trig("intro", {"event": "start"},
     say(("Steward", "Lord Aldren's guest is waiting on the west road. Three wolf packs have closed the vale."),
         ("Steward", "Clear the road and bring her safely to Varenhold.")),
     {"quest": "wolves"},
     lore("The Families", "Your family, House Varen, holds Varenhold. Your cousin Corvin Hale holds Hallowmere across the Coldwater."))

trig("ushari_arrives", {"event": "cleared", "names": ["Hill Wolves", "Den Wolves", "Hermit's Wolves"]},
     say(("Steward", "Ushari, my lord. She asked for Lord Aldren's rooms."),
         ("Ushari", "He left? We were meant to ride together."),
         ("Steward", "East, to his cousin's. Your father wants you to fetch him home."),
         ("Ushari", "Then I am coming with you.")),
     {"xp": 40}, {"join": "ushari"}, {"quest_done": "wolves"}, {"quest": "aldren"},
     lore("Ushari", "Aldren's lover arrived expecting to travel with him. Her family keeps the old rites; in the northern halls, people call them witches."))

trig("ushari_road", {"event": "day", "after": "ushari_arrives", "unless": "hallowmere"},
     say(("Ushari", "He promised me he would wait. Did he promise you that too?")),
     {"quest": "bridge"})

trig("bridge_seen", {"event": "see", "name": "Bridge Wardens"},
     say(("Scout", "Brigands at the Coldwater bridge. Hallowmere is on the far bank.")))

trig("hallowmere", {"event": "visit", "name": "Hallowmere", "after": "ushari_arrives"},
     say(("Corvin", "Ushari! He said you would be along. I kept a room for you both."),
         ("Ushari", "You should have kept him."),
         ("Corvin", "I tried. He wanted the old shafts under the Greyfangs. My shepherds saw his camp on the east road."),
         ("Corvin", "Take my signet. You can draw supplies here, and I have men to spare.")),
     {"item": "hale_signet"}, {"give": {"Wood": 5}}, {"xp": 50},
     {"quest": "bridge"}, {"quest_done": "bridge"}, {"offer": "hire_hale"},
     {"reveal": [8, -9, 2]},
     {"quest_text": ["aldren", "Corvin's shepherds saw Aldren's camp on the east road. Search there, then check the old shafts. Corvin has offered supplies and soldiers."]})

trig("aldren_camp", {"event": "visit", "name": "Aldren's Camp", "wait": "hallowmere"},
     say(("Scout", "One bedroll. His saddlebag is still here."),
         ("Ushari", "I mended that strap before he left."),
         ("Scout", "It has been cut.")),
     {"clue": True, "silent": True},
     lore("The Empty Camp", "Aldren's saddlebag lay beside an unused bedroll. Its repaired strap had been cut. There was no sign of him."),
     {"quest_text": ["aldren", "Aldren's belongings are still at his camp. Search the old shafts for another trace of him; the route marked in his notes rules out one of them."]})

# Optional discoveries stay brief. They suggest a history without explaining it.
for id, name, inscription in [
    ("tarn_obelisk", "Tarn Obelisk", "WE WERE ASKED TO SLEEP. DO NOT COUNT THE STEPS."),
    ("quarry_obelisk", "Quarry Obelisk", "THE KINGS BROUGHT SALT AND GRAIN AND THEIR SECOND SONS."),
    ("drowned_obelisk", "Drowned Obelisk", "THE KING WENT INTO THE WATER SO THAT HE WOULD NOT HAVE TO GO DOWN."),
    ("greyfang_obelisk", "Greyfang Obelisk", "SOUTH OF THE MOUNTAIN THE SAND IS A LID."),
]:
    trig(id, {"event": "visit", "name": name}, say(("Inscription", inscription)), lore(name, inscription))

trig("witch_ways", {"event": "visit", "name": "Tarn Obelisk", "wait": "ushari_arrives"},
     say(("Ushari", "My mother made us cover stones like this before we slept.")))
trig("warm_stone", {"event": "item", "item": "warm_stone", "wait": "ushari_arrives"},
     say(("Ushari", "Wrap it. Keep it away from your skin.")))
trig("listening_shard", {"event": "item", "item": "listening_shard"},
     say(("Scout", "It only starts counting when I put it down.")))
trig("raiders", {"event": "see", "name": "Shariw War Camp"},
     say(("Scout", "Riders ahead. They are pulling down the old shafts. If they get there first, we lose the trail.")))
trig("crown_found", {"event": "item", "item": "drowned_crown"},
     say(("Scout", "The crown is wet. Everything else in the barrow was dry.")))

HOME_ENDING = {
    "id": "father", "heading": "The Riderless Horse", "return_to": "Varenhold",
    "ushari": "entered_alone", "knowledge": "blood_on_aldrens_saddle",
    "body": "You returned to your father. Blood beneath Aldren's saddle has broken his reassurance. That same night, Ushari entered the passage alone. Your paths have separated.",
    "frames": [
        {"scene": "home", "text": "Varenhold. That night."},
        {"scene": "home", "text": "Your father has left a place for Aldren at the table.\n\n“He will come home. Stay here.”"},
        {"scene": "horse", "text": "Hooves in the courtyard.\nAldren's horse stands at the gate. No rider."},
        {"scene": "blood", "text": "The stablehand lifts the saddle.\nBlood has dried beneath it. Your father takes the lantern."},
        {"scene": "alone", "text": "Under the Greyfangs, that same night,\nUshari steps into the passage alone."},
    ],
}
TUNNEL_ENDING = {
    "id": "ushari", "heading": "Into the Passage",
    "ushari": "with_player", "knowledge": "aldren_still_missing",
    "body": "You followed Ushari instead of returning to your father. Together you have entered the untouched passage. Aldren is still missing; you have found no evidence that he came this way.",
    "frames": [
        {"scene": "tunnel", "text": "Under the Greyfangs. That night."},
        {"scene": "tunnel", "text": "Ushari binds a thread around her wrist,\nthen offers you the other end."},
        {"scene": "together", "text": "Your footsteps break the dust.\nThere are no tracks ahead of you."},
        {"scene": "together", "text": "She raises the lamp.\nYou go down together."},
    ],
}

trig("passage", {"event": "passage_found", "wait": "hallowmere"},
     say(("Scout", "Steps beyond the rockfall. The dust is unbroken. Nobody has been through."),
         ("Commander", "Father told us to come back if we could not find him. We should wait at home."),
         ("Ushari", "Aldren came looking for this. I want to know why. Come with me.")),
     {"quest_text": ["aldren", "The trail ends at an untouched passage. There is no sign that Aldren entered. Return to your father and wait, or investigate with Ushari."]},
     {"choice": {"id": "at_the_passage", "title": "The Way Back",
                 "text": "Your father expects you home. Ushari means to enter the passage tonight.",
                 "options": [
                     {"id": "return_to_father", "label": "Return to your father",
                      "detail": "Ushari will continue alone.",
                      "then": [{"leave": "ushari"}, {"ending": HOME_ENDING}]},
                     {"id": "follow_ushari", "label": "Go with Ushari",
                      "detail": "Enter the passage together instead of returning home.",
                      "then": [{"ending": TUNNEL_ENDING}]},
                 ]}})

OUT.write_text(json.dumps({"start_companions": [], "essential_companions": ["ushari"], "passage_ends_scenario": False,
                          "quests": quests, "offers": offers, "triggers": T}, indent=2, ensure_ascii=False) + "\n")
print(f"Wrote {OUT.name}: {len(quests)} quests, {len(offers)} offers, {len(T)} triggers")
