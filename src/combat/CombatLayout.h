#pragma once
#include "hex/HexCoord.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

/*
 * ObstacleKind — a kind of battlefield obstacle (data/combat_layouts.json
 * "obstacle_kinds"): trees from the layout, barricades and stakes the player
 * builds. Every obstacle blocks movement; an opaque one also blocks shots.
 * Any stack can attack one (see CombatEngine::canAttackObstacle); at 0 hit
 * points it is gone. hp 0 = cannot be destroyed (only fieldworks placed
 * without data, which the connectivity guard keeps from walling anyone in).
 */
struct ObstacleKind {
    std::string id;
    int  hp     = 0;
    bool opaque = true;
};

/*
 * CombatLayout — the shape of one battle (data/combat_layouts.json "layouts").
 * Cells in the file are offset [col, row] (CombatMap); here they are axial.
 *
 *   width, height   grid size
 *   obstacles       terrain obstacles from the start ({cell, kind}; the
 *                   kind's hit points and opacity are copied in on load)
 *   playerZone      where the player deploys and may move units before the
 *                   battle starts, in placement preference order: companions
 *                   take the first cells, troops wall them in (front first)
 *   enemyZone       where the enemy starts, same order rules
 *   fieldworkZone   where barricades and stakes may be built (default: playerZone)
 *   firstMove       who acts first in round one: normal (initiative),
 *                   enemy_first (an ambush), player_first
 *
 * "field" (CombatLayout::field()) is the open 11 × 5 battle every encounter
 * fought before layouts existed: cols 0-1 vs cols 9-10, fieldworks in cols 2-4.
 */
struct CombatLayout {
    enum class FirstMove { Normal, EnemyFirst, PlayerFirst };
    struct Obstacle { HexCoord cell; std::string kind; int hp = 0; bool opaque = true; };   // hp, opaque: from its kind

    std::string id = "field";
    int width  = 11;
    int height = 5;
    std::vector<Obstacle> obstacles;
    std::vector<HexCoord> playerZone, enemyZone, fieldworkZone;
    FirstMove firstMove = FirstMove::Normal;

    // Most stacks one side brings: five troops and two companions.
    static constexpr int MAX_STACKS = 7;

    static CombatLayout field();
    static const char* firstMoveName(FirstMove f);
};

// Every layout of data/combat_layouts.json, and the obstacle kinds they use.
struct CombatLayouts {
    std::map<std::string, ObstacleKind> kinds;
    std::vector<CombatLayout> layouts;
    const CombatLayout* find(const std::string& id) const;
    const ObstacleKind* kind(const std::string& id) const;
};

// A layout's problems, or nullopt: in bounds, zones big enough for MAX_STACKS,
// on open hexes and apart, known obstacle kinds, every open hex connected.
std::optional<std::string> validateCombatLayout(const CombatLayout& layout,
                                                const std::map<std::string, ObstacleKind>& kinds);

// Parse (and validate) the file's JSON text. nullopt = success.
std::optional<std::string> parseCombatLayouts(const std::string& text, CombatLayouts& out);
