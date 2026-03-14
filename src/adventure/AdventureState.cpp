#include "AdventureState.h"
#include "castle/CastleState.h"
#include "combat/CombatAI.h"
#include "combat/CombatEngine.h"
#include "combat/CombatState.h"
#include "combat/CombatUnit.h"
#include "dungeon/DungeonState.h"
#include "equipment/EquipmentState.h"
#include "party/PartyState.h"
#include "core/Application.h"
#include "render/TileVisuals.h"
#include "world/Resources.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <unordered_map>

// ── Army builders ─────────────────────────────────────────────────────────────

CombatArmy AdventureState::buildPlayerArmy() const {
    CombatArmy army;
    army.ownerName = m_hero.name;
    army.isPlayer  = true;

    for (const auto& slot : m_hero.army) {
        if (!slot.isEmpty())
            army.stacks.push_back(CombatUnit::make(slot.unitType, slot.count, true));
    }

    // SCs fight as solo units (count=1), with passive item bonuses baked in.
    for (const auto& sc : m_hero.specials) {
        if (sc.isEmpty()) continue;
        CombatUnit cu = CombatUnit::make(&sc.combatStats, 1, true);
        for (const WondrousItem* item : sc.equipped) {
            if (!item) continue;
            for (const ItemEffect& ef : item->passiveEffects) {
                if      (ef.stat == "attack")  cu.attackBonus  += ef.amount;
                else if (ef.stat == "defense") cu.defenseBonus += ef.amount;
                else if (ef.stat == "damage")  cu.damageBonus  += ef.amount;
                else if (ef.stat == "speed")   cu.speedBonus   += ef.amount;
            }
        }
        // Bake SC progression state into the CombatUnit so the engine can
        // award XP and level-ups during battle, then write them back to outcome.
        cu.isSpecialCharacter = true;
        cu.scId               = sc.id;
        cu.scDef              = sc.def;
        cu.scLevel            = sc.level;
        cu.scXp               = sc.xp;
        cu.scUnlocked         = sc.unlockedActions;
        if (sc.def) {
            cu.killXp    = sc.def->killBonusXp;
            cu.perTurnXp = sc.def->perTurnXp;
        }
        if (cu.attackBonus || cu.defenseBonus || cu.damageBonus || cu.speedBonus)
            std::cout << "[Adventure] " << sc.name << " item bonuses:"
                      << " atk+" << cu.attackBonus
                      << " def+" << cu.defenseBonus
                      << " dmg+" << cu.damageBonus
                      << " spd+" << cu.speedBonus << "\n";
        army.stacks.push_back(cu);
    }

    // Fallback: if hero has no army (shouldn't happen after initHeroArmy), fight bare-handed.
    if (army.stacks.empty()) {
        static const UnitType s_heroAlone = []() {
            UnitType t;
            t.id = "hero_alone"; t.name = "Hero";
            t.hitPoints = 20; t.attack = 5; t.defense = 4;
            t.minDamage = 1;  t.maxDamage = 4; t.speed = 5; t.moveRange = 3;
            return t;
        }();
        army.stacks.push_back(CombatUnit::make(&s_heroAlone, 1, true));
    }
    return army;
}

CombatArmy AdventureState::buildEnemyArmy(const MapObjectDef& obj) const {
    CombatArmy army;
    army.ownerName = obj.name;
    army.isPlayer  = false;

    auto& rm = Application::get().resources();
    if (rm.loaded()) {
        if (const UnitType* t = rm.unit("skeleton_warrior"))
            army.stacks.push_back(CombatUnit::make(t, 12, false));
        if (const UnitType* t = rm.unit("sand_scorpion"))
            army.stacks.push_back(CombatUnit::make(t,  4, false));
    }

    // Fallback if data didn't load.
    if (army.stacks.empty()) {
        static const UnitType s_guard = []() {
            UnitType t;
            t.id = "guard"; t.name = "Dungeon Guard";
            t.hitPoints = 8; t.attack = 5; t.defense = 4;
            t.minDamage = 1; t.maxDamage = 3; t.speed = 4; t.moveRange = 3;
            return t;
        }();
        army.stacks.push_back(CombatUnit::make(&s_guard, 8, false));
    }
    return army;
}

void AdventureState::initHeroArmy() {
    if (m_hero.armySize() > 0) return;  // already initialised (re-enter after combat)

    auto& rm = Application::get().resources();
    if (!rm.loaded()) return;

    if (const UnitType* t = rm.unit("desert_archer"))
        m_hero.addUnit(t, 10);
    if (const UnitType* t = rm.unit("mummy"))
        m_hero.addUnit(t, 3);

    std::cout << "[Adventure] Hero army: " << m_hero.armySize() << " stacks.\n";
}

// ── Construction ──────────────────────────────────────────────────────────────

