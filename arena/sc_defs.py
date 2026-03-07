# sc_defs.py — Special Character definitions
# Data only. No engine logic here.
# Run arena.py to test; this module is imported by the combat engine.

from dataclasses import dataclass, field
from enum import Enum, auto


# ── Effect kind ───────────────────────────────────────────────────────────────

class EffectKind(Enum):
    STAT_MOD  = auto()   # permanent stat bump applied on level-up
    PASSIVE   = auto()   # per-turn trigger added to SC's passive list
    ON_EVENT  = auto()   # fires on a named combat event
    UNLOCK    = auto()   # grants access to a named action


# ── Ability effect ─────────────────────────────────────────────────────────────

@dataclass
class AbilityEffect:
    kind:       EffectKind

    # STAT_MOD
    stat:       str = ""    # "attack" | "defence" | "max_hp" | "move"
    delta:      int = 0

    # PASSIVE / ON_EVENT
    trigger:    str = ""    # "start_turn" | "on_kill" | "on_hit_taken"
    value:      int = 0     # heal amount, damage reduction, etc.

    # UNLOCK
    action_key: str = ""    # key into ACTION_REGISTRY


# ── Action definition ─────────────────────────────────────────────────────────
# An action is something the SC can DO in combat once unlocked.

@dataclass
class ActionDef:
    key:         str
    name:        str
    description: str
    cooldown:    int         # turns between uses; 0 = every turn
    targeting:   str         # "self" | "unit_enemy" | "unit_friendly" | "hex"
    range:       int         # hex distance; 1 = adjacent only
    shape:       str         # "single" | "arc" | "ring" | "line"
    size:        int = 1     # arc/ring radius or line length
    terrain:     str = ""    # "" | "difficult" | "impassable" | "hazard"
    duration:    int = 0     # turns terrain lasts; 0 = permanent
    damage:      int = 0     # damage dealt to units in area (hazard entry, AoE)
    heal:        int = 0     # hp restored to friendly targets


# ── Level ability ─────────────────────────────────────────────────────────────

@dataclass
class LevelAbility:
    level:       int
    name:        str
    description: str
    effects:     list[AbilityEffect] = field(default_factory=list)


# ── SC definition ─────────────────────────────────────────────────────────────

@dataclass
class SCDefinition:
    name:           str
    flavour:        str
    max_level:      int
    xp_thresholds:  list[int]          # len == max_level - 1; cumulative XP to reach each level
    per_turn_xp:    int
    assist_xp:      int
    kill_bonus_xp:  int
    level_up_heal:  int                # flat HP restored on level-up
    base_stats:     dict               # attack, defence, max_hp, move, initiative
    stat_growth:    dict               # deltas applied automatically on every level-up
    abilities:      list[LevelAbility] # indexed by level they unlock at
    default_army:   list[tuple[str, int]]  # [("Skeleton Warrior", 3), ...]


# ── Action registry ───────────────────────────────────────────────────────────
# All unlockable actions live here. Engine looks them up by key.

ACTION_REGISTRY: dict[str, ActionDef] = {

    # Ushari
    "double_strike": ActionDef(
        key         = "double_strike",
        name        = "Double Strike",
        description = "Attack your target and one adjacent enemy.",
        cooldown    = 1,
        targeting   = "unit_enemy",
        range       = 1,
        shape       = "arc",
        size        = 2,      # target hex + 1 neighbour of your choice
        damage      = 0,      # uses SC base attack stat
    ),
    "sweeping_arc": ActionDef(
        key         = "sweeping_arc",
        name        = "Sweeping Arc",
        description = "Strike every enemy in the ring around you.",
        cooldown    = 2,
        targeting   = "self",
        range       = 1,
        shape       = "ring",
        size        = 1,      # full radius-1 ring
        damage      = 0,      # uses SC base attack stat
    ),

    # Khet
    "mend": ActionDef(
        key         = "mend",
        name        = "Mend",
        description = "Restore HP to an adjacent friendly unit.",
        cooldown    = 1,
        targeting   = "unit_friendly",
        range       = 1,
        shape       = "single",
        heal        = 12,
    ),
    "quicksand": ActionDef(
        key         = "quicksand",
        name        = "Quicksand",
        description = "Mark a hex as difficult terrain (+1 move cost to enter).",
        cooldown    = 2,
        targeting   = "hex",
        range       = 3,
        shape       = "single",
        terrain     = "difficult",
        duration    = 0,      # permanent until dispelled
    ),
    "ward": ActionDef(
        key         = "ward",
        name        = "Ward",
        description = "Seal a hex — no unit may enter it.",
        cooldown    = 3,
        targeting   = "hex",
        range       = 2,
        shape       = "single",
        terrain     = "impassable",
        duration    = 4,      # lasts 4 turns then collapses
    ),

    # Sekhara
    "displace": ActionDef(
        key         = "displace",
        name        = "Displace",
        description = "Push or pull an enemy up to 2 hexes.",
        cooldown    = 2,
        targeting   = "unit_enemy",
        range       = 4,
        shape       = "single",
        size        = 2,      # steps pushed/pulled
    ),
    "firewall": ActionDef(
        key         = "firewall",
        name        = "Firewall",
        description = "Lay a 3-hex line of fire. Units may cross but take damage.",
        cooldown    = 3,
        targeting   = "hex",
        range       = 3,
        shape       = "line",
        size        = 3,
        terrain     = "hazard",
        duration    = 3,
        damage      = 8,      # taken on entry
    ),
    "stone_wall": ActionDef(
        key         = "stone_wall",
        name        = "Stone Wall",
        description = "Raise a 2-hex impassable wall. Must be walked around.",
        cooldown    = 4,
        targeting   = "hex",
        range       = 2,
        shape       = "line",
        size        = 2,
        terrain     = "impassable",
        duration    = 5,
    ),
}


