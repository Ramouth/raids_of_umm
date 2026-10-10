# Aldren's bond: played, not told (proposal, nothing built)

Status: draft for the user. Nothing in `chapter1_story.py`, `gen_chapter1.py` or `data/` has changed.
Direction A-D is approved. The lines and the consequence map below are proposals.

The problem: Aldren is with the player only in 1.0, and the bond is told in dialogue.
The fix: his character is the way he fights. He leaves two habits that 1.3 pays off. Without
him, 1.1 is harder. And the sword he leaves behind is the chapter's first real choice.

## 1. The beats (exact lines, three at most each)

**A. 1.0: he fights recklessly** (battle banter, the extended tips system)
- The first time Aldren rushes ahead of the line:
  Drest: "Stay back behind the lines!" / Aldren *(laughing)*: "Then keep up, old man."
- Thicket Dogs (`corridor_ambush`): Aldren hacks through the tree line instead of going round it.
  Aldren: "Through, not round."

**B. 1.0: what 1.3 pays off**
- After the Thicket Dogs are beaten (`encounter_won`), a scene frame: "Aldren cuts an A into the
  bark with his knife." Aldren: "So they know who cleared this wood."
  (1.3 Bridge Post: "an A, in the hand of his letter". Leave that line as it is and let the player make the link.)
- Poachers' Fire (dead end), only while Aldren rides with you, a scene frame: "Aldren kneels and banks the old
  fire: ash over the coals, stones round the edge." Drest: "Every fire, every time. The rain
  would do it for you." Aldren: "You taught me."
  (1.3 Cold Fire, unchanged: "I taught him to bank a fire like that. He was listening, then.")

**C. 1.1: the gap**
- With Aldren gone, the first 1.1 battle is harder anyway (his 90 hit points are gone). No new data.
- The first time the hero rushes, Drest says: "Your brother would have done that. Don't."
  (Note: 1.2 already has "Your brother would have reached for his sword." Two "your brother would have"
  lines can stand as a refrain, or one of them can change.)

**D. The toast loses its choice, and the sword becomes the choice**
- Toast scene, no options. Your father: "To Aldren. To my heir." Text: "Every cup in the hall
  goes up. Aldren drinks, and does not look at you."
- Dawn, frames: "First light. Aldren's horse is gone from the stable." / "In the hall the sword
  of your forefathers hangs on its pegs. A red hand on the hilt; a note pinned beneath."
- Choice `sword`, *The Sword of Your Forefathers*. Note: "I leave the sword of our forefathers.
  Do not take it. Do not come after me. — A."
  Options: **Take the sword** (detail: "He asked you not to.") / **Leave it on the wall**.
  The player always breaks the second order, because following him is the plot. The first order is
  the one the player can choose to keep. The red hand is still seen on both branches, in the frame.

**The Ushari contradiction** (1.2 says "the man who carried that sword rode past us"). She cannot
say that if the sword is still in the hall. Options:
1. *The red hand on his shield.* Ushari: "A man with a red hand on his shield rode past us going
   east." Drest: "Aldren's shield." On take, she adds: "The same hand is on your sword." This works on
   both branches, and the red hand gets one more sighting.
2. *His face.* Ushari: "You have his face. He rode past us going east." This works on both branches,
   adds no new object, and does not touch the red-hand thread.
3. *Only the sword.* On take, the line stays as it is. On leave, she says "You are going east. So am I."
   This is the weakest: her reason to ride with you goes thin on the leave branch.

## 2. Consequence map: the sword

Build: **a** = buildable now (condition trees with `{"item": "red_hand_sword"}` / `choice.sword`,
`betray`, `vanish`, `reveal`, items, encounters); **b** = needs a new system; **c** = far future.
Feel: **S** = the player feels it in play (fight, item, route, decision); **W** = text only.

