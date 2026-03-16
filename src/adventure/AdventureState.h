#pragma once
#include "core/StateMachine.h"
#include "core/TurnManager.h"
#include "world/WorldMap.h"
#include "world/MapObject.h"
#include "hero/Hero.h"
#include "combat/CombatArmy.h"
#include "combat/CombatEvent.h"
#include "render/HexRenderer.h"
#include "render/SpriteRenderer.h"
#include "render/AnimatedSprite.h"
#include "render/HUDRenderer.h"
#include "render/Camera2D.h"
#include "render/RenderOffsets.h"
#include "hex/HexCoord.h"
#include "world/ObjectControl.h"
#include "world/TownState.h"
#include "castle/RecruitResult.h"
#include "dungeon/DungeonOutcome.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

/*
 * AdventureState — the main overworld game state.
 *
 * Owns:
 *   WorldMap        : static map data (terrain + object definitions)
 *   Hero            : the player character
 *   Camera2D        : viewport
 *   m_objectControl : per-session ownership/defeat flags (game state, NOT map data)
 *
 * Constructed either procedurally (default) or from an existing WorldMap
 * (e.g. handed in by WorldBuilderState for test-play).
 *
 * This state must not mutate the WorldMap's terrain or object definitions —
 * session state is tracked separately. If future gameplay needs to alter
 * tiles (e.g. building a road), that goes through a dedicated command object
 * that can be serialised and replayed.
 */
class AdventureState final : public GameState {
public:
    // Default: generate a procedural map at startup.
    AdventureState() = default;

    // Accept an externally built map (from WorldBuilderState test-play,
    // or a loaded save file). The map is moved in — no copying.
    explicit AdventureState(WorldMap map, std::string mapPath = {});

    // "Continue" path: normal init then immediately load data/saves/session.json.
    struct LoadSave {};
    explicit AdventureState(LoadSave);

    void onEnter()  override;
    void onExit()   override;
    void onResume() override;  // called after CombatState / CastleState pops
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    // ── Initialisation ───────────────────────────────────────────────────────
    void initMap();          // only called when no external map was provided
    void initHeroArmy();     // give hero a starting army from ResourceManager (first enter only)

    // ── Combat army builders ──────────────────────────────────────────────────
    CombatArmy buildPlayerArmy() const;
    CombatArmy buildEnemyArmy(const MapObjectDef& obj) const;

    // ── Game logic ───────────────────────────────────────────────────────────
    void moveHero(const HexCoord& dest);
    void endTurn();
    void wait();
    void onHeroVisit(const HexCoord& coord);

    // ── Rendering helpers ────────────────────────────────────────────────────
    void renderTerrain();
    void renderObjects();
    void renderHero();
    void renderPathPreview();

    // ── Session save / load ──────────────────────────────────────────────────
    void saveSession();
    void loadSession();
    void renderExitPrompt(int sw, int sh);

    // ── Members ──────────────────────────────────────────────────────────────

    WorldMap       m_map;
    Hero           m_hero;
    Camera2D       m_cam;

    HexRenderer    m_hexRenderer;
    AnimatedSprite m_heroSprite;               // hero — walk/idle clips
    SpriteRenderer m_dungeonSpriteRenderer;    // ObjType::Dungeon
    SpriteRenderer m_buildingSpriteRenderer;   // ObjType::Town
    SpriteRenderer m_goldMineSpriteRenderer;   // ObjType::GoldMine
    SpriteRenderer m_crystalMineSpriteRenderer;// ObjType::CrystalMine
    SpriteRenderer m_artifactSpriteRenderer;   // ObjType::Artifact
    HUDRenderer    m_hud;

    // Session state — object ownership/defeat flags live here, NOT in WorldMap.
    ObjectControlMap m_objectControl;

    // Per-town recruit pools and building state (keyed by town HexCoord).
    TownStateMap m_townStates;

    // Pending recruit result — set before pushing CastleState, read in onResume().
    std::shared_ptr<RecruitResult> m_pendingRecruit;

    // True after a combat defeat; blocks movement and shows overlay.
    bool m_isDefeated = false;

    // Smooth movement — render pos chases waypoints along the actual hex path
    glm::vec3              m_heroRenderPos { 0.0f };
    glm::vec3              m_heroTargetPos { 0.0f };
    std::vector<HexCoord>  m_moveQueue;      // waypoints still to traverse
    int                    m_moveQueueIdx = 0;

    // Movement animation state
    float                  m_heroFacingAngle = 0.0f;  // radians, 0 = facing right
    float                  m_walkCycle = 0.0f;         // 0-1 progress within current hex step
    bool                   m_isWalking = false;
    HexCoord               m_pendingVisit;             // tile to visit when movement finishes
    bool                   m_hasPendingVisit = false;

    // Path preview
    std::vector<HexCoord> m_previewPath;

    // Hover
    HexCoord   m_hovered;
    bool       m_hasHovered = false;

    // Hero selection
    bool       m_heroSelected = false;

    // Key states for camera pan
    bool m_keyW=false, m_keyA=false, m_keyS=false, m_keyD=false;

    TurnManager m_turnManager;

    // Toggle the dev HUD — set false here (or press H) to hide it
    bool m_showHUD = true;

    // Debug mode: double-tap P for infinite movement
    bool m_infiniteMoves = false;
    float m_lastPTime = 0.0f;

    // Whether this state owns a procedurally generated map or a provided one.
    bool        m_externalMap = false;

    // If true, onEnter calls loadSession() after normal init.
    bool        m_autoLoad    = false;

    // Transient notification banner (e.g. artifact pickup). Fades after m_notifyDuration.
    std::string m_notification;
    float       m_notifyTimer    = 0.0f;
    static constexpr float NOTIFY_DURATION = 3.5f;

    // Exit confirmation prompt: shown when ESC is pressed.
    bool        m_showExitPrompt = false;
    int         m_exitPromptHovered = -1;  // 0 = Exit, 1 = Save, 2 = Load, 3 = Continue

    // Path used to load the map — needed to write into the session save file.
    // Empty for procedural maps (auto-saved to data/saves/auto_map.json on first save).
    std::string m_mapPath;

    // Pending dungeon result — set before pushing DungeonState, read in onResume().
    std::shared_ptr<DungeonOutcome> m_pendingDungeon;
    HexCoord                        m_dungeonCoord;  // overworld coord of the entered dungeon

    // Pending direct combat result — set before pushing CombatState, read in onResume().
    std::shared_ptr<CombatOutcome>  m_pendingCombat;

    RenderOffsetConfig m_offsets;

    static constexpr float HEX_SIZE        = 1.0f;
    static constexpr float CAM_SPEED       = 8.0f;
    static constexpr float HERO_MOVE_SPEED = 12.0f;     // hexes per second
    static constexpr float CAM_FOLLOW_SPEED = 4.0f;    // camera lerp rate while hero moves
    static constexpr float WALK_BOB_HEIGHT = 0.08f;    // vertical bob amount
    static constexpr float STEP_PAUSE      = 0.12f;     // pause at each hex center (seconds)
};
