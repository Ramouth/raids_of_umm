#pragma once
#include "combat/CombatEngine.h"
#include "world/CombatTip.h"
#include <set>
#include <string>
#include <vector>

/*
 * CombatTipWatch — decides, in C++, which companion tip (data/combat_tips.json)
 * is due in one battle. The client shows what it is told; demo_bot prints it.
 *
 * A tip is due when, after an action:
 *   - its "when" holds now (conditions(), the keys of combatTipWhenKeys()):
 *       battle_start        round 1
 *       first_turn_shooter  a player stack with shots left, not engaged, takes its turn
 *       enemy_adjacent      a living enemy stands next to a living player stack
 *       flanked             a player stack is pinned (CombatEngine::isFlanked)
 *       low_stack           a troop stack is below a quarter of its starting count
 *       defend_available    the player gives the active stack its orders (not mad)
 *       victory_first       the battle is won
 *       companion_hit       a player companion has lost hit points (or fallen)
 *       rushes              the turn just played was a player stack's (not mad):
 *                           it walked, ends it next to a living enemy, and no
 *                           other living player stack stands within 2 hexes of
 *                           it: out ahead of the line, alone
 *       obstacle_broken     the turn just played was a player stack's and its
 *                           blow destroyed an obstacle (tip "kind": of that kind)
 *   - its "actor" (unit id or name) is the stack the key is about: the one that
 *     just played its turn (rushes, obstacle_broken) or the active one
 *     (first_turn_shooter, defend_available); other keys ignore it;
 *   - the battle's campaign map is in its "maps";
 *   - its speaker fights in this battle and still stands (a wounded companion
 *     stays home and so never speaks; a fallen one is silent);
 *   - it was not shown before in this campaign (the shown set given at the
 *     start), or for "repeat": "battle" tips, not before in this battle;
 *   - no tip was due yet this turn (one per turn; the end of the battle is a turn).
 * The first such tip in file order is due; check() marks it shown. lines() is
 * then the exchange to show: the tip's own line, then each reply whose speaker
 * fights and stands (the others are dropped).
 */
class CombatTipWatch {
public:
    CombatTipWatch() = default;
    // tips: the registry's list (must outlive the watch); map: campaign map id
    // ("" = no tips at all); shown: ids already shown this campaign.
    CombatTipWatch(const std::vector<CombatTip>* tips, std::string map,
                   std::set<std::string> shown, const CombatEngine& engine);

    // After every action (and once at the start): the tip due now, or null.
    const CombatTip* check(const CombatEngine& engine);

    // The exchange of the tip check() returned last, in order (empty before any).
    const std::vector<CombatTipLine>& lines() const { return m_lines; }

    // Which when-keys hold right now for any actor (exposed for tests).
    std::set<std::string> conditions(const CombatEngine& engine) const;
    // The tip's when-key holds for its "actor" and "kind" filters (when holds at all).
    static bool actorMatches(const CombatEngine& engine, const CombatTip& tip);
    // The speaker (a companion's name or id, any case) fights for the player and stands.
    static bool speakerFighting(const CombatEngine& engine, const std::string& speaker);
    // The player stack that just played its turn is out ahead of the line (see rushes).
    static bool rushed(const CombatEngine& engine);

    // Tip ids shown in this battle, in order (the adventure records them).
    const std::vector<std::string>& shownHere() const { return m_shownHere; }

private:
    const std::vector<CombatTip>* m_tips = nullptr;
    std::string                   m_map;
    std::set<std::string>         m_shown;
    std::vector<std::string>      m_shownHere;
    std::vector<CombatTipLine>    m_lines;
    std::vector<int>              m_companionHp;   // per player stack: starting hp (companions; 0 = troop)
    std::string                   m_lastTurn;      // the turn the last tip was due on
};