AdventureState::AdventureState(WorldMap map)
    : m_map(std::move(map)), m_externalMap(true) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void AdventureState::onEnter() {
    auto& app = Application::get();
    m_cam = Camera2D(app.width(), app.height());

    m_hexRenderer.init();
    m_hexRenderer.loadTerrainTextures();
    m_spriteRenderer.init();
    m_spriteRenderer.loadSprite("assets/textures/units/armoured_warrior.png");
    m_dungeonSpriteRenderer.init();
    m_dungeonSpriteRenderer.loadSprite("assets/textures/objects/dungeon.png");
    m_buildingSpriteRenderer.init();
    m_buildingSpriteRenderer.loadSprite("assets/textures/objects/castle.png");
    m_hud.init();

    if (!m_externalMap)
        initMap();

    initHeroArmy();

    m_turnManager.init(2000);

    // Seed TownState for every Town on the map (initial pool = one week's growth).
    {
        auto& rm = Application::get().resources();
        m_townStates.clear();
        for (const auto& obj : m_map.objects()) {
            if (obj.type == ObjType::Town) {
                TownState& ts = m_townStates[obj.pos];
                if (rm.loaded()) {
                    for (const UnitType* u : rm.unitsByTier()) {
                        if (u->weeklyGrowth > 0)
                            ts.recruitPool[u->id] = u->weeklyGrowth;
                    }
                }
                std::cout << "[Adventure] Town '" << obj.name
                          << "' seeded with " << ts.recruitPool.size() << " unit type(s).\n";
            }
        }
    }

    // Load render offsets (missing file = fresh start, not an error)
    if (auto err = m_offsets.load("assets/render_offsets.json"))
        std::cerr << "[Adventure] Offsets load warning: " << *err << "\n";

    // Place hero at origin; ensure it's passable.
    m_hero.pos = {0, 0};
    if (MapTile* t = m_map.tileAt(m_hero.pos))
        t->passable = true;

    // Always place a test enemy scout 2 hexes east so combat can be tested immediately.
    HexCoord scoutPos{2, 0};
    if (MapTile* t = m_map.tileAt(scoutPos))
        t->passable = true;
    m_map.placeObject({ scoutPos, ObjType::Dungeon, "Enemy Scout" });

    // Centre camera on hero.
    float hx, hz;
    m_hero.pos.toWorld(HEX_SIZE, hx, hz);
    m_cam.setPosition({hx, hz});
    m_heroRenderPos = { hx, 0.0f, hz };
    m_heroTargetPos = m_heroRenderPos;

    std::cout << "[Adventure] Day " << m_turnManager.day() << " — " << m_map.name() << "\n";
    std::cout << "  Click hero to select, then click a tile to move.\n";
    std::cout << "  SPACE = end turn  |  R = restart  |  WASD = pan  |  scroll = zoom  |  ESC = back to editor\n";
}

void AdventureState::onExit() {
    std::cout << "[Adventure] Exited\n";
}

