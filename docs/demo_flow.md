# Demo flow and player understanding

> **Archived (2026-10-09).** This describes the old "Old Passage" demo (wolves, the hooded druid, Corvin, Kaldur). It is kept for its method and history. The current story is Chapter 1 "The Marches": its single source is the header of `scripts/chapter1_story.py`, published as the "Chapter 1 — The Marches" tab of the story doc. Factions: `Factions.md`.

At every step ask: **Does the player understand enough context and plot to act?**
The required information must appear in dialogue, objectives or the map before it is needed. Author notes do not count as player knowledge. Mystery may conceal Aldren's fate; it must not conceal the next actionable objective.

| Beat | What the player should understand | Action and supporting cue |
| --- | --- | --- |
| Varenhold opening | You are a Varen commander. Aldren's guest cannot reach the castle because wolves block the road. | The opening quest names all five packs. Clear the packs. |
| Hooded druid | Someone knows why Aldren went east. The old passage may contain a danger. His identity and intentions are unresolved. | His warning comes before the Hermit's Wolves battle; his disappearance follows victory. No separate druid hunt is required. |
| Ushari arrives | She expected Aldren to wait. He is your missing brother; your father wants him home. Ushari belongs to the last witch tribe and has a personal stake. | The steward explicitly asks you to end the day after the wolves. Her night conversation introduces the search; the journal points to Corvin at Hallowmere. |
| Coldwater crossing | Hallowmere lies beyond the bridge; brigands obstruct the crossing. | The scout identifies the brigands. Inscriptions copied before Ushari arrives are retained for her translation. |
| Corvin's offer | He has a useful lead and supplies. He also wants you to take his old rival's castle. Conquest is optional. | His shepherds place Aldren's camp beyond Greyfang. He names the northern pass and Troll Gate alternative, offers supplies and soldiers, and warns that Rook will mobilise now and may cross west in two days. |
| Three inscriptions | These are original place names on old road charts, and evidence that the families rewrote the land's history. | Ushari names Varenhold's wall, Coldwater bridge and the assembly court south of Hallowmere. Translations enter the journal. All three are required, even if the player knows the puzzle answer. |
| House Rook's border | This is an armed incursion into a hostile house's territory. Rook accuses Corvin of using your search to pursue Hale claims. | Edric's messenger explains his grievance before the passage decision. The choice warns that his promise may not protect Ushari. Asking schedules a meeting; it does not immediately teleport enemies into battle. |
| Promised escort, optional | The meeting is at Greyfang Watch. The promise is a gamble. The captain's racism and throne claim are revealed in the betrayal. | A map reveal and journal objective identify the meeting. On arrival the scout spots the encirclement, the captain threatens Ushari, and combat starts. Victory yields patrol orders and a route past the border posts. Retreat leaves surviving enemies in the field. |
| Choose an approach | A chart alone will not get an army past a hostile border. | Hold Rookwatch for a recruiting/supply base; clear Troll Gate for an unsupported southern track; or recover the escort's patrol orders. Each has a journal explanation. The castle remains optional. |
| Aldren's camp | His belongings and handwriting establish that he was here, without proving his present condition. | His note specifically names Kaldur lodge. The journal and reveal point there and repeat the inscriptions and approach requirements. |
| Kaldur survey | Aldren reached this surface lodge and corrected the bearings. Ishara is the house of names and Ushari seeks its records. This is a road chart, not the passage entrance. | Match the translated names. The puzzle explains missing prerequisites and allows leaving to explore and returning later. The camp visit and three readings prevent an accidental early ending. |
| Departure decision | You have a definite itinerary through House Rook's wider domain. You have not found Aldren or entered the passage. | Report home and provision with Ushari, or continue together immediately. The home ending reveals the riderless horse and blood; the road ending does not give the party that knowledge. Both retain Ushari. |
| Chapter handoff | Rookwatch is one border castle. Greyfen remains House Rook's domain even if the castle falls. | Both endings explicitly place another above-ground journey ahead. Save the approach, supply base, patrol information and political choices for the planned next level. |

Corvin's ambiguity concerns the proposed annexation and his own benefit. The escort's persecution is unequivocal; it does not prove his family's historic land claim. Aldren's later condition remains author-only knowledge in the plot document.

## Verification and limits

`godot/native/build/demo_bot . <conquest|bypass|slow|escort> <seed>` uses the production starting army, movement, town economy, recruitment, companion rules and actual CombatEngine outcomes. It handles intermediate conversations and the survey instead of stopping at the first choice. The slow case adds fourteen opening days before proceeding. No resources, victories or map-position teleports are granted.

These are scripted, omniscient routes, not blind human playtests. The seed controls scenario setup, not all combat randomness. Route completion establishes mechanical feasibility; it does not establish that a new player understands the cues or enjoys the pacing. Dedicated siege walls, the next level and its playable consequences remain future work.

Native regressions separately exercise missing inscriptions, missing approaches, both endings, saved choices, finite escort troops, delayed mobilisation and enemy casualties after retreat. Godot smoke tests exercise the actual puzzle, ending presentation and escort dialogue-to-combat handoff.

Final local sample (29 September 2026):

| Route | Setup seed | Completed | Saved approach |
| --- | --- | --- | --- |
| Conquest, including Edric's field army | 2 | Day 20 | `castle_road` |
| Southern bypass | 1 | Day 21 | `southern_track` |
| Escort betrayal and patrol orders | 3 | Day 18 | `patrol_cut` |
| Slow opening, fourteen extra days, report-home ending | 2 | Day 33 | `southern_track` |

All three native CTest suites and the Godot `demo_smoke`, `expanded_demo_smoke` and `rookwatch_smoke` checks passed. Mandatory Greyfang Pass, Troll Gate, the camp's Troll Warband and Kaldur guards were reduced after the initial runs exposed recruitment waits lasting many weeks. The optional treant grove retains its stronger army; the conquest run approaches by Greyfang Watch and Aldren's camp. Taking an undefended castle and leaving Edric alive allowed a recapture in an earlier run, so the completed conquest explicitly defeats his field army as well.
