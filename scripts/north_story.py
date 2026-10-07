#!/usr/bin/env python3
"""The Old Passage: clear the road, search for Aldren, choose whom to trust.

Author knowledge: Aldren has become a zombie; this chapter does not reveal it. His traces lead
to a definite surface route toward the old passage, not its entrance. Corvin remains an ally but urges an optional conquest of his old rival at Rookwatch.
Kharim and the betrayal belong later.
Both endings take place on the night of the player's decision. On the home
route Ushari returns with the expedition to report and provision the next march.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "data/maps/old_passage.triggers.json"


def say(*lines):
    return {"say": [list(line) for line in lines]}


def lore(title, text):
    return {"lore": [title, text]}


# Castle lord uses ordinary income, buildings and weekly recruits. The passes
# contain his early expansion; two days after Corvin's warning he can march west.
RIVAL_LORD = {
    "home": "Rookwatch", "name": "Lord Edric Rook",
    "mobilize_after": "hallowmere", "territory_q_min": 5,
    "escort_at": "Greyfang Watch",
    "escort_army": [{"id": "woad_runner", "count": 12}, {"id": "cruth_slinger", "count": 6}, {"id": "painted_blade", "count": 3}],
    "army": [{"id": "woad_runner", "count": 18}, {"id": "cruth_slinger", "count": 8}, {"id": "painted_blade", "count": 3}],
    "garrison": [{"id": "woad_runner", "count": 16}, {"id": "cruth_slinger", "count": 8}],
}

quests = [
    {"id": "rook_escort", "title": "The Promised Escort", "main": False,
     "text": "Meet House Rook's escort at Greyfang Watch beyond the northern pass. They promise passage through their patrols, but you are still entering hostile land."},
    {"id": "rookwatch", "title": "Corvin's Old Rival", "main": False,
     "text": "Corvin urges you to take Rookwatch, seat of Lord Edric Rook, and secure a northern base. Conquest is optional: the southern Troll Gate also leads into the marches. Edric's army can take your mines and towns; defeat it and capture his castle to end his resistance."},
    {"id": "wolves", "title": "The Wolves of Varenhold",
     "text": "Kill all five wolf packs near the castle and drive them from House Varen's domain."},
    {"id": "aldren", "title": "Find Aldren", "main": True,
     "text": "Aldren left before Ushari arrived. Ask your cousin Corvin at Hallowmere where he was heading. Your father expects you to bring your brother home."},
    {"id": "bridge", "title": "The Road to Hallowmere",
     "text": "Clear the Coldwater bridge and reach Corvin at Hallowmere."},
]

offers = [
    {"id": "rookwatch_supplies", "label": "Accept Corvin's campaign supplies (750 gold, 5 wood)", "at": "Hallowmere",
     "then": [{"give": {"Gold": 750, "Wood": 5}},
              say(("Corvin", "For the road north. Once Rookwatch is yours, neither of us need fear Edric's tolls.")),
              lore("Corvin's contribution", "You accepted Corvin's supplies. He benefits if House Rook loses its castle; accepting his help does not bind you to the siege.")]},
    {"id": "hire_hale", "label": "Hire 16 Hale men-at-arms (1000 gold)", "at": "Hallowmere",
     "cost": {"Gold": 1000},
     "then": [{"troops": [{"id": "hale_man_at_arms", "count": 16}]},
              say(("Corvin", "Bring them home with your brother. I will need them for the harvest."))]},
]

T = []


def trig(id, when, *actions):
    T.append({"id": id, "when": when, "do": list(actions)})


trig("intro", {"event": "start"},
     say(("Steward", "Lord Aldren's guest is waiting on the west road. Five wolf packs have closed the vale."),
         ("Steward", "Clear the road and bring her safely to Varenhold.")),
     {"quest": "wolves"},
     lore("The Families", "Your family, House Varen, holds Varenhold. Your cousin Corvin Hale holds Hallowmere across the Coldwater."))

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

trig("vale_cleared", {"event": "cleared", "names": ["Hill Wolves", "Den Wolves", "Hermit's Wolves", "Lowland Wolves", "Thornwood Wolves"]},
     {"xp": 40}, {"quest_done": "wolves"},
     say(("Steward", "The west road is clear. Ushari can reach us after nightfall. End the day when you are ready.")))

trig("ushari_arrives", {"event": "night", "after": "vale_cleared"},
     {"join": "ushari"}, {"quest": "aldren"},
     lore("Ushari", "Ushari comes from the last witch tribe in the realm. Her people's rulers governed this land before the families of the Compact conquered it. Where others shut their doors to her people, Aldren welcomed her. She arrived expecting to find him at Varenhold."),
     {"choice": {"id": "ushari_at_night", "title": "Varenhold · After the wolves · That night",
                 "presentation": "conversation",
                 "lines": [
                     # [name] is reserved for future commander-name substitution.
                     ["Ushari", "[name], where is your brother?"],
                 ],
                 "text": "What do you tell her?",
                 "options": [
                     {"id": "stand_by_aldren", "label": "He went East for the passage.",
                      "then": [{"morale": {"id": "ushari", "value": 1}},
                               say(("Ushari", "East. The old passage. That is why I came to Varenhold. Beyond it lie my people's household records — the names the families tried to erase.")),
                               lore("Aldren's trail", "You told Ushari that Aldren went east to find the old passage. He had promised to help her recover the lost names of her people.")]},
                     {"id": "dismiss_aldren", "label": "How should I know?",
                      "then": [{"morale": {"id": "ushari", "value": -2}},
                               say(("Ushari", "I thought his own family might know where he went.")),
                               lore("A hard answer", "Ushari came to Varenhold expecting to find Aldren. Your reply left her uncertain how much she can trust you.")]},
                     {"id": "uncertain_aldren", "label": "Welcome to Varenhold. We are glad you are here.",
                      "then": [{"morale": {"id": "ushari", "value": 1}},
                               say(("Ushari", "Thank you. Aldren promised me safe passage here. I come from the last witch tribe; he said he would help me find the old passage.")),
                               lore("A welcome at Varenhold", "You welcomed Ushari into your home. Aldren had promised to help her find the old passage and the records of her people.")]},
                 ]}})

trig("ushari_road", {"event": "day", "after": "ushari_arrives", "delay": 1, "unless": "hallowmere"},
     say(("Steward", "Your father wants Aldren brought home. His last letter came through your cousin Corvin at Hallowmere, across Coldwater bridge."),
         ("Ushari", "Then we ask Corvin where he went. He promised me he would wait. Did he promise you that too?")),
     {"quest": "bridge"})

trig("bridge_seen", {"event": "see", "name": "Bridge Wardens"},
     say(("Scout", "Brigands at the Coldwater bridge. Hallowmere is on the far bank.")))

trig("hallowmere", {"event": "visit", "name": "Hallowmere", "after": "ushari_arrives"},
     say(("Corvin", "Ushari! He said you would be along. I kept a room for you both."),
         ("Ushari", "You should have kept him."),
         ("Corvin", "I tried. He wanted the old shafts under the Greyfangs. My shepherds saw his camp on the east road."),
         ("Corvin", "Take my signet. You can draw supplies here, and I have men to spare.")),
     say(("Corvin", "Edric Rook holds Rookwatch beyond Greyfang, at the western approach to his family's domain in Greyfen. His house has disputed those roads with mine since our grandfathers. Now he is turning travellers back."),
         ("Corvin", "Take Rookwatch. You cannot search those hills with his riders behind you. You would have a castle to supply your army, and I will help provision the march."),
         ("Ushari", "Your brother went missing in the mountains. Why are we laying siege to your cousin's neighbour?"),
         ("Corvin", "Because his neighbour controls the road. The southern Troll Gate is open to anyone foolish enough to fight through it.")),
     {"quest": "rookwatch"}, {"offer": "rookwatch_supplies"},
     say(("Corvin", "Rook has not called his estates to war yet. Your arrival will reach him by tonight. Expect him to start recruiting now; his riders could cross Greyfang in two days."),
         ("Ushari", "We should copy the old names as we go: your wall at Varenhold, the Coldwater bridge and the ruined assembly court south of Hallowmere. The old road charts use those names; without all three, we could follow the wrong bearings.")),
     lore("The Hale–Rook feud", "House Hale and House Rook dispute the northern roads and mining estates. Corvin calls the conquest of Rookwatch necessary for the search. Ushari questions the detour; the southern Troll Gate offers another way into the marches."),
     {"item": "hale_signet"}, {"give": {"Wood": 5}}, {"xp": 50},
     {"quest": "bridge"}, {"quest_done": "bridge"}, {"offer": "hire_hale"},
     {"reveal": [8, -9, 2]},
     {"quest_text": ["aldren", "Corvin's shepherds saw Aldren's camp on the east road. Search his camp beyond the Greyfangs. Copy the inscriptions at Varenhold, Coldwater bridge and the assembly court south of Hallowmere on the way. Cross by Greyfang Pass or the southern Troll Gate. Taking Rookwatch is optional; Corvin offers supplies and soldiers at Hallowmere."]})

trig("aldren_camp", {"event": "visit", "name": "Aldren's Camp", "wait": "hallowmere"},
     say(("Scout", "One bedroll. His saddlebag is still here."),
         ("Ushari", "I mended that strap before he left."),
         ("Scout", "It has been cut. There is a folded page beneath the bedroll."),
         ("Commander", "His handwriting. 'The mine surveyors copied the old roads. Kaldur lodge holds their chart. Find it before crossing Greyfen.' He was here."),
         ("Ushari", "Kaldur, then. The chart uses the old names: we need our copies from Varenhold, Coldwater bridge and the assembly court."),
         ("Scout", "For the army to continue into Greyfen, we also need an approach: Rookwatch's road, the cleared Troll Gate, or a way past the border patrols.")),
     {"reveal": [10, -13, 2]},
     lore("The Empty Camp", "Aldren's saddlebag lay beside an unused bedroll, its repaired strap cut. A note in his handwriting says the mine surveyors copied the old roads and directs him to their chart before crossing Greyfen. He was here; his present fate remains unknown."),
     {"quest_text": ["aldren", "Aldren's belongings are still at his camp. His note names the survey lodge at the Old Mine of Kaldur. Read its chart using the inscriptions at Varenhold, Coldwater bridge and the assembly court. Secure Rookwatch, Troll Gate, or a patrol route before departure."]})

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
trig("rook_warning", {"event": "day", "after": "hallowmere", "delay": 1, "unless": "rook_defeated"},
     say(("Scout", "Rook's riders are mustering. They may cross Greyfang tomorrow. Leave men at the mines you mean to hold.")))
trig("rook_seen", {"event": "see", "name": "Rookwatch"},
     say(("Scout", "Rookwatch. A working castle, with mines paying for the soldiers inside. Edric's field army recruits there.")),
     lore("Rookwatch", "House Rook draws income from its castle and mines, develops its town and recruits each week. Taking the castle cuts off Edric's northern base. His surviving field army can still contest your holdings."))
trig("rook_reply", {"event": "enter_q", "q_min": 5, "after": "hallowmere"},
     say(("Rook's Messenger", "Lord Edric says: You ride under Varen colours with Hale's blessing. Has your cousin finally found someone to bleed for his grandfather's claim?"),
         ("Commander", "I am looking for my brother."),
         ("Rook's Messenger", "Then look. Bring an army to our estates and we will meet it as an army.")),
     lore("Edric's reply", "Edric accuses Corvin of using your expedition to settle the Hale–Rook feud. His warning confirms the hostility, but says nothing certain about Aldren's fate."))
trig("rook_passage", {"event": "enter_q", "q_min": 5, "after": "hallowmere", "unless": "rook_captured"},
     {"choice": {"id": "rook_passage", "title": "House Rook's Road",
                 "text": "Rook's messenger waits for your answer. You could ask for passage to search for Aldren, or make your own way through the marches.",
                 "options": [
                     {"id": "ask_rook_passage", "label": "Ask House Rook for passage",
                      "detail": "Explain that you seek your brother, and request a meeting at Greyfang Watch. Rook is hostile; his word may not protect Ushari.",
                      "then": [
                          say(("Rook's Messenger", "Meet our escort at Greyfang Watch. They will guide you through the border patrols.")),
                          {"event": "escort_requested"},
                          {"reveal": [8, -5, 2]},
                          {"quest": "rook_escort"},
                          lore("Passage requested", "House Rook has promised a road escort at Greyfang Watch. Meet them at Greyfang Watch beyond the northern pass. Rook remains hostile; the promise is not a guarantee of safe conduct.")]},
                     {"id": "decline_rook_passage", "label": "We will find our own road",
                      "then": [say(("Commander", "We will make our own way. Tell your lord to keep his riders clear.")),
                               lore("No bargain with Rook", "You declined to seek passage from House Rook. The search and the optional conquest of Rookwatch remain your decisions.")]},
                 ]}})
trig("escort_accepted", {"event": "escort_requested"})
trig("escort_meeting", {"event": "visit", "name": "Greyfang Watch", "after": "escort_accepted"},
     {"ambush_from": {"at": "Greyfang Watch", "band": "Rook's escort", "say": [
         ["Rook's Messenger", "You may pass. Our men will show you the road."],
         ["Scout", "Their escort has spread out on both sides of us. Commander — those slingers are loading!"],
         ["Rook Captain", "A Varen asking passage for a witch. Your family always did lack the stomach to finish what it started."],
         ["Rook Captain", "Look at me, witch. Had it been left to House Rook, your people would all be dead."],
         ["Rook Captain", "Once we take the vacant throne of Umm'Natur, we will cleanse this land of witches and witch-lovers alike."],
         ["Ushari", "My people had names for this land before yours came to burn them. We are still here."],
         ["Commander", "Shields up. Stay together!"]]}},
     {"quest_text": ["rook_escort", "The promised escort attacked at Greyfang Watch. Defeat the patrol; if you retreat, its survivors remain in the field. You can also use Troll Gate or capture Rookwatch to proceed."]},
     lore("House Rook's false passage", "The escort attacked at Greyfang Watch. Its captain threatened to exterminate Ushari's people and their allies once House Rook takes the vacant throne of Umm'Natur."))
trig("escort_orders", {"event": "rival_beaten", "name": "Rook's escort"},
     {"quest_done": "rook_escort"},
     {"give": {"Gold": 300}},
     say(("Scout", "The captain carried the patrol schedule. There is a supply track through the border posts between their rounds. We can take it without storming Rookwatch.")),
     lore("A gap in the patrols", "The defeated escort carried House Rook's border schedule. It opens a scouted supply track into Greyfen. You recovered 300 gold from the company's pay chest. Rookwatch's defenders remain at the castle."))
trig("southern_route", {"event": "encounter_won", "name": "Troll Gate"},
     {"xp": 100},
     say(("Scout", "The southern track is clear. We can take the expedition through the hills without holding Rookwatch. There is no town to replenish us on that road.")),
     lore("The southern track", "Troll Gate gives access to the hill track into Greyfen. Taking it avoids a castle siege, but supplies must travel with the army; Rookwatch remains a hostile base."))
trig("southern_route_visit", {"event": "visit", "name": "Troll Gate", "unless": "southern_route"},
     lore("The southern track", "You scouted the cleared Troll Gate. The hill track into Greyfen is open, but it offers no friendly town for resupply."))
trig("rook_defeated", {"event": "rival_beaten", "name": "Lord Edric Rook"},
     say(("Scout", "Edric's field army is broken. Any castle still flying his banner can continue to muster a garrison.")),
     lore("Edric's army defeated", "You defeated Lord Edric Rook's field army. He will not raise a replacement hero in this chapter. Take his remaining local holdings to secure this stretch of the border. House Rook's wider domain lies ahead in Greyfen."))
trig("rook_captured", {"event": "capture", "name": "Rookwatch"},
     {"quest": "rookwatch"}, {"quest_done": "rookwatch"}, {"give": {"Gold": 750, "Wood": 5}},
     say(("Scout", "Rookwatch is ours. Its halls, workshops and recruiting grounds are intact. The stores hold 750 gold and five loads of timber. We have a supply base for the Greyfen road."),
         ("Ushari", "A castle for you. One less rival for Corvin. Now can we return to Aldren?")),
     lore("The capture of Rookwatch", "You took House Rook's castle. Its income, buildings and recruitment now support your expedition. Corvin's old rival has lost his border castle. House Rook still holds the domain beyond it in Greyfen; this victory has not answered what happened to Aldren."))
trig("corvin_rookwatch", {"event": "visit", "name": "Hallowmere", "after": "rook_captured"},
     say(("Corvin", "Rookwatch! Your father will hear of this. A safer road for you, cousin, and peace for my people."),
         ("Corvin", "My steward knows those disputed estates. If you need someone to help administer them while you search, you have only to ask."),
         ("Ushari", "He had someone ready.")),
     lore("Corvin's steward", "Corvin offered an administrator for the conquered estates. The castle remains under your command. His eagerness makes his stake in the expedition clearer."))
trig("crown_found", {"event": "item", "item": "drowned_crown"},
     say(("Scout", "The crown is wet. Everything else in the barrow was dry.")))

# Keep branch and puzzle IDs stable for unfinished expeditions. This chapter
# now identifies a surface route; neither ending enters the old passage.
HOME_ENDING = {
    "id": "father", "heading": "The Riderless Horse", "return_to": "Varenhold",
    "ushari": "with_player", "knowledge": "blood_on_aldrens_saddle", "departure_plan": "report_then_march",
    "route": "greyfen_road", "aldren_trace": "annotated_survey", "passage_entered": False,
    "next_chapter": "greyfen_road",
    "body": "You returned to your father with proof that Aldren reached the survey lodge and a clear route toward the old passage. Blood beneath his horse's saddle breaks your father's reassurance. Ushari has returned with you to provision the expedition. You will follow the secured route into House Rook's domain together; the passage itself is still ahead.",
    "frames": [
        {"scene": "home", "text": "Varenhold. That night."},
        {"scene": "home", "text": "You lay Aldren's annotated survey before your father.\nThe Greyfen causeway. The split watchtower. The southern escarpment.\nA road to follow, but no brother to bring home."},
        {"scene": "horse", "text": "Hooves in the courtyard.\nAldren's horse stands at the gate. No rider."},
        {"scene": "blood", "text": "The stablehand lifts the saddle.\nBlood has dried beneath it. Your father takes the lantern."},
        {"scene": "home", "text": "Ushari folds the survey beside you.\n‘We tell him what we found. Then we provision the army and go together.\nI will not walk into Rook's country alone.’"},
    ],
}
ROAD_ENDING = {
    "id": "ushari", "heading": "The Road Across Greyfen",
    "ushari": "with_player", "knowledge": "aldrens_route_confirmed", "departure_plan": "immediate_march",
    "route": "greyfen_road", "aldren_trace": "annotated_survey", "passage_entered": False,
    "next_chapter": "greyfen_road",
    "body": "Aldren's annotations confirm he reached the survey lodge. You and Ushari follow his marked route into House Rook's domain in Greyfen: the causeway, the split watchtower and the southern escarpment. Another stretch of open country lies ahead before you can find the old passage itself.",
    "frames": [
        {"scene": "road", "text": "The eastern marches. That night.\nThe survey chart lies open between you."},
        {"scene": "road_together", "text": "Ushari traces Aldren's fresh ink beside the old names.\n‘He found the road. Now we follow it.’"},
        {"scene": "road_together", "text": "Beyond the last ridge, House Rook's domain stretches beneath the stars.\nThe Greyfen causeway and its watchtowers lie ahead."},
        {"scene": "road_together", "text": "You set out together.\nThe old passage is still ahead.\nSo is the next part of Aldren's trail."},
    ],
}

trig("passage", {"event": "passage_found", "wait": "aldren_camp"},
     {"choice": {"id": "at_the_passage", "title": "The Road Ahead",
                 "text": "Aldren was here. His chart gives you a definite road toward the old passage through House Rook's domain in Greyfen. Return to your father with the news, or continue with Ushari.",
                 "options": [
                     {"id": "return_to_father", "label": "Report and regroup at Varenhold",
                      "detail": "Return together, tell your father what you found, and prepare the next march.",
                      "then": [{"ending": HOME_ENDING}]},
                     {"id": "follow_ushari", "label": "Follow Aldren's road with Ushari",
                      "detail": "Enter House Rook's domain in Greyfen together. The passage lies beyond their lands.",
                      "then": [{"ending": ROAD_ENDING}]},
                 ]}})

# These are public records of a conquered country, not numbered puzzle clues.
# Keep the rune IDs stable so earlier saves retain their discovery history.
READINGS = [
    ("root", "Varenhold Wall Rune Stone", "Esh Varen", [
        ("Scout", "Old marks in the boundary wall. The masons set this block sideways."),
        ("Ushari", "Esh Varen. The orchard settlement. My grandmother still called this valley that."),
        ("Commander", "Varenhold was built on empty land."),
        ("Ushari", "That is what your family wrote. This stone names the households whose land it was."),
        ("Ushari", "I can read three names. The mason cut through the others.")],
     "Esh Varen — the orchard settlement beneath Varenhold. Its household stone was reused in your family's boundary wall; several names were cut away. Ushari's grandmother remembered the settlement."),
    ("stream", "Coldwater Bridge Rune Stone", "Tal Eren", [
        ("Commander", "The north arch bears my grandfather's crest."),
        ("Ushari", "There is an older dedication below it. Tal Eren. The crossing of the joined waters. Queen Sereth commissioned this bridge."),
        ("Commander", "Our histories say he built the crossing."),
        ("Ushari", "He set his crest over hers. Look: the older letters continue beneath the mortar.")],
     "Tal Eren — the river crossing now called Coldwater. A dedication to Queen Sereth lies beneath the Varen crest. Your family's history credits the bridge to your grandfather."),
    ("moon", "Assembly Rune Stone", "Orun", [
        ("Scout", "There are wagon tracks. Someone has been taking building stone."),
        ("Ushari", "Orun. The assembly court. My people's rulers heard petitions here. These lines name the communities entitled to speak."),
        ("Commander", "Did you know this place was here?"),
        ("Ushari", "I knew the word. My mother never knew where it stood."),
        ("Ushari", "Give me a moment. I want to copy the names before another wagon comes.")],
     "Orun — the assembly court, now being quarried for building stone. Its surviving inscription names communities that petitioned Ushari's people's rulers. Even Ushari's mother no longer knew its location."),
]
for rune, site, original_name, lines, record in READINGS:
    trig("gate_sketch_" + rune,
         {"event": "visit", "name": site, "unless": "ushari_arrives"},
         say(("Scout", "An inscription beneath the newer masonry. I will take a rubbing; someone may still read it.")))
    trig("gate_rune_" + rune,
         {"event": "visit", "name": site, "wait": "ushari_arrives"},
         say(*lines), lore("Gate rune · " + original_name, {"root": "Branch mark. ", "stream": "Joined-lines mark. ", "moon": "Circle mark. "}[rune] + record))

passage = next(t for t in T if t["id"] == "passage")
branch = passage["do"][-1]
passage["do"] = [
    say(("Scout", "A survey lodge beside the shaft. There is a road diagram cut into the table — and a newer copy on the desk."),
        ("Commander", "Aldren's hand again. He corrected the surveyor's bearings. The ink has not faded."),
        ("Ushari", "The diagram uses the old names. From the orchards, cross the joined waters; bring your petition to the court, then return to the house of names."),
        ("Ushari", "Ishara. My mother called it the house of names. Our household records were kept there before your family took this land."),
        ("Commander", "Does it show the passage?"),
        ("Ushari", "It shows the road toward it. We must cross Greyfen first. Let me match his bearings to these names. Before we leave, our army still needs a way through the border: the castle road, Troll Gate, or a gap in the patrols.")),
    lore("Aldren's survey", "At the surface lodge beside an old shaft, you found Aldren's annotated copy of the surveyors' road chart. His handwriting and corrected bearings confirm that he reached this place. The chart leads across Greyfen toward the old passage; it is not the passage entrance."),
    lore("Gate rune · Ishara", "Doorway mark. Ishara — the house of names, the destination on the old road diagram. Ushari knows the name from her mother. Her people's household records remain her reason for seeking the passage beyond Greyfen."),
    {"quest_text": ["aldren", "Aldren's survey survives in the lodge beside this shaft. Match its old place names using the inscriptions at Varenhold, Coldwater bridge and the ruined assembly court. Before departure, we also need an approach for the army: hold Rookwatch, clear Troll Gate, or recover the escort's patrol orders. We can leave this survey and return when ready."]},
    {"choice": {"id": "passage_gate", "title": "The Surveyors' Road", "allow_exploration": True,
                "options": [{"id": "esh_varen_tal_eren_orun_ishara", "label": "Trace the route",
                    "requires_lore": ["Gate rune · Esh Varen", "Gate rune · Tal Eren", "Gate rune · Orun"],
                    "requires_route": True,
                    "then": [say(("Ushari", "Esh Varen. Tal Eren. Orun. Ishara. Now the bearings fit."),
                                 ("Commander", "Across the Greyfen causeway, south past the split watchtower, then along the escarpment to the old road cut. He marked every turn."),
                                 ("Scout", "Greyfen is House Rook's domain. Rookwatch guards its western approach; their lands run beyond the castle."),
                                 ("Ushari", "Then Aldren's road takes us through their country. We know where to go, but we will have to get through it.")),
                             lore("The Greyfen road", "Aldren's marked route is clear: cross the Greyfen causeway, head south past the split watchtower, then follow the southern escarpment to the old road cut. These are House Rook's lands. Taking Rookwatch does not conquer the domain beyond it. The old passage itself has not been found or entered."),
                             {"quest_text": ["aldren", "Aldren was at the survey lodge. His chart gives a clear route through House Rook's domain in Greyfen toward the old passage. Return to your father with the news, or set out with Ushari. Aldren's fate remains unknown."]}, branch]}]}}
]

OUT.write_text(json.dumps({"survey_site": "Old Mine of Kaldur", "rival_lord": RIVAL_LORD, "start_companions": [], "essential_companions": ["ushari"], "passage_ends_scenario": False,
                          "quests": quests, "offers": offers, "triggers": T}, indent=2, ensure_ascii=False) + "\n")
print(f"Wrote {OUT.name}: {len(quests)} quests, {len(offers)} offers, {len(T)} triggers")
