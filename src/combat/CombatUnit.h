#pragma once
#include "entities/UnitType.h"
#include "hex/HexCoord.h"

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
    UnitType type;           // species data  (TODO → const UnitType* when ResourceManager exists)
    int      count        = 0;   // creatures remaining in stack
    int      hpLeft       = 0;   // HP of the leading creature (1..type.hitPoints)
    bool     isPlayer     = false;
    bool     isDefending  = false;
    HexCoord pos;            // position on the combat grid (CombatMap coordinate space)
    int      shotsLeft    = 0;   // remaining ammo; 0 means melee-only
    bool     hasRetaliated = false; // true once this stack retaliates this round

    bool isDead()  const { return count <= 0; }

    // Total HP across the entire stack
    int totalHp() const {
        if (count <= 0) return 0;
        return (count - 1) * type.hitPoints + hpLeft;
    }

    // Factory: fill hpLeft to full.  pos defaults to origin — CombatEngine
    // overwrites this in placeArmies() before any game logic runs.
    static CombatUnit make(const UnitType& t, int stackCount, bool player,
                           HexCoord startPos = { 0, 0 }) {
        CombatUnit u;
        u.type          = t;
        u.count         = stackCount;
        u.hpLeft        = t.hitPoints;
        u.isPlayer      = player;
        u.isDefending   = false;
        u.pos           = startPos;
        u.shotsLeft     = t.shots;
        u.hasRetaliated = false;
        return u;
    }
};
