#pragma once
#include "entities/UnitType.h"
#include "entities/SCDef.h"
#include "hex/HexCoord.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * CombatUnit — one creature stack taking part in a battle.
 *
 * 'type' is stored by value here as a temporary measure until
 * ResourceManager exists.  Once it does, this becomes const UnitType*
 * pointing into the registry (no copy, no ownership).
 *
 * hpLeft tracks the HP of the leading creature only.
 * When hpLeft reaches 0, count decrements and hpLeft resets to type.hitPoints.
 * That logic lives in CombatEngine — this struct is pure data.
 */
struct CombatUnit {
    const UnitType* type  = nullptr; // points into ResourceManager registry — never owned
    int      count        = 0;   // creatures remaining in stack
    int      hpLeft       = 0;   // HP of the leading creature (1..type->hitPoints)
    bool     isPlayer     = false;
    bool     isDefending  = false;
    HexCoord pos;            // position on the combat grid (CombatMap coordinate space)
    int      shotsLeft    = 0;   // remaining ammo; 0 means melee-only
    bool     hasRetaliated = false; // true once this stack retaliates this round

    // SC progression — zero/nullptr for ordinary stacks.
    // Set by buildPlayerArmy() when the stack represents a SpecialCharacter.
    // CombatEngine updates scLevel/scXp/scUnlocked as XP is earned.
    // CombatState::onExit() reads these back into CombatOutcome::scUpdates.
    bool                     isSpecialCharacter = false;
    std::string              scId;             // matches SpecialCharacter::id
    const SCDef*             scDef    = nullptr;
    int                      scLevel  = 1;
    int                      scXp     = 0;
    int                      killXp   = 0;    // from scDef->killBonusXp
    int                      perTurnXp= 0;    // from scDef->perTurnXp
    std::vector<std::string> scUnlocked;       // actions unlocked so far

    // Branch choices made during this combat (level → chosen branch id).
    // Mirrors SpecialCharacter::chosenBranches; written back via SCProgressUpdate.
    std::map<int, std::string> scChosenBranches;

    // Flexible per-SC stat overrides (aoe_radius, push_range, etc.).
    // Engine reads these for mechanic-specific logic; unknown keys are ignored.
    std::unordered_map<std::string, int> scExtraStats;

    // Passive item bonuses — zero for ordinary stacks.
    // Populated at army-build time from SpecialCharacter::equipped[].
    int attackBonus  = 0;
    int defenseBonus = 0;
    int damageBonus  = 0;  // flat bonus added per-creature before the atk/def multiplier
    int speedBonus   = 0;  // added to type->speed for initiative order

    // Effective stats (base + item bonuses) — convenience used by CombatEngine.
    int effectiveAttack()  const { return type->attack  + attackBonus;  }
    int effectiveDefense() const { return type->defense + defenseBonus; }
    int effectiveSpeed()   const { return type->speed   + speedBonus;   }

    bool isDead()  const { return count <= 0; }

    // Total HP across the entire stack
    int totalHp() const {
        if (count <= 0) return 0;
        return (count - 1) * type->hitPoints + hpLeft;
    }

    // Factory: fill hpLeft to full.  pos defaults to origin — CombatEngine
    // overwrites this in placeArmies() before any game logic runs.
    // Caller must ensure *t outlives this CombatUnit (ResourceManager guarantees this).
    static CombatUnit make(const UnitType* t, int stackCount, bool player,
                           HexCoord startPos = { 0, 0 }) {
        CombatUnit u;
        u.type          = t;
        u.count         = stackCount;
        u.hpLeft        = t->hitPoints;
        u.isPlayer      = player;
        u.isDefending   = false;
        u.pos           = startPos;
        u.shotsLeft     = t->shots;
        u.hasRetaliated = false;
        return u;
    }
};
