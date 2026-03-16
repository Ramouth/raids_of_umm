# Raids of Umm'Natur — Factions

> **Design principle:** Three factions, each with a distinct economic identity, a unique unit roster,
> and a different answer to the question *"what does winning look like for us?"*
> Multiplayer mirrors Warcraft 3 — you can win by outlasting everyone, or by completing the
> world objective before they can stop you.

---

## The World Objective — Eye of Djangjix

**Djangjix** is the oldest and greatest Djinn in the known world — neither villain nor ally,
but a force of nature with a price. Millennia ago he shattered his own essence into the
*Eye of Djangjix*, a crystalline artifact of pure elemental will, and scattered the shards
across the dungeons of Umm'Natur to prevent any single faction from claiming it.

Whoever assembles the Eye and carries it to Djangjix's sealed tomb at the heart of the desert
wins his pact — and dominion over Umm'Natur.

**This creates two viable win conditions in every game:**

| Win Condition | Description |
|---------------|-------------|
| **Military** | Destroy all rival factions' towns and heroes |
| **Pact of Djangjix** | Assemble all Eye shards from dungeons and deliver to the Tomb |

The Pact win is faster but requires pushing deep into dungeons while enemies pressure you.
The Military win is safer but slower — factions are designed so neither is dominant.

---

## Faction 1 — The Scarab Lords

> *"The desert endures. So do we."*

**Identity:** The living human kingdoms of Umm'Natur. Disciplined armies, trade networks,
and the oldest gold-hoarding tradition on the continent. They fight with steel and bow —
no magic, no necromancy, no shortcuts.

**Colour / aesthetic:** Sandy gold, deep earth tones. Banners of the scarab beetle.
Burnished bronze armour, obsidian arrow tips.

**Playstyle:** Balanced economy, strong mid-tier units, best riders. Favours map control
and resource denial. The "human" faction — highest unit counts, lowest individual power.

**Economy emphasis:** Gold + Amber

**Unit roster:**

| Unit | Tier | Attack Type | Role |
|------|------|-------------|------|
| Desert Archer | 2 | Piercing | Ranged — opens every fight |
| Armoured Warrior | 3 | Physical | Front line, high defence |
| Rider Archer | 4 | Piercing | Fast ranged, flanking |
| Rider Knight | 5 | Physical | Heavy cavalry charge |
| Rider Lance | 5 | Piercing | Elite mounted pierce |
| Ancient Guardian | 6 | Physical | Siege wall, cannot be moved |

**Special Characters:**
- **Ushari** — Warrior archetype. Aura buffs adjacent physical units.
- **Kharim** — Strategist archetype. Commander who increases rider speed and flanking radius.
- *(TBD)* — Merchant archetype. Economy SC; builds mine income, reduces unit cost.

---

## Faction 2 — The Tomb Dynasties

> *"Death is not the end. It is promotion."*

**Identity:** The undead empire of the buried pharaohs. They cannot be exhausted — fallen
soldiers rise again. Their cities are pyramid tombs; their economy runs on BoneDust and
relic magic. They lose early but become exponentially stronger as the game goes on.

**Colour / aesthetic:** Bone white, void purple, tarnished gold. Wrapping bandages,
cracked stone, necromantic glow seeping from the ground.

**Playstyle:** Attrition. The Dynasties lose early skirmishes but the `raise_dead` ability
on high-tier units means their army doesn't deplete. Lean into dungeon control — every
dungeon they clear adds undead to their roster.

**Economy emphasis:** BoneDust + AncientRelic

**Unit roster:**

| Unit | Tier | Attack Type | Role |
|------|------|-------------|------|
| Skeleton Warrior | 1 | Physical | Cannon fodder — cheap, replenishable |
| Mummy | 3 | Physical | Tank + `curse_strike` (reduces enemy ATK) |
| Sand Scorpion | 2 | Physical | Fast skirmisher, `poison_strike` DoT |
| Pharaoh Lich | 7 | Magical | Supreme caster; ranged magical, `raise_dead` |

**Special Characters:**
- **Sekhara** — Glass Cannon archetype. Channels nearby stack for magical burst.
- **Khet** — Support archetype. Sustain and economy. Passive income on BoneDust mines.
- *(TBD)* — Necromancer archetype. Field SC who converts killed enemies to skeleton_warrior mid-combat.

