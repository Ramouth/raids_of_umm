#pragma once
#include <string>
#include <vector>
#include <unordered_map>

/*
 * SCDef — static definition for a Special Character's progression.
 *
 * Pure data — no methods, no GL, no SDL.
 *
 * ── Tree system ───────────────────────────────────────────────────────────────
 *
 * Each SC has a vector of SCLevelNodes. A node fires when the SC reaches that
 * level (and passes the requiresBranch gate). Nodes are either:
 *
 *   auto-apply   — autoEffects fire unconditionally (requiresBranch may narrow this)
 *   choice point — choices is non-empty; engine emits ScChoicePending and waits
 *                  for the player to pick one BranchOption before continuing
 *
 * NodeEffect uses an open string `type` — new effect kinds can be added in
 * data without touching engine code. The `key` and `value` fields cover all
 * current types; unusual effects can overload them creatively without a struct
 * change.
 *
 *   type "stat_mod"  — key=stat name ("attack","defense","hp","move",...), value=delta
 *   type "unlock"    — key=action key (matches ACTION_REGISTRY in sc_defs.py)
 *   type "passive"   — key=trigger name ("start_turn","on_kill",...),      value=amount
 *   type "aura_mod"  — key=aura field  ("radius","def_bonus"),             value=delta
 *
 * Branch choices are recorded at runtime on SpecialCharacter::chosenBranches,
 * keyed by the level at which the choice was made. Multiple choice points per
 * SC are fully supported.
 *
 * ── Backward compat ───────────────────────────────────────────────────────────
 *
 * The flat attackGrowth/defenseGrowth/maxHpGrowth/speedGrowth fields are still
 * read by CombatEngine for the existing XP/level system. They will be retired
 * once the choice-UI and tree-driven level-up are wired end-to-end.
 */

// ── Node effect ───────────────────────────────────────────────────────────────
//
// Open `type` string: engine dispatches by string, so new effect types
// never require a struct or enum change.

struct NodeEffect {
    std::string type;   // "stat_mod" | "unlock" | "passive" | "aura_mod" | ...
    std::string key;    // stat name / action key / trigger / aura field
    int         value = 0;
};

// Convenience factories — preferred over direct construction.
inline NodeEffect neStat  (const std::string& stat,   int delta)  { return {"stat_mod",  stat,    delta};  }
inline NodeEffect neUnlock(const std::string& action)              { return {"unlock",     action,  0};      }
inline NodeEffect nePassive(const std::string& trigger, int v)     { return {"passive",    trigger, v};      }
inline NodeEffect neAura  (const std::string& field,  int delta)   { return {"aura_mod",   field,   delta};  }

// ── Branch option ─────────────────────────────────────────────────────────────
//
// One option at a choice point. Player picks exactly one; the id is stored in
// SpecialCharacter::chosenBranches[level] for gate checks at higher levels.

struct BranchOption {
    std::string             id;           // "duelist", "berserker", "pyromancer", ...
    std::string             name;
    std::string             description;
    std::vector<NodeEffect> effects;      // applied immediately when this branch is chosen
};

// ── Level node ────────────────────────────────────────────────────────────────

struct SCLevelNode {
    int                       level;
    std::string               requiresBranch; // "" = always; else must match chosenBranches value
    std::vector<NodeEffect>   autoEffects;    // fire when node applies (choice already resolved)
    std::vector<BranchOption> choices;        // non-empty → player must choose before combat resumes
};

// ── SC ability def (legacy display) ──────────────────────────────────────────
// Still used by PartyState for text display until tree UI lands.

struct SCAbilityDef {
    int         level;
    std::string key;
    std::string name;
    std::string description;
};

// ── SC definition ─────────────────────────────────────────────────────────────

struct SCDef {
    std::string id;
    std::string name;
    std::string flavour;
    std::string archetype;   // "warrior" | "mage" | "support" | "commander"

    int maxLevel;
    std::vector<int> xpThresholds;  // cumulative; len = maxLevel - 1

    int perTurnXp   = 0;
    int killBonusXp = 0;
    int levelUpHeal = 0;

    // ── Flat growth (backward compat — still read by CombatEngine) ────────────
    // These will be retired once tree-driven level-up is wired end-to-end.
    int attackGrowth  = 0;
    int defenseGrowth = 0;
    int maxHpGrowth   = 0;
    int speedGrowth   = 0;

    // ── Skill tree (new) ──────────────────────────────────────────────────────
    // Nodes evaluated in level order when an SC levels up.
    // requiresBranch == "" nodes always apply; gated nodes only apply if the
    // SC's chosenBranches map matches the gate string.
    std::vector<SCLevelNode> tree;

    // ── Legacy ability list (display only) ───────────────────────────────────
    std::vector<SCAbilityDef> abilities;
};

// ── Hardcoded SC definitions ──────────────────────────────────────────────────
// Will move to data/sc_defs.json once the tree UI is wired.

