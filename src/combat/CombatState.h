#pragma once
#include "core/StateMachine.h"
#include "CombatEngine.h"
#include "CombatEvent.h"
#include "CombatRenderer.h"
#include "CombatMap.h"
#include "render/Camera2D.h"
#include "render/HUDRenderer.h"
#include "world/SpecialCharacter.h"
#include "hex/HexCoord.h"
#include <glm/glm.hpp>
#include <deque>
#include <optional>
#include <queue>
#include <memory>
#include <string>
#include <unordered_map>

/*
 * CombatState — thin coordinator for the combat screen.
 *
 * Owns:
 *   CombatEngine   — pure turn logic
 *   CombatRenderer — GL rendering
 *   Camera2D       — isometric combat viewport
 *
 * Nothing game-logic-related lives here — it belongs in CombatEngine.
 * Nothing render-related lives here — it belongs in CombatRenderer.
 *
 * Controls:
 *   Arrow keys / WASD   — pan camera
 *   Mouse move          — hover hex highlight
 *   Enter               — attack first enemy stack (stub)
 *   Space               — defend (pass turn)
 *   Escape              — retreat
 */
class CombatState final : public GameState {
public:
    // outcome: optional shared result written in onExit() for the caller to read
    //          after this state is popped.  Pass nullptr to ignore.
    explicit CombatState(CombatArmy player, CombatArmy enemy,
                         std::shared_ptr<CombatOutcome>  outcome   = nullptr,
                         std::vector<std::string>        lootTable = {},
                         std::optional<SpecialCharacter> dungeonSC = std::nullopt);

    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    // ── Animation helpers ─────────────────────────────────────────────────────
    void tickAnimation(float dt);
    void startEventAnimation(const CombatEvent& ev);

    // ── Combat log ────────────────────────────────────────────────────────────
    struct LogEntry { std::string text; glm::vec4 color; };
    void pushLog(std::string text, glm::vec4 color);
    void renderLog(int screenW, int screenH);

    // ── SC XP overlay (shown after battle ends) ───────────────────────────────
    void renderXpOverlay(int screenW, int screenH);

    // ── Branch choice overlay (shown mid-combat on level-up choice point) ─────
    // Thin placeholder — designed to be replaced wholesale during UI overhaul.
    // All layout state lives in m_pendingChoiceEvent / m_hoveredBranch.
    void renderBranchChoice(int screenW, int screenH);

    // ── Members ───────────────────────────────────────────────────────────────
    CombatEngine   m_engine;
    CombatRenderer m_renderer;
    HUDRenderer    m_hud;
    Camera2D       m_cam;

    std::deque<LogEntry> m_log;
    static constexpr int MAX_LOG = 14;

    HexCoord m_hoveredHex;
    bool     m_hasHovered = false;

    // Camera pan key state
    bool m_panUp    = false;
    bool m_panDown  = false;
    bool m_panLeft  = false;
    bool m_panRight = false;

    // ── Event-driven animation ────────────────────────────────────────────────
    std::queue<CombatEvent> m_pendingEvents;  // events waiting to be animated
    CombatEvent             m_currentEvent;   // event currently being animated
    bool                    m_animating    = false;
    float                   m_animTimer    = 0.f;
    float                   m_animDuration = 0.f;

    // Per-frame interpolated render state (passed to CombatRenderer).
    UnitAnimState m_anim;
    glm::vec3     m_animFrom = { 0.f, 0.f, 0.f };
    glm::vec3     m_animTo   = { 0.f, 0.f, 0.f };

    // Auto-battle: AI controls friendly units too (toggle with A key).
    bool m_autoBattle = false;

    // Counts level-ups per SC this battle (keyed by scId); populated as
    // ScLevelUp events stream through startEventAnimation().
    std::unordered_map<std::string, int> m_scLevelUpsThisBattle;

    // True once the player has acknowledged the battle result — popped next update().
    // Never call popState() directly from handleEvent (use-after-free when
    // m_processing is false during processEvents).
    bool m_wantsDismiss = false;

    // Cached ScChoicePending event; set when the engine pauses for a player
    // branch decision.  Reset after resolveScChoice() is called.
    std::optional<CombatEvent> m_pendingChoiceEvent;
    int                        m_hoveredBranch = -1;   // -1=none, 0=left, 1=right

    // Written in onExit(); caller reads this after the state is popped.
    std::shared_ptr<CombatOutcome> m_outcome;

    // Item IDs eligible to drop on PlayerWon. One is chosen at random.
    std::vector<std::string> m_lootTable;

    // SC resident in this dungeon; joins hero on PlayerWon (nullopt = no SC).
    std::optional<SpecialCharacter> m_dungeonSC;

    static constexpr float CAM_SPEED           = 8.0f;
    static constexpr float HEX_SIZE            = 1.0f;
    static constexpr float MOVE_ANIM_DURATION  = 0.30f;
    static constexpr float ATTACK_ANIM_DURATION= 0.20f;
    static constexpr float DAMAGE_ANIM_DURATION= 0.15f;
    static constexpr float DEATH_ANIM_DURATION = 0.40f;
    static constexpr float FLASH_ANIM_DURATION = 0.05f;
};
