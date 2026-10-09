# Story conditions

Every story decision asks one question: does this hold right now? There is one
way to ask it, a **condition tree**, and one evaluator (`src/adventure/Condition.h`).
New story data uses `if`; do not add single-purpose fields like `requires_item`.

## Where a condition goes

| Place | Shape | Effect |
|---|---|---|
| Trigger | `{"id", "when", "if": cond, "do": [...]}` | Fires only while `cond` holds; checked each time the event comes up |
| Option, hidden | `{"id", "label", "if": cond, "then": [...]}` | Offered only while `cond` holds |
| Option, locked | `{"id", "label", "requires": cond, "locked_text": "..."}` | Shown but locked until `cond` holds |
| Action branch | `{"if": cond, "then": [...], "else": [...]}` | Runs one branch; carries no other keys |

Every tree is checked when the map loads. A malformed one stops the load with
its path, e.g. `triggers[3].if.all[1]: unknown condition 'flg'`.

## Combinators

| Shape | Holds when |
|---|---|
| `{"all": [c, ...]}` | every one holds (an empty list holds) |
| `{"any": [c, ...]}` | at least one holds |
| `{"not": c}` | `c` does not hold |
| `true` / `false` | always / never |

## Leaves

| Leaf | Holds when |
|---|---|
| `{"flag": id}` | the story flag is set and truthy |
| `{"flag": id, "is": value}` | the flag equals `value`; every choice records `choice.<id>` |
| `{"item": id}` | the hero carries that item or keepsake |
| `{"companion": id}` | that companion is in the party |
| `{"companion": id, "present": true}` | ... and travelling: not wounded, not stationed |
| `{"quest": id, "state": "active"\|"done"}` | the quest is open / completed |
| `{"cleared": name}` | that guard camp was beaten or has left the map |
| `{"lore": title}` | that codex entry has been read |
| `{"trigger_fired": id}` | that trigger is used up (use it for "visited": fire a trigger on the visit) |
| `{"trigger_fired": id, "days": {">=": n}}` | ... at least `n` days ago |
| `{"level": {">=": n}}` | commander level |
| `{"day": {">=": n}}` | the current day |
| `{"resource": "Gold", ">=": n}` | treasury, any resource |

Comparisons take exactly one of `>=`, `>`, `<=`, `<`, `==` with an integer.

## Examples

Hand the Northern Warrior the dog tag (option shown only while you carry it):

```json
{"id": "give_tag", "label": "Hand him the dog tag", "if": {"item": "dog_tag"},
 "then": [{"take_item": "dog_tag"}, {"say": [["Northern Warrior", "Drusk... Grath vorrn uthk."]]},
          {"withdraw": "Northern Warrior"}]}
```

A line only if Drest is with you and you spared the warrior:

```json
{"if": {"all": [{"companion": "drest"}, {"flag": "choice.warrior", "is": "give_tag"}]},
 "then": [{"say": [["Drest", "You let him go. Good."]]}],
 "else": []}
```

## Older spellings

These still work and are translated into the same trees (`Scenario::compile`):
`when.after` / `when.delay`, `when.flag`, `when.unless_flag`, `requires_flag`,
`requires_item`, `requires_lore`, `requires_route`. `when.unless` and `when.wait`
are story moves, not conditions, and stay as they are.