inline const SCDef& ushariDef() {
    static const SCDef d = [] {
        SCDef s;
        s.id        = "ushari";
        s.name      = "Ushari";
        s.flavour   = "Blade Dancer of the Crimson Dunes. She does not wait for enemies to come to her.";
        s.archetype = "warrior";
        s.maxLevel  = 3;
        s.xpThresholds = { 15, 35 };
        s.perTurnXp    = 2;
        s.killBonusXp  = 6;
        s.levelUpHeal  = 15;
        // Flat growth (CombatEngine compat)
        s.attackGrowth = 1;
        s.maxHpGrowth  = 8;
        // Skill tree
        s.tree = {
            // Level 2 — choose a fighting style
            {
                2, "",
                {},  // no auto effects
                {
                    {
                        "duelist",
                        "Duelist",
                        "Precision over power. Unlocks Double Strike — attack your target and one adjacent enemy.",
                        { neUnlock("double_strike"), neStat("attack", 1) }
                    },
                    {
                        "berserker",
                        "Berserker",
                        "Abandon finesse. Raw offensive power — no repositioning tricks, just damage.",
                        { neStat("attack", 3) }
                    },
                }
            },
            // Level 3 — duelist branch payoff
            {
                3, "duelist",
                { neUnlock("sweeping_arc"), neStat("hp", 8) },
                {}
            },
            // Level 3 — berserker branch payoff
            {
                3, "berserker",
                { neUnlock("bloodlust"), neStat("attack", 2), neStat("hp", 4) },
                {}
            },
        };
        // Legacy display
        s.abilities = {
            { 2, "double_strike", "Double Strike", "Attack your target and one adjacent enemy." },
            { 3, "sweeping_arc",  "Sweeping Arc",  "Strike every enemy in the ring around you." },
        };
        return s;
    }();
    return d;
}

inline const SCDef& khetDef() {
    static const SCDef d = [] {
        SCDef s;
        s.id        = "khet";
        s.name      = "Khet";
        s.flavour   = "Sand Warden. She does not fight the desert — she IS the desert.";
        s.archetype = "support";
        s.maxLevel  = 3;
        s.xpThresholds = { 12, 28 };
        s.perTurnXp    = 2;
        s.killBonusXp  = 6;
        s.levelUpHeal  = 12;
        // Flat growth
        s.defenseGrowth = 1;
        s.maxHpGrowth   = 6;
        // Skill tree
        s.tree = {
            // Level 2 — choose a support style
            {
                2, "",
                {},
                {
                    {
                        "warden",
                        "Warden",
                        "Guardian of allies. Unlocks Mend — restore HP to an adjacent friendly unit.",
                        { neUnlock("mend"), neStat("defense", 1) }
                    },
                    {
                        "geomancer",
                        "Geomancer",
                        "Reshape the battlefield. Unlocks Quicksand — mark a hex as difficult terrain.",
                        { neUnlock("quicksand"), neStat("hp", 6) }
                    },
                }
            },
            // Level 3 — warden branch payoff
            {
                3, "warden",
                { neUnlock("ward"), neStat("defense", 1) },
                {}
            },
            // Level 3 — geomancer branch payoff
            {
                3, "geomancer",
                { neUnlock("stone_wall"), neStat("hp", 6) },
                {}
            },
        };
        // Legacy display
        s.abilities = {
            { 2, "mend",      "Mend",       "Restore HP to an adjacent friendly unit." },
            { 2, "quicksand", "Quicksand",  "Mark a hex as difficult terrain." },
            { 3, "ward",      "Ward",       "Seal a hex — no unit may enter it." },
        };
        return s;
    }();
    return d;
}

inline const SCDef& sekharaDef() {
    static const SCDef d = [] {
        SCDef s;
        s.id        = "sekhara";
        s.name      = "Sekhara";
        s.flavour   = "Flame Architect. Every wall she raises is a question: how will you answer it?";
        s.archetype = "mage";
        s.maxLevel  = 3;
        s.xpThresholds = { 12, 26 };
        s.perTurnXp    = 2;
        s.killBonusXp  = 6;
        s.levelUpHeal  = 10;
        // Flat growth
        s.maxHpGrowth = 5;
        // Skill tree
        s.tree = {
            // Level 1 — mages always get a basic AoE
            {
                1, "",
                { neUnlock("aoe_strike") },
                {}
            },
            // Level 2 — choose a school of magic
            {
                2, "",
                {},
                {
                    {
                        "pyromancer",
                        "Pyromancer",
                        "Area denial through fire. Unlocks Firewall — a line of burning hexes.",
                        { neUnlock("firewall"), neStat("hp", 5) }
                    },
                    {
                        "displacer",
                        "Displacer",
                        "Control the battlefield through force. Unlocks Push — knock enemies away.",
                        { neUnlock("push"), neStat("hp", 5) }
                    },
                }
            },
            // Level 3 — pyromancer branch payoff
            {
                3, "pyromancer",
                { neStat("aoe_radius", 1), neUnlock("firewall_extended") },
                {}
            },
            // Level 3 — displacer branch payoff
            {
                3, "displacer",
                { neUnlock("pull"), neStat("push_range", 1) },
                {}
            },
        };
        // Legacy display
        s.abilities = {
            { 1, "aoe_strike", "AoE Strike",  "Small area attack — always available." },
            { 2, "firewall",   "Firewall",     "Lay a 3-hex line of fire." },
            { 2, "push",       "Push",         "Knock an enemy away." },
        };
        return s;
    }();
    return d;
}

// Returns a pointer to the matching SCDef, or nullptr if unknown.
inline const SCDef* findSCDef(const std::string& id) {
    if (id == "ushari")  return &ushariDef();
    if (id == "khet")    return &khetDef();
    if (id == "sekhara") return &sekharaDef();
    return nullptr;
}