void AdventureState::onResume() {
    // Handle recruit result from CastleState.
    if (m_pendingRecruit) {
        TownState* srcTown = nullptr;
        auto tsIt = m_townStates.find(m_pendingRecruit->townCoord);
        if (tsIt != m_townStates.end()) srcTown = &tsIt->second;

        for (const auto& s : m_pendingRecruit->hired) {
            if (!s.type || s.count <= 0) continue;
            if (!m_hero.addUnit(s.type, s.count)) {
                // Army is full — refund gold and return units to the pool.
                ResourcePool& treasury = m_turnManager.playerFaction().treasury;
                for (int i = 0; i < s.count; ++i) treasury += s.type->cost;
                if (srcTown) srcTown->recruitPool[s.type->id] += s.count;
                std::cout << "[Adventure] Refunded " << s.count << "x " << s.type->name
                          << " — army full.\n";
            }
        }
        if (!m_pendingRecruit->hired.empty())
            std::cout << "[Adventure] After recruit: " << m_hero.armySize() << " stacks.\n";
        m_pendingRecruit.reset();
    }

    // Handle direct combat result (battle-terrain encounter).
    if (m_pendingCombat) {
        CombatOutcome result = std::move(*m_pendingCombat);
        m_pendingCombat.reset();

        // Sync hero army from survivors (skip SC units — they live in specials, not army).
        for (auto& slot : m_hero.army) slot = {};
        for (const auto& s : result.survivors) {
            if (!s.type || s.count <= 0) continue;
            if (m_hero.findSpecial(s.type->id)) continue;  // SC unit — not an army slot
            m_hero.addUnit(s.type, s.count);
        }
        std::cout << "[Adventure] Returned from combat. Army: " << m_hero.armySize() << " stacks.\n";

        // Sync SC progression earned in combat back to the hero.
        for (const auto& upd : result.scUpdates) {
            SpecialCharacter* sc = m_hero.findSpecial(upd.scId);
            if (!sc || !sc->def) continue;
            int levelDelta = upd.level - sc->level;
            for (int i = 0; i < levelDelta; ++i) {
                sc->combatStats.attack    += sc->def->attackGrowth;
                sc->combatStats.defense   += sc->def->defenseGrowth;
                sc->combatStats.hitPoints += sc->def->maxHpGrowth;
            }
            sc->level             = upd.level;
            sc->xp                = upd.xp;
            sc->unlockedActions = upd.unlockedActions;
            for (const auto& [lv, br] : upd.chosenBranches)
                sc->chosenBranches[lv] = br;
            if (levelDelta > 0)
                std::cout << "[Adventure] " << sc->name
                          << " is now level " << sc->level << "!\n";
        }

        if (result.result == CombatResult::EnemyWon) {
            std::cout << "[Adventure] Hero defeated in combat.\n";
            m_isDefeated = true;
        }
        return;
    }

    if (!m_pendingDungeon) return;

    DungeonOutcome outcome = std::move(*m_pendingDungeon);
    m_pendingDungeon.reset();

    // Sync hero army from dungeon survivors.
    for (auto& slot : m_hero.army) slot = {};
    for (const auto& s : outcome.survivors)
        if (s.type && s.count > 0)
            m_hero.addUnit(s.type, s.count);
    std::cout << "[Adventure] Returned from dungeon. Army: " << m_hero.armySize() << " stacks.\n";

    if (outcome.survivors.empty()) {
        // Hero wiped out in dungeon.
        std::cout << "[Adventure] Hero defeated in dungeon.\n";
        m_isDefeated = true;
        return;
    }

    // Sync SC progression earned inside the dungeon back to the real hero.
    for (const auto& upd : outcome.scUpdates) {
        SpecialCharacter* sc = m_hero.findSpecial(upd.scId);
        if (!sc || !sc->def) continue;
        int levelDelta = upd.level - sc->level;
        // Apply permanent stat growth for each level gained.
        for (int i = 0; i < levelDelta; ++i) {
            sc->combatStats.attack    += sc->def->attackGrowth;
            sc->combatStats.defense   += sc->def->defenseGrowth;
            sc->combatStats.hitPoints += sc->def->maxHpGrowth;
        }
        sc->level              = upd.level;
        sc->xp                 = upd.xp;
        sc->unlockedActions  = upd.unlockedActions;
        if (levelDelta > 0)
            std::cout << "[Adventure] " << sc->name
                      << " is now level " << sc->level << "!\n";
    }

    if (outcome.completed) {
        m_objectControl[m_dungeonCoord].guardDefeated = true;

        auto& rm = Application::get().resources();
        for (const auto& sc : outcome.scRecruited) {
            if (m_hero.addSpecial(sc))
                std::cout << "[Adventure] " << sc.name << " joins your party!\n";
            else
                std::cout << "[Adventure] Party full — " << sc.name << " could not join.\n";
        }
        for (const auto& id : outcome.itemsFound) {
            if (const WondrousItem* it = rm.item(id))
                if (m_hero.addItem(it))
                    std::cout << "[Adventure] Found item: " << it->name << "\n";
        }
    }
}

void AdventureState::initMap() {
    m_map.generateProcedural(12, /*seed=*/0);
}

// ── Game logic ────────────────────────────────────────────────────────────────

