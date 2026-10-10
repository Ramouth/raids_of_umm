#pragma once
#include <string>
#include <vector>

/*
 * CombatTip — a short line a companion says in battle to teach a rule, or a
 * small exchange of banter (data/combat_tips.json "tips"). Loaded by
 * ResourceManager; CombatTipWatch (combat/CombatTips.h) decides when one is due.
 *
 *   when     one of combatTipWhenKeys(); a file naming any other key fails to load
 *   speaker  the companion who says it (unit name or id): only while that
 *            companion fights in this battle (not wounded, not fallen)
 *   text     the line
 *   maps     campaign map ids it may appear on ("1.0", ...)
 *   reply    0-2 lines said after it ({speaker, text}); a reply whose speaker
 *            is not fighting and standing is dropped (the tip still comes)
 *   repeat   "campaign" (default): at most once per campaign
 *            (AdventureSession::shownTips); "battle": again in a later battle,
 *            at most once per battle
 *   actor    unit id or name (any case) of the stack the when-key is about
 *            (see CombatTipWatch: rushes, obstacle_broken, first_turn_shooter,
 *            defend_available); "" = any of the player's stacks; ignored by
 *            the other keys
 *   kind     obstacle_broken only: the obstacle kind ("tree"); "" = any
 */
struct CombatTipLine {
    std::string speaker, text;
};

struct CombatTip {
    std::string id, when, speaker, text;
    std::vector<std::string> maps;
    std::vector<CombatTipLine> reply;
    bool perBattle = false;   // "repeat": "battle"
    std::string actor, kind;
};

// The conditions a tip may wait for (see CombatTipWatch::conditions).
inline const std::vector<std::string>& combatTipWhenKeys() {
    static const std::vector<std::string> keys = {
        "battle_start", "first_turn_shooter", "enemy_adjacent", "flanked",
        "low_stack", "defend_available", "victory_first", "companion_hit",
        "rushes", "obstacle_broken"};
    return keys;
}
