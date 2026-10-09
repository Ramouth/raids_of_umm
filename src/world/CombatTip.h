#pragma once
#include <string>
#include <vector>

/*
 * CombatTip — a short line a companion says in battle to teach a rule
 * (data/combat_tips.json "tips"). Loaded by ResourceManager; CombatTipWatch
 * (combat/CombatTips.h) decides when one is due.
 *
 *   when     one of combatTipWhenKeys(); a file naming any other key fails to load
 *   speaker  the companion who says it (unit name or id): only while that
 *            companion fights in this battle (not wounded, not fallen)
 *   maps     campaign map ids it may appear on ("1.0", ...)
 * Each tip is shown at most once per campaign (AdventureSession::shownTips).
 */
struct CombatTip {
    std::string id, when, speaker, text;
    std::vector<std::string> maps;
};

// The conditions a tip may wait for (see CombatTipWatch::conditions).
inline const std::vector<std::string>& combatTipWhenKeys() {
    static const std::vector<std::string> keys = {
        "battle_start", "first_turn_shooter", "enemy_adjacent", "flanked",
        "low_stack", "defend_available", "victory_first", "companion_hit"};
    return keys;
}