namespace {

float angleFromDirection(const glm::vec3& dir) {
    if (glm::length(dir) < 0.001f) return 0.0f;
    return std::atan2(dir.x, -dir.z);
}

float lerpAngle(float from, float to, float t) {
    float diff = to - from;
    constexpr float PI = 3.14159265358979f;
    while (diff > PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    return from + diff * t;
}

} // namespace

void AdventureState::moveHero(const HexCoord& dest) {
    if (dest == m_hero.pos)      return;
    if (!m_map.hasTile(dest))    return;

    auto path = m_map.findPath(m_hero.pos, dest);

    if (path.size() < 2) {
        std::cout << "  [!] No path to that tile.\n";
        return;
    }

    int steps = static_cast<int>(path.size()) - 1;
    if (!m_infiniteMoves && !m_hero.canReach(steps)) {
        std::cout << "  [!] Not enough movement ("
                  << m_hero.movesLeft << " left, need " << steps << ").\n";
        return;
    }

    m_hero.pos       = dest;
    if (!m_infiniteMoves) {
        m_hero.movesLeft -= steps;
    }

    // Queue every waypoint so the render position follows the path hex by hex.
    m_moveQueue    = path;
    m_moveQueueIdx = 1;
    float hx, hz;
    m_moveQueue[1].toWorld(HEX_SIZE, hx, hz);
    m_heroTargetPos = { hx, 0.0f, hz };
    
    // Initialize facing direction toward first waypoint
    glm::vec3 dir = m_heroTargetPos - m_heroRenderPos;
    m_heroFacingAngle = angleFromDirection(dir);
    m_walkCycle = 0.0f;
    m_isWalking = true;

    // Set pending visit to trigger when movement finishes
    m_pendingVisit = dest;
    m_hasPendingVisit = true;

    std::cout << "  [Day " << m_turnManager.day() << "] Hero moved to (" << dest.q << "," << dest.r
              << ") — Moves: " << m_hero.movesLeft << "/" << m_hero.movesMax
              << " | Controlled: " << m_objectControl.size() << "\n";
}

void AdventureState::onHeroVisit(const HexCoord& coord) {
    const MapObjectDef* obj = m_map.objectAt(coord);
    if (!obj) return;

    bool alreadyHandled = false;
    if (auto it = m_objectControl.find(coord); it != m_objectControl.end()) {
        if (obj->type == ObjType::Dungeon)
            alreadyHandled = it->second.guardDefeated;
        else
            alreadyHandled = it->second.ownerFaction == 1;
    }

    // Towns are always re-enterable (recruit, build, etc.).
    // One-time objects (dungeons, mines, artifacts) block re-entry once handled.
    if (alreadyHandled && obj->type != ObjType::Town) return;

    auto& ctrl = m_objectControl[coord];
    ctrl.objType = obj->type;
    // Note: guardDefeated for dungeons is set in onResume() when DungeonOutcome.completed is true.
    if (obj->type != ObjType::Town && obj->type != ObjType::Dungeon)
        ctrl.ownerFaction = 1;
    std::cout << "  *** Visited " << obj->typeName() << ": " << obj->name << " ***\n";

    switch (obj->type) {
        case ObjType::Town: {
            TownState& ts = m_townStates[coord];
            ResourcePool& treasury = m_turnManager.playerFaction().treasury;
            m_pendingRecruit = std::make_shared<RecruitResult>();
            m_pendingRecruit->townCoord = coord;
            Application::get().pushState(
                std::make_unique<CastleState>(obj->name, ts, treasury, m_hero, m_pendingRecruit));
            break;
        }
        case ObjType::Dungeon: {
            std::cout << "     Entering dungeon!\n";
            m_pendingDungeon = std::make_shared<DungeonOutcome>();
            m_dungeonCoord   = coord;

            // Each dungeon has a unique resident SC.
            // Add new entries here as dungeons are added to WorldMap.
            struct DungeonSCDef { const char* id; const char* name; const char* archetype; };
            static const std::unordered_map<std::string, DungeonSCDef> kDungeonSCs = {
                { "Tomb of Kha'Set",  { "ushari",   "Ushari",   "warrior"      } },
                { "Buried Sanctum",   { "sekhara",  "Sekhara",  "glass_cannon" } },
            };

            std::optional<SpecialCharacter> residentSC;
            auto scIt = kDungeonSCs.find(obj->name);
            if (scIt != kDungeonSCs.end()) {
                const auto& d = scIt->second;
                residentSC = SpecialCharacter::make(d.id, d.name, d.archetype);
            }

            Application::get().pushState(
                std::make_unique<DungeonState>(m_hero, residentSC,
                                               std::vector<std::string>{"scarab_amulet"},
                                               m_pendingDungeon));
            break;
        }
        case ObjType::GoldMine:
            std::cout << "     +500 Gold per turn secured!\n";
            break;
        case ObjType::CrystalMine:
            std::cout << "     +2 Sand Crystal per turn secured!\n";
            break;
        case ObjType::Artifact:
            std::cout << "     Artifact added to hero's backpack!\n";
            break;
        case ObjType::COUNT:
            break;  // sentinel, never a real object
    }
}

void AdventureState::endTurn() {
    m_turnManager.nextDay(m_hero, m_objectControl,
                          Application::get().resources(), m_townStates);

    int gold = m_turnManager.playerFaction().treasury[Resource::Gold];
    m_heroSelected = false;
    m_previewPath.clear();

    // ── End-of-turn summary ───────────────────────────────────────────────────
    std::cout << "[Turn] Month " << m_turnManager.month()
              << "  Week "       << m_turnManager.weekOfMonth()
              << "  Day "        << m_turnManager.dayOfWeek()
              << "  (Day "       << m_turnManager.day() << " absolute)"
              << "  | Gold: "    << gold
              << "  | Moves: "   << m_hero.movesLeft << "/" << m_hero.movesMax
              << "\n";

    if (!m_turnManager.lastEvent().empty())
        std::cout << "[Turn] >>> " << m_turnManager.lastEvent() << " <<<\n";
}

void AdventureState::wait() {
    m_hero.resetMoves();
    m_heroSelected = false;
    m_previewPath.clear();
    std::cout << "[Adventure] Waited. Moves restored.\n";
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool AdventureState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    // Defeat screen: only allow restart or quit.
    if (m_isDefeated) {
        if (e->type == SDL_KEYDOWN && !e->key.repeat) {
            switch (e->key.keysym.sym) {
                case SDLK_r:
                    Application::get().replaceState(std::make_unique<AdventureState>());
                    return true;
                case SDLK_ESCAPE:
                    Application::get().popState();
                    return true;
                default: break;
            }
        }
        return true;  // swallow all other events while defeated
    }

    if (e->type == SDL_KEYDOWN && !e->key.repeat) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE: Application::get().popState(); return true;
            case SDLK_SPACE:  endTurn(); return true;
            case SDLK_h:      m_showHUD = !m_showHUD; return true;
            case SDLK_e:
                Application::get().pushState(
                    std::make_unique<EquipmentState>(m_hero));
                return true;
            case SDLK_f:
                Application::get().pushState(
                    std::make_unique<PartyState>(m_hero));
                return true;
            case SDLK_w:      wait(); return true;
            case SDLK_p: {
                float now = SDL_GetTicks() / 1000.0f;
                if (now - m_lastPTime < 0.5f) {
                    m_infiniteMoves = !m_infiniteMoves;
                    std::cout << "  [DEBUG] Infinite moves: " << (m_infiniteMoves ? "ON" : "OFF") << "\n";
                }
                m_lastPTime = now;
                return true;
            }
            case SDLK_r:
                Application::get().replaceState(std::make_unique<AdventureState>(std::move(m_map)));
                return true;
            case SDLK_q: {
                // Quick auto-resolved test combat — no visuals, instant result.
                static const UnitType s_dummy = []() {
                    UnitType t;
                    t.id = "dummy"; t.name = "Training Dummy";
                    t.hitPoints = 5; t.attack = 2; t.defense = 2;
                    t.minDamage = 1; t.maxDamage = 2; t.speed = 2; t.moveRange = 2;
                    return t;
                }();
                CombatArmy enemy;
                enemy.ownerName = "Training Dummies";
                enemy.isPlayer  = false;
                enemy.stacks.push_back(CombatUnit::make(&s_dummy, 6, false));

                CombatEngine sim(buildPlayerArmy(), std::move(enemy));
                constexpr int MAX_TURNS = 500;
                for (int i = 0; i < MAX_TURNS && !sim.isOver(); ++i)
                    CombatAI::takeTurn(sim);

                const char* res = sim.result() == CombatResult::PlayerWon ? "PLAYER WINS"
                                : sim.result() == CombatResult::EnemyWon  ? "ENEMY WINS"
                                                                           : "DRAW/RETREAT";
                std::cout << "[QuickCombat] " << res
                          << " after round " << sim.roundNumber() << "\n";
                for (const auto& s : sim.playerArmy().stacks)
                    if (!s.isDead())
                        std::cout << "  Survivor: " << s.type->name
                                  << " x" << s.count << "\n";
                return true;
            }
            case SDLK_UP:    m_keyW = true; return true;
            case SDLK_DOWN:  m_keyS = true; return true;
            case SDLK_LEFT:  m_keyA = true; return true;
            case SDLK_RIGHT: m_keyD = true; return true;
        }
    }
    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_UP:    m_keyW = false; return true;
            case SDLK_DOWN:  m_keyS = false; return true;
            case SDLK_LEFT:  m_keyA = false; return true;
            case SDLK_RIGHT: m_keyD = false; return true;
        }
    }

    if (e->type == SDL_MOUSEWHEEL) {
        m_cam.adjustZoom(-e->wheel.y * Camera2D::ZOOM_STEP);
        return true;
    }

    if (e->type == SDL_MOUSEMOTION) {
        glm::vec2 wp  = m_cam.screenToWorld(e->motion.x, e->motion.y);
        HexCoord  hc  = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
        m_hasHovered  = m_map.hasTile(hc);
        m_hovered     = hc;

        m_previewPath.clear();
        if (m_heroSelected && m_hasHovered && hc != m_hero.pos)
            m_previewPath = m_map.findPath(m_hero.pos, hc);
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        auto& app2 = Application::get();
        float sc2  = app2.height() / 600.0f;
        float btnX = 10 * sc2, btnY = 10 * sc2;
        float btnW = 110 * sc2, btnH = 36 * sc2;
        float mx = static_cast<float>(e->button.x);
        float my = static_cast<float>(e->button.y);
        if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH) {
            endTurn();
            return true;
        }

        glm::vec2 wp      = m_cam.screenToWorld(e->button.x, e->button.y);
        HexCoord  clicked = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);

        float heroWx, heroWz;
        m_hero.pos.toWorld(HEX_SIZE, heroWx, heroWz);
        float dx = wp.x - heroWx;
        float dz = wp.y - heroWz;
        bool clickedHero = (dx*dx + dz*dz) < (HEX_SIZE * HEX_SIZE * 1.5f);

        if (clickedHero) {
            m_heroSelected = !m_heroSelected;
            m_previewPath.clear();
            std::cout << "  Hero " << (m_heroSelected ? "selected" : "deselected") << "\n";
        } else if (m_heroSelected && m_map.hasTile(clicked)) {
            moveHero(clicked);
            m_heroSelected = false;
            m_previewPath.clear();
        } else {
            m_heroSelected = false;
            m_previewPath.clear();
        }
        return true;
    }

    if (e->type == SDL_WINDOWEVENT &&
        e->window.event == SDL_WINDOWEVENT_RESIZED) {
        m_cam.resize(e->window.data1, e->window.data2);
    }

    return false;
}

