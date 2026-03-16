#pragma once
#include "core/StateMachine.h"
#include "DungeonOutcome.h"
#include "combat/CombatEvent.h"
#include "combat/CombatArmy.h"
#include "world/SpecialCharacter.h"
#include "hero/Hero.h"
#include "world/WorldMap.h"
#include "render/HexRenderer.h"
#include "render/AnimatedSprite.h"
#include "render/HUDRenderer.h"
#include "render/Camera2D.h"
#include "hex/HexCoord.h"
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/*
 * DungeonState — underground exploration map (HoMM3-style).
 *
 * The hero navigates a small hex map, fights one guard encounter,
 * then may recruit the dungeon's Special Character before exiting.
 *
 * Layout (10×5 even-q offset grid):
 *
 *   col:  0  1  2  3  4  5  6  7  8  9
 *   row0: O  O  O  O  O  O  O  O  O  O
 *   row1: O  .  .  .  O  O  O  .  .  O
 *   row2: E  .  .  .  .  G  .  S  .  O   ← E=entrance, G=guard, S=SC
 *   row3: O  .  .  .  O  O  O  .  .  O
 *   row4: O  O  O  O  O  O  O  O  O  O
 *
 *   '.' = Ruins (passable)   'O' = Obsidian (wall)
 *
 * Controls: click hero → click tile to move; ESC = exit dungeon.
 */
class DungeonState final : public GameState {
public:
    DungeonState(Hero                                    heroSnapshot,
                 std::optional<SpecialCharacter>         dungeonSC,
                 std::vector<std::string>                lootTable,
                 std::shared_ptr<DungeonOutcome>         outcome);

    void onEnter()  override;
    void onExit()   override;
    void onResume() override;
    void update(float dt) override;
    void render()   override;
    bool handleEvent(void* sdlEvent) override;

private:
    void generateMap();
    void moveHero(const HexCoord& dest);
    void onHeroVisit(const HexCoord& coord);
    void renderCombatResult(int sw, int sh);
    CombatArmy buildPlayerArmy() const;
    CombatArmy buildEnemyArmy()  const;

    // ── Data ──────────────────────────────────────────────────────────────────
    WorldMap m_map;
    Hero     m_hero;
    Camera2D m_cam;

    HexRenderer    m_hexRenderer;
    AnimatedSprite m_heroSprite;
    AnimatedSprite m_enemySprite;
    AnimatedSprite m_scSprite;
    HUDRenderer    m_hud;

    std::optional<SpecialCharacter> m_dungeonSC;
    std::vector<std::string>        m_lootTable;
    std::shared_ptr<DungeonOutcome> m_outcome;
    std::shared_ptr<CombatOutcome>  m_pendingCombat;

    // ── Dungeon layout ────────────────────────────────────────────────────────
    HexCoord m_entrancePos;
    HexCoord m_enemyPos;
    HexCoord m_scPos;

    // ── Session state ─────────────────────────────────────────────────────────
    bool m_enemyDefeated = false;
    bool m_scRecruited   = false;
    bool m_wantsDismiss  = false;
    std::vector<std::string> m_collectedItems;

    // ── Post-combat result overlay ────────────────────────────────────────────
    struct CombatResultDisplay {
        CombatResult result = CombatResult::Ongoing;
        struct ScGain {
            std::string name;
            int oldLevel, newLevel, newXp;
        };
        std::vector<ScGain>     scGains;
        std::vector<std::string> itemNames;
        bool wantsDismissAfter = false;
    };
    bool                m_showCombatResult = false;
    CombatResultDisplay m_lastCombatResult;

    // ── Movement ──────────────────────────────────────────────────────────────
    glm::vec3             m_heroRenderPos { 0.f, 0.f, 0.f };
    glm::vec3             m_heroTargetPos { 0.f, 0.f, 0.f };
    std::vector<HexCoord> m_moveQueue;
    int                   m_moveQueueIdx = 0;
    HexCoord              m_pendingVisit;
    bool                  m_hasPendingVisit = false;

    // ── Input ─────────────────────────────────────────────────────────────────
    HexCoord              m_hoveredHex;
    bool                  m_hasHovered   = false;
    bool                  m_heroSelected = false;
    std::vector<HexCoord> m_previewPath;
    bool m_keyW=false, m_keyA=false, m_keyS=false, m_keyD=false;

    // ── Constants ─────────────────────────────────────────────────────────────
    static constexpr float HEX_SIZE        = 1.0f;
    static constexpr float HERO_MOVE_SPEED = 8.0f;
    static constexpr float CAM_SPEED       = 8.0f;
    static constexpr int   GRID_W          = 10;
    static constexpr int   GRID_H          = 5;

    // Even-q flat-top offset → axial (same convention as CombatMap).
    static HexCoord toHex(int col, int row) {
        return { col, row - (col - (col & 1)) / 2 };
    }
};
