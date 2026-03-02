#pragma once
#include "core/StateMachine.h"
#include "render/HexRenderer.h"
#include "render/SpriteRenderer.h"
#include "hex/HexGrid.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

// ── Terrain ───────────────────────────────────────────────────────────────────

enum class Terrain : uint8_t {
    Sand, Dune, Rock, Oasis, Ruins, Obsidian,
};

struct MapTile {
    Terrain terrain  = Terrain::Sand;
    bool    passable = true;
};

inline glm::vec3 terrainColor(Terrain t) {
    switch (t) {
        case Terrain::Sand:     return { 0.87f, 0.74f, 0.42f };
        case Terrain::Dune:     return { 0.72f, 0.55f, 0.25f };
        case Terrain::Rock:     return { 0.55f, 0.48f, 0.38f };
        case Terrain::Oasis:    return { 0.30f, 0.58f, 0.42f };
        case Terrain::Ruins:    return { 0.70f, 0.63f, 0.50f };
        case Terrain::Obsidian: return { 0.18f, 0.16f, 0.20f };
    }
    return { 1, 0, 1 };
}

// ── Map objects ───────────────────────────────────────────────────────────────

enum class ObjType { Town, Dungeon, GoldMine, CrystalMine, Artifact };

struct MapObject {
    HexCoord    pos;
    ObjType     type;
    std::string name;
    bool        visited = false;

    glm::vec3 color() const {
        switch (type) {
            case ObjType::Town:        return { 0.20f, 0.80f, 0.90f }; // cyan
            case ObjType::Dungeon:     return { 0.75f, 0.12f, 0.12f }; // red
            case ObjType::GoldMine:    return { 0.95f, 0.70f, 0.10f }; // gold
            case ObjType::CrystalMine: return { 0.50f, 0.80f, 0.95f }; // light blue
            case ObjType::Artifact:    return { 1.00f, 1.00f, 1.00f }; // white
        }
        return { 1, 0, 1 };
    }
    const char* typeName() const {
        switch (type) {
            case ObjType::Town:        return "Town";
            case ObjType::Dungeon:     return "Dungeon";
            case ObjType::GoldMine:    return "Gold Mine";
            case ObjType::CrystalMine: return "Crystal Mine";
            case ObjType::Artifact:    return "Artifact";
        }
        return "?";
    }
};

// ── Hero ─────────────────────────────────────────────────────────────────────

struct Hero {
    HexCoord pos       { 0, 0 };
    int      movesMax  { 8 };
    int      movesLeft { 8 };
};

// ── State ─────────────────────────────────────────────────────────────────────

class AdventureState final : public GameState {
public:
    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    void buildMap();
    void placeObjects();
    void moveHero(const HexCoord& dest);
    void endTurn();
    void onHeroVisit(const HexCoord& coord);

    glm::mat4 viewMatrix()     const;
    glm::mat4 projMatrix()     const;
    glm::mat4 viewProjMatrix() const;
    glm::vec2 screenToWorld(int px, int py) const; // returns world XZ

    HexRenderer          m_hexRenderer;
    SpriteRenderer       m_spriteRenderer;
    HexGrid<MapTile>     m_grid;
    std::vector<MapObject> m_objects;

    Hero       m_hero;
    bool       m_heroSelected = false;

    // Smooth movement animation
    glm::vec3  m_heroRenderPos { 0.0f, 0.0f, 0.0f }; // current drawn position
    glm::vec3  m_heroTargetPos { 0.0f, 0.0f, 0.0f }; // destination world position

    // Path preview (A* result from hero to hovered tile)
    std::vector<HexCoord> m_previewPath;

    // Camera
    glm::vec2  m_camPos { 0.0f, 0.0f };
    float      m_zoom   { 8.0f };
    float      m_hexSize{ 1.0f };
    int        m_vpW    = 1280;
    int        m_vpH    = 720;

    // Hovered tile
    HexCoord   m_hovered;
    bool       m_hasHovered = false;

    // Key states
    bool m_keyW=false, m_keyA=false, m_keyS=false, m_keyD=false;

    int  m_day = 1;

    static constexpr int   MAP_RADIUS      = 12;
    static constexpr float CAM_SPEED       = 8.0f;
    static constexpr float ZOOM_STEP       = 1.5f;
    static constexpr float HERO_MOVE_SPEED = 5.0f; // world units per second
};