// ── Update ────────────────────────────────────────────────────────────────────

void AdventureState::update(float dt) {
    float speed = CAM_SPEED * m_cam.zoom() / 12.0f;
    float dx = 0.0f, dz = 0.0f;
    if (m_keyW) dz -= speed * dt;
    if (m_keyS) dz += speed * dt;
    if (m_keyA) dx -= speed * dt;
    if (m_keyD) dx += speed * dt;
    if (dx != 0.0f || dz != 0.0f)
        m_cam.pan(dx, dz);

    // Smooth hero animation — step through each hex in the path, not straight-line.
    glm::vec3 delta = m_heroTargetPos - m_heroRenderPos;
    float dist = glm::length(delta);
    
    if (dist > 0.001f) {
        // Hero is moving between hexes
        m_isWalking = true;
        
        // Calculate progress with easing (ease-in-out for smoother starts/stops)
        float stepDist = HERO_MOVE_SPEED * dt;
        float t = (stepDist >= dist) ? 1.0f : stepDist / dist;
        
        // Use smoother easing: smoothstep-like curve
        float easedT = t * t * (3.0f - 2.0f * t);
        
        glm::vec3 movement = delta * easedT;
        m_heroRenderPos += movement;
        
        // Update facing direction based on movement direction
        float targetAngle = angleFromDirection(delta);
        m_heroFacingAngle = lerpAngle(m_heroFacingAngle, targetAngle, 8.0f * dt);
        
        // Walk bob animation - oscillate based on distance traveled
        float walkSpeed = 15.0f;
        m_walkCycle += dt * walkSpeed;
        
        // Camera does NOT move at all - for testing
        // glm::vec2 heroXZ = { m_heroRenderPos.x, m_heroRenderPos.z };
        // m_cam.setPosition(heroXZ);
        
    } else if (!m_moveQueue.empty() && 
               m_moveQueueIdx + 1 < static_cast<int>(m_moveQueue.size())) {
        // Arrived at waypoint — snap render pos and advance to next hex
        m_heroRenderPos = m_heroTargetPos;
        
        // Brief pause at each hex (like HoMM stepping rhythm)
        m_walkCycle = 0.0f;
        
        ++m_moveQueueIdx;
        float hx, hz;
        m_moveQueue[m_moveQueueIdx].toWorld(HEX_SIZE, hx, hz);
        m_heroTargetPos = { hx, 0.0f, hz };
    } else {
        // Hero arrived at final destination
        if (m_hasPendingVisit) {
            onHeroVisit(m_pendingVisit);
            m_hasPendingVisit = false;
        }
        
        // Check for battle terrain
        MapTile* currentTile = m_map.tileAt(m_hero.pos);
        if (currentTile && currentTile->terrain == Terrain::Battle) {
            MapObjectDef battleObj{ m_hero.pos, ObjType::Dungeon, "Battle Encounter" };
            m_pendingCombat = std::make_shared<CombatOutcome>();
            Application::get().pushState(
                std::make_unique<CombatState>(buildPlayerArmy(), buildEnemyArmy(battleObj),
                                              m_pendingCombat));
        }
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void AdventureState::render() {
    auto& app = Application::get();
    m_cam.resize(app.width(), app.height());
    
    glm::mat4 view = m_cam.viewMatrix();
    glm::mat4 proj = m_cam.projMatrix();
    glm::mat4 vp   = m_cam.viewProjMatrix();

    m_hexRenderer.beginFrame(vp, HEX_SIZE, {0.4f, 1.0f, 0.3f}, {1.0f, 0.92f, 0.70f}, {0.35f, 0.30f, 0.22f}, {m_cam.position().x, 0.0f, m_cam.position().y});
    renderTerrain();
    renderPathPreview();
    m_hexRenderer.endFrame();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    renderObjects();
    renderHero();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (m_showHUD) {
        m_hud.render(app.width(), app.height(),
                     m_turnManager.day(),
                     m_turnManager.month(),
                     m_turnManager.weekOfMonth(),
                     m_hero.movesLeft, m_hero.movesMax,
                     static_cast<int>(m_objectControl.size()),
                     m_hero.pos.q, m_hero.pos.r, m_infiniteMoves,
                     m_turnManager.playerFaction().treasury[Resource::Gold]);

        // ── Army panel (bottom-right) ─────────────────────────────────────────
        int sw = app.width(), sh = app.height();
        float sc      = sh / 600.0f;
        float slotW   = 65 * sc;
        float slotH   = 52 * sc;
        float gap     = 4  * sc;
        float labelH  = 16 * sc;
        float totalW  = Hero::ARMY_SLOTS * (slotW + gap) - gap;
        float panX    = sw - totalW - 10 * sc;
        float panY    = sh - slotH - 10 * sc;

        m_hud.begin(sw, sh);

        // Backing strip — tall enough to include the "ARMY" label row above slots.
        float stripY = panY - labelH - 6 * sc;
        float stripH = slotH + labelH + 14 * sc;
        m_hud.drawRect(panX - 6*sc, stripY, totalW + 12*sc, stripH,
                       {0.0f, 0.0f, 0.0f, 0.72f});
        m_hud.drawText(panX, stripY + 3*sc, sc * 1.05f,
                       "ARMY", {0.65f, 0.52f, 0.25f, 1.0f});

        char slotBuf[32];
        for (int i = 0; i < Hero::ARMY_SLOTS; ++i) {
            const ArmySlot& slot = m_hero.army[i];
            float sx = panX + i * (slotW + gap);

            if (slot.isEmpty()) {
                m_hud.drawRect(sx, panY, slotW, slotH, {0.08f, 0.06f, 0.04f, 0.50f});
            } else {
                // Golden border so occupied slots stand out clearly.
                m_hud.drawRect(sx, panY, slotW, slotH, {0.65f, 0.48f, 0.15f, 1.0f});
                m_hud.drawRect(sx + 2*sc, panY + 2*sc,
                               slotW - 4*sc, slotH - 4*sc, {0.20f, 0.14f, 0.07f, 0.95f});

                // Show the last word of the unit name so "Desert Archer" → "Archer",
                // "Skeleton Warrior" → "Warrior", "Mummy" → "Mummy".
                const std::string& fullName = slot.unitType->name;
                auto spacePos = fullName.rfind(' ');
                const char* label = (spacePos != std::string::npos)
                    ? fullName.c_str() + spacePos + 1
                    : fullName.c_str();
                std::snprintf(slotBuf, sizeof(slotBuf), "%.7s", label);
                m_hud.drawText(sx + 3*sc, panY + 5*sc, sc * 0.85f,
                               slotBuf, {0.95f, 0.88f, 0.70f, 1.0f});
                std::snprintf(slotBuf, sizeof(slotBuf), "%d", slot.count);
                m_hud.drawText(sx + 3*sc, panY + 25*sc, sc * 1.6f,
                               slotBuf, {0.35f, 0.92f, 0.35f, 1.0f});
            }
        }

        // ── SC party panel (above army panel) ────────────────────────────────
        float scSlotW = slotW;
        float scSlotH = 38 * sc;
        float scTotalW = Hero::SC_SLOTS * (scSlotW + gap) - gap;
        float scPanX  = sw - scTotalW - 10 * sc;
        float scPanY  = panY - scSlotH - 8 * sc;

        m_hud.drawRect(scPanX - 6*sc, scPanY - 6*sc, scTotalW + 12*sc, scSlotH + 12*sc,
                       {0.0f, 0.0f, 0.0f, 0.50f});

        for (int i = 0; i < Hero::SC_SLOTS; ++i) {
            float sx = scPanX + i * (scSlotW + gap);
            bool occupied = i < static_cast<int>(m_hero.specials.size());

            glm::vec4 bg = occupied
                ? glm::vec4{0.10f, 0.08f, 0.16f, 0.90f}   // dark purple when filled
                : glm::vec4{0.06f, 0.05f, 0.10f, 0.45f};  // dimmer empty slot
            m_hud.drawRect(sx, scPanY, scSlotW, scSlotH, bg);

            if (occupied) {
                const SpecialCharacter& sc_char = m_hero.specials[i];
                std::snprintf(slotBuf, sizeof(slotBuf), "%.7s", sc_char.name.c_str());
                m_hud.drawText(sx + 3*sc, scPanY + 4*sc,  sc * 0.80f,
                               slotBuf, {0.90f, 0.80f, 1.00f, 1.0f});
                std::snprintf(slotBuf, sizeof(slotBuf), "%.6s", sc_char.archetype.c_str());
                m_hud.drawText(sx + 3*sc, scPanY + 20*sc, sc * 0.75f,
                               slotBuf, {0.65f, 0.55f, 0.85f, 1.0f});
            } else {
                m_hud.drawText(sx + 3*sc, scPanY + 12*sc, sc * 0.75f,
                               "- SC -", {0.30f, 0.25f, 0.40f, 0.70f});
            }
        }
    }

    // ── End Turn button — top-left, always visible ───────────────────────────
    {
        int sw = app.width(), sh = app.height();
        float sc = sh / 600.0f;
        float btnX = 10 * sc, btnY = 10 * sc;
        float btnW = 110 * sc, btnH = 36 * sc;

        m_hud.begin(sw, sh);

        // Glow gold when hero is out of moves (nudge the player to end turn).
        bool outOfMoves = !m_infiniteMoves && m_hero.movesLeft == 0;
        glm::vec4 borderCol = outOfMoves
            ? glm::vec4{0.95f, 0.80f, 0.20f, 1.0f}
            : glm::vec4{0.50f, 0.40f, 0.18f, 1.0f};
        glm::vec4 bgCol = outOfMoves
            ? glm::vec4{0.25f, 0.18f, 0.04f, 0.97f}
            : glm::vec4{0.10f, 0.08f, 0.04f, 0.90f};

        m_hud.drawRect(btnX, btnY, btnW, btnH, borderCol);
        m_hud.drawRect(btnX + 2*sc, btnY + 2*sc, btnW - 4*sc, btnH - 4*sc, bgCol);
        m_hud.drawText(btnX + 8*sc, btnY + 10*sc, sc * 1.3f,
                       "END TURN", {1.0f, 0.95f, 0.70f, 1.0f});
    }

    if (m_isDefeated) {
        int w = app.width(), h = app.height();
        m_hud.begin(w, h);
        // Dark translucent veil.
        m_hud.drawRect(0, 0, static_cast<float>(w), static_cast<float>(h),
                       {0.0f, 0.0f, 0.0f, 0.65f});
        float sc = h / 600.0f;
        // y-DOWN: smaller y = higher on screen; larger y = lower.
        m_hud.drawText(w * 0.5f - 90 * sc, h * 0.5f - 20 * sc, sc * 3.0f,
                       "DEFEATED", {0.9f, 0.15f, 0.1f, 1.0f});
        m_hud.drawText(w * 0.5f - 115 * sc, h * 0.5f + 30 * sc, sc * 1.2f,
                       "Press R to restart  |  ESC to exit", {0.85f, 0.8f, 0.7f, 1.0f});
    }
}

void AdventureState::renderTerrain() {
    for (const auto& [coord, tile] : m_map) {
        RenderOffset off = m_offsets.forTerrain(coord, tile.terrain);
        GLuint tex      = m_hexRenderer.terrainTex(tile.terrain);
        glm::vec3 color = (tex != 0) ? glm::vec3(1.0f) : terrainColor(tile.terrain);
        float h         = terrainHeight(tile.terrain) + off.dy;
        m_hexRenderer.drawTile(coord, color, HEX_SIZE, h, tex, {off.dx, off.dz});
    }

    // Hover outline (yellow).
    if (m_hasHovered) {
        float h = 0.0f;
        if (const MapTile* t = m_map.tileAt(m_hovered)) h = terrainHeight(t->terrain);
        m_hexRenderer.drawOutline(m_hovered, {1.0f, 0.9f, 0.3f}, HEX_SIZE * 0.95f, h);
    }
}

void AdventureState::renderPathPreview() {
    for (size_t i = 1; i < m_previewPath.size(); ++i) {
        float h = 0.0f;
        if (const MapTile* t = m_map.tileAt(m_previewPath[i])) h = terrainHeight(t->terrain);
        bool reachable = (static_cast<int>(i) <= m_hero.movesLeft);
        glm::vec3 col  = reachable
            ? glm::vec3{0.3f, 0.7f, 1.0f}
            : glm::vec3{0.9f, 0.2f, 0.2f};
        m_hexRenderer.drawOutline(m_previewPath[i], col, HEX_SIZE * 0.95f, h);
    }
}

void AdventureState::renderObjects() {
    for (const auto& obj : m_map.objects()) {
        RenderOffset off = m_offsets.forObject(obj.pos, obj.type);
        float wx, wz;
        obj.pos.toWorld(HEX_SIZE, wx, wz);
        wx += off.dx;  wz += off.dz;

        if (obj.type == ObjType::Dungeon) {
            // Hide entrance sprite once the dungeon is cleared.
            auto it = m_objectControl.find(obj.pos);
            if (it != m_objectControl.end() && it->second.guardDefeated) continue;
            m_dungeonSpriteRenderer.draw({ wx, off.dy, wz }, HEX_SIZE * 1.4f,
                                         m_cam.viewMatrix(), m_cam.projMatrix());
        } else {
            m_buildingSpriteRenderer.draw({ wx, off.dy, wz }, HEX_SIZE * 1.8f,
                                          m_cam.viewMatrix(), m_cam.projMatrix());
        }
    }
}

void AdventureState::renderHero() {
    HexCoord spriteHex = HexCoord::fromWorld(
        m_heroRenderPos.x, m_heroRenderPos.z, HEX_SIZE);

    if (m_heroSelected)
        m_hexRenderer.drawOutline(spriteHex, {1.0f, 1.0f, 1.0f}, HEX_SIZE * 0.65f);

    glm::vec3 moveCol = m_hero.hasMoves()
        ? glm::vec3{0.2f, 1.0f, 0.3f}
        : glm::vec3{0.45f, 0.45f, 0.45f};
    m_hexRenderer.drawOutline(spriteHex, moveCol, HEX_SIZE * 0.65f);

    // Apply walk bobbing animation when moving
    glm::vec3 renderPos = m_heroRenderPos;
    if (m_isWalking) {
        float bob = std::sin(m_walkCycle) * WALK_BOB_HEIGHT;
        renderPos.y += bob;
    }

    m_spriteRenderer.draw(renderPos, HEX_SIZE,
                          m_cam.viewMatrix(), m_cam.projMatrix());
}