# ── SC roster ─────────────────────────────────────────────────────────────────

USHARI = SCDefinition(
    name    = "Ushari",
    flavour = "Blade Dancer of the Crimson Dunes. She does not wait for enemies to come to her.",
    max_level      = 3,
    xp_thresholds  = [15, 35],   # reach 15 XP → level 2; reach 35 → level 3
    per_turn_xp    = 2,
    assist_xp      = 4,
    kill_bonus_xp  = 6,
    level_up_heal  = 15,
    base_stats  = {"attack": 4, "defence": 2, "max_hp": 35, "move": 3, "initiative": 7},
    stat_growth = {"attack": 1, "max_hp": 8},
    abilities   = [
        LevelAbility(
            level       = 2,
            name        = "Double Strike",
            description = "Unlocks the Double Strike action.",
            effects     = [AbilityEffect(kind=EffectKind.UNLOCK, action_key="double_strike")],
        ),
        LevelAbility(
            level       = 3,
            name        = "Sweeping Arc",
            description = "Unlocks the Sweeping Arc action.",
            effects     = [AbilityEffect(kind=EffectKind.UNLOCK, action_key="sweeping_arc")],
        ),
    ],
    default_army = [("Skeleton Warrior", 4), ("Sand Golem", 1)],
)

KHET = SCDefinition(
    name    = "Khet",
    flavour = "Sand Warden. She does not fight the desert — she IS the desert.",
    max_level      = 4,
    xp_thresholds  = [12, 28, 50],
    per_turn_xp    = 2,
    assist_xp      = 4,
    kill_bonus_xp  = 6,
    level_up_heal  = 12,
    base_stats  = {"attack": 2, "defence": 3, "max_hp": 30, "move": 2, "initiative": 5},
    stat_growth = {"defence": 1, "max_hp": 6},
    abilities   = [
        LevelAbility(
            level       = 2,
            name        = "Mend & Quicksand",
            description = "Unlocks Mend and Quicksand.",
            effects     = [
                AbilityEffect(kind=EffectKind.UNLOCK, action_key="mend"),
                AbilityEffect(kind=EffectKind.UNLOCK, action_key="quicksand"),
            ],
        ),
        LevelAbility(
            level       = 3,
            name        = "Warden's Focus",
            description = "Mend range extends to 2 hexes.",  # engine reads this as a flag
            effects     = [AbilityEffect(kind=EffectKind.STAT_MOD, stat="mend_range", delta=1)],
        ),
        LevelAbility(
            level       = 4,
            name        = "Ward",
            description = "Unlocks Ward. Also gains passive HP regen.",
            effects     = [
                AbilityEffect(kind=EffectKind.UNLOCK, action_key="ward"),
                AbilityEffect(kind=EffectKind.PASSIVE, trigger="start_turn", value=3),
            ],
        ),
    ],
    default_army = [("Skeleton Warrior", 3), ("Sand Golem", 2)],
)

SEKHARA = SCDefinition(
    name    = "Sekhara",
    flavour = "Flame Architect. Every wall she raises is a question: how will you answer it?",
    max_level      = 5,
    xp_thresholds  = [12, 26, 46, 72],
    per_turn_xp    = 2,
    assist_xp      = 4,
    kill_bonus_xp  = 6,
    level_up_heal  = 10,
    base_stats  = {"attack": 2, "defence": 1, "max_hp": 22, "move": 3, "initiative": 8},
    stat_growth = {"max_hp": 5},
    abilities   = [
        LevelAbility(
            level       = 2,
            name        = "Displace",
            description = "Unlocks Displace — push or pull an enemy.",
            effects     = [AbilityEffect(kind=EffectKind.UNLOCK, action_key="displace")],
        ),
        LevelAbility(
            level       = 3,
            name        = "Firewall",
            description = "Unlocks Firewall — a 3-hex hazard line.",
            effects     = [AbilityEffect(kind=EffectKind.UNLOCK, action_key="firewall")],
        ),
        LevelAbility(
            level       = 4,
            name        = "Stone Wall",
            description = "Unlocks Stone Wall — 2-hex impassable barrier.",
            effects     = [AbilityEffect(kind=EffectKind.UNLOCK, action_key="stone_wall")],
        ),
        LevelAbility(
            level       = 5,
            name        = "Architect's Eye",
            description = "Displace range increases by 2.",
            effects     = [AbilityEffect(kind=EffectKind.STAT_MOD, stat="displace_range", delta=2)],
        ),
    ],
    default_army = [("Skeleton Warrior", 2), ("Dune Serpent", 1)],
)


SC_ROSTER: list[SCDefinition] = [USHARI, KHET, SEKHARA]
