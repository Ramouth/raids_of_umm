# Raids of Umm'Natur — Factions

Factions are tied to **land**. Each land shapes how its people fight, and each
land has one **school of magic**. The campaign is an odyssey through these
lands, so the player meets each people where they live.

Status key: **Decided** = the user has decided it. *Proposal* = written by
Claude, waiting for the user. The full proposal, with open questions, is the
Claude Doc "Lands, Factions and Magic: Proposal"
(https://claude.ai/artifact/4zRccMgeP9qSFdoUPD7naU).

The previous six-faction design (Pha'raxh, the Crown, the Eye of Djangjix) is
archived in `docs/archive/factions_six_2026-10-07.md`. Its parts are folded into
the lands below.

## The four lands (Decided, 2026-10-08)

| Land | People | How they fight | School | Where on the journey |
|---|---|---|---|---|
| **Lush** (player) | The Cruths: painted, Pict-like warbands led by British nobles (House Varen) | Alone, fast, hard-hitting, little armour | Growth | The Marches, chapter 1: home |
| **Ice** | The northern people (unnamed; placeholder "rime-folk", faction id `ice`) | Hold ground: heavy armour, slow, they let you come | Frost | They come south into the Marches; first met on map 1.2 |
| **Desert** | *Proposal:* Medjay-like tomb watchers, the djinn, and Ushari's people (the tribe that ruled before the families) | Positions: phalanx, flanking, oaths, terrain | Sand | Chapter 3 |
| **Below** | The Veined | Consume: grow from the dead | Below | The Under-road (ch 2) and the dark earth inside the walls (ch 4) |

The pairs are opposites: Lush attacks and Ice holds; Desert fights by position
and Below by consuming.

## Lush — the Cruths (Decided)

The player's people. Green vale, orchards, deep woods. Small fast warbands that
strike and go home. Units in data: `woad_runner`, `cruth_slinger`,
`painted_blade` (Cruth); `gododdin_rider`, `teulu` (British nobles).
Abilities in use: `lone_wolf`, `strike_and_return`, `renown` / `great_renown`.
*Proposal:* the old Sylvan Host's groves fold in here.

## Ice — the northern people (Decided: they come to our land)

A hard land where nothing grows fast. On map 1.2 the player finds their traces
(snow at the north edge, an empty camp with dog-pens and sled tracks, a
studded collar no one in the Marches makes) and meets a Northern Warrior
searching for his dog. They speak only their own tongue; nothing is translated.
Unit in data: `northern_warrior` (tough, slow, high defence).
*Proposal:* their land is failing as the ice grows, so they come as settlers
with spears rather than raiders; they are the old builders who raised the walls
of the walled nation (from the old Iron Conclave).

## Desert (Proposal)

Medjay-like watchers (from the old Shariw) who guard the tombs, the djinn
(from the old Djinn Courts), and Ushari's people, who ruled the land before the
families and read the prophecy as their own good sign. Hellenic flavour:
phalanx, flanking bonus, oaths (data faction `symmachy`). Ushari's tribe has
no name yet.

## Below — the Veined (Decided)

The power under Pha'raxh: they ran the desert empire as a front and feed on the
dead (Ammit, the devourer). Home: the Under-road and the dark earth the walls
keep in. The Below school is offered by the road, never chosen: it is the
temptation (Vein Sight, Grave Harvest, Listen Below, The Door).

## Magic follows the lands (Decided, 2026-10-08)

One school per land. Your land's school is native; the others are learned on
the road. No mana: a battle spell works once per battle, a map spell once per
day. Every school has a price (*Proposal*): Growth is slow; Frost chills your
own stacks beside it; Sand needs open ground; Below turns companions against
you. The ability and status system that carries this is not built yet; see
`docs/abilities_inventory.md` and `docs/logbook/2026-10-08-magic-system-needed.md`.

## Where the old six went

| Old faction | Goes to |
|---|---|
| The Veined | Below |
| Sylvan Host | Lush (*proposal*) |
| Ivory Compact (Hellenic, `symmachy`) | Desert, as Ushari's people (*proposal*) |
| Djinn Courts | Desert (*proposal*) |
| Shariw | Desert, as the Medjay tomb-watchers (*proposal*) |
| Iron Conclave | Ice, as the wall builders (*proposal*) |

The names "Ivory Compact" and "Iron Conclave" are retired.