| Where | Take | Leave | Build | Feel |
|---|---|---|---|---|
| 1.0 dawn | the strongest weapon of Chapter 1 | nothing in hand; the note as a lore entry | a | S |
| 1.1 battles | the hero hits harder | the hero is weaker, on the map where Aldren's loss already bites | a | S |
| 1.1 quest text, Barrow door | "left you his sword"; "Like the one on your sword." | "left his sword on the wall"; "Like the one on the sword Aldren left." | a | W |
| 1.1 hooded man (hear) | He knows it. Hooded Man: "That sword went east before you. Follow it." Quest text names the road. | "Son of Varen." only | a now; b once the three roads out of 1.1 exist | W now, S later |
| 1.1 Varen Barrow (when built) | an extra clue | the barrow is the only place to learn the road: going down becomes necessary | b (dungeon, three roads) | S |
| 1.2 Ushari | she recognises the sword and tells you more. Ushari: "He asked me the way to the roots." Sets a flag, and 1.3 starts with the mine and the Cold Fire revealed | she joins (see options above) and the 1.3 traces must be found | a | S (route) |
| 1.2 Red Banner | Captain: "That hand is not yours to carry." New option: **Hand over the sword** (`take_item`; cross free, no toll, no fight) | the conversation as today | a | S |
| 1.2 hunters | red-hand outriders ride out from a small new site (`betray`, `if` sword) | the site is removed at start (`vanish`) | a (one map site) | S |
| 1.3 hunters | a band waits at a choke on the east road | removed at start | a (one map site) | S |
| 1.3 Broken Camp | optional extra line. Ushari: "They asked my people about a sword." (heavy: does the choice get her people killed? see Q4) | as today | a | W, heavy |
| Ch3 3.2 Red Banner trophy | if handed over in 1.2, the sword is won back from the Red Banner | (n/a) | c | S |
| Ch4 Drest's death near the Walled Nation | Drest: "Give it back to him, if you find him." | Drest kept it all along. Drest: "He said don't take it. He never said I couldn't give it." Take or leave again, a second choice | c | S |
| Ch4 4.2 zombie Aldren (decided: "for one moment he knows the sword") | he hesitates: loses his first turn | conflicts with the decided beat (see Q2) | b (an enemy skips a turn under a condition) | S |
| 4.3 crown | the hilt is the first link in the mark chain (hilt, banner, tomb door, crown) | the same, seen on the wall | c | W |

Hunters pay XP and gold, so taking the sword can come out richer. The per-map level cap absorbs
part of that (see Q5).

## 3. Consequence map: A-C (brief)

| Beat | Pays off where | Build | Feel |
|---|---|---|---|
| A: rushes / breaks the tree line | 1.1 Drest's "Don't." (C). Ch4 4.2: zombie Aldren still rushes and breaks your line the way he did in 1.0, so the player knows him by how he fights | 1.0 banter: b (tips with replies, being built). Him rushing on his own: b (see Q3). 4.2: c | S |
| B: carved A, banked fire | 1.3 Bridge Post, Cold Fire (lines exist) | a (`encounter_won` / `visit` + scene; check that the Poachers' Fire pickup fires `visit`, else make it a site like 1.3's Cold Fire) | W, but it is recognition, the strongest kind of W |
| C: the gap | the rest of Chapter 1: the hero fights without his best stack | the line: b (`hero_rushes`, being built); the difficulty: a (already true) | S |

## 4. For umm-engine (not faked in data)
1. Battle banter with a reply (speaker, then a second speaker), plus the when-keys `companion_rushes` / `hero_rushes`. This is in progress.
2. Reckless Aldren: either AI-controlled while he rides with you, or a when-key for "moved ahead of the line" (Q3).
3. A when-key for "a tree obstacle is broken" (the Thicket Dogs line). Trees already have hit points, and any stack can attack them.
4. 4.2: an enemy stack loses its first turn under a story condition (far future).
5. Data side, no engine change: `choice.toast` goes, and `choice.sword` = take | leave is recorded automatically. The docstring's CHOICES list and the 1.1 quest text change.

## 5. Open questions for the user
1. **Ushari's recognition:** the red hand on his shield, his face, or only the sword?
2. **4.2 if the sword was left:** what does zombie Aldren know for that one moment? (a) the letter you
   still carry, (b) nothing, so the fight is harder (respect costs you), or (c) Drest's dying hand-over
   in chapter 4 puts the sword on your hip before 4.2 anyway.
3. **Aldren in battle:** does he act on his own (truly reckless, and the player cannot stop him), or
   does the player move him and the banter only reacts when they rush him?
4. **Weight:** do you want the Red Banner's "Hand over the sword" option, and the Broken Camp line
   that ties the sword to Ushari's dead? Both make the choice heavier. The second may be too heavy.
5. **Balance:** how much stronger is the sword than nothing, and should the hunters' fights pay XP
   and spoils (so taking it is a gamble) or pay little (so it is only a cost)?