---

## Faction 3 — The Djinn Courts

> *"We were here before the sand. We will be here after."*

**Identity:** The elemental spirits of wind, sand, and storm. Smaller armies, immense
individual power. They move faster than any other faction and can cross terrain that
blocks others (desert storm tiles, rifts). Their connection to Djangjix gives them a
1-step advantage on the Pact objective — but factions know this, and target them early.

**Colour / aesthetic:** Teal and pale violet, lightning-white edges, translucent forms.
No physical armour — pure energy. Buildings shimmer with heat-haze.

**Playstyle:** Glass cannon faction. Fewest units, highest speed and damage.
Must leverage mobility and magical bypass. Loses wars of attrition, wins wars of timing.

**Economy emphasis:** DjinnEssence + SandCrystal

**Unit roster:**

| Unit | Tier | Attack Type | Role |
|------|------|-------------|------|
| Sand Wraith | 1 | Magical | Fast, fragile; `flying` |
| Djinn | 5 | Magical | Core unit; `flying`, `magic_resistance` |
| Storm Caller | 4 | Magical | Ranged magical, AoE lightning |
| *(TBD)* | 6 | Magical | Elite; possibly a mini-Djangjix avatar |

**Special Characters:**
- *(TBD)* — Elemental archetype. Djinn-bonded warrior who phases through ZoC.
- *(TBD)* — Seer archetype. Reveals fog of war ahead of movement; economic value via crystal prediction.
- **Djangjix** *(campaign-only)* — Not recruitable in multiplayer. Appears as a quest objective
  and campaign event trigger.

---

## Multiplayer Design Notes

### Win Condition Balance
The Pact objective needs to be roughly as fast as a military win, or players will ignore it.
Target: **first shard retrieval turn 15–20**, full assembly turn **35–45** on a standard map.
Military elimination on the same map: **30–50 turns** depending on aggression.

Tuning levers:
- Number of Eye shards (2–4 recommended)
- Distance of Djangjix's Tomb from starting positions
- Guard strength at each shard dungeon

### Faction Asymmetry Targets (to validate with combat heuristics)
| Match-up | Expected advantage |
|----------|-------------------|
| Scarab Lords vs Tomb Dynasties (early game) | Scarab Lords +15% |
| Scarab Lords vs Tomb Dynasties (late game) | Tomb Dynasties +20% |
| Djinn Courts vs either (open field) | Djinn Courts +10% per tier differential |
| Djinn Courts vs either (attrition) | Djinn Courts −30% |

These are design targets, not observed values. **Must be validated by combat simulation before release.**

---

## Campaign Outline

Three acts, each playable from one faction's perspective.
Each faction has 3–4 missions with unique objectives.

### Act I — The Shards Awaken
Djangjix's seals break. All three factions simultaneously detect the Eye's fragments.
- Scarab Lords mission: defend your gold mines while your scouts locate the first shard
- Tomb Dynasties mission: Sekhara leads a ritual to locate the shard via necromantic divination
- Djinn Courts mission: you already know — Djangjix whispered it to you

### Act II — The Race
Factions collide over dungeon control. Each dungeon tells a story (see Dungeon Narrative doc).
SC fear events begin here — entering certain tombs causes SCs to hesitate or refuse.

### Act III — The Pact
One faction holds all shards or has eliminated the others.
The Tomb of Djangjix is accessible. Final dungeon — no combat, pure SC-driven narrative.
The faction that arrives decides the fate of Umm'Natur.

---

## Open Questions (for design back-and-forth)

1. **Fourth faction?** Sand Wraiths (sandstorm creatures, chaos faction) or keep it at 3?
2. **SC fear mechanics**: which dungeons trigger it, and what's the in-game consequence
   (SC refuses to enter → hero army fights without them, or hero must go alone)?
3. **Multiplayer hero limit**: 1 hero per faction like early HoMM, or multiple heroes WC3-style?
4. **Artifact delivery**: does the delivering hero need to survive the final dungeon,
   or is it a town-action?
5. **Campaign branching**: linear (WC3) or choice-based (HOMM campaigns)?
