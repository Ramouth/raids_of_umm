#pragma once
#include "core/StateMachine.h"
#include "castle/RecruitResult.h"
#include "render/SpriteRenderer.h"
#include "render/HUDRenderer.h"
#include "render/Texture.h"
#include "world/TownState.h"
#include "world/Resources.h"
#include "world/UnitType.h"
#include "hero/Hero.h"
#include <glad/glad.h>
#include <memory>
#include <unordered_map>
#include <vector>

/*
 * CastleState — recruit screen pushed by AdventureState when hero visits a Town.
 *
 * Holds references into AdventureState-owned data:
 *   town     — TownState for this specific town (recruitPool, buildings)
 *   treasury — player faction's ResourcePool (deducted on purchase)
 *   hero     — read-only view of the hero's current army (display + capacity check)
 *
 * On exit, m_result->hired contains all units purchased this visit.
 * AdventureState reads the result in onResume() and adds units to the hero army.
 */
class CastleState final : public GameState {
public:
    CastleState(std::string               castleName,
                TownState&                town,
                ResourcePool&             treasury,
                const Hero&               hero,
                std::shared_ptr<RecruitResult> result);

    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    // Staging helpers — +/- adjust the pending cart, BUY commits it.
    void incrementPending(int rowIdx);
    void decrementPending(int rowIdx);
    void confirmBuy();

    // True if the hero can carry at least one more of this unit type.
    bool heroCanAccept(const UnitType* u) const;
    // Total resource cost of everything currently staged.
    ResourcePool pendingCost() const;

    std::string      m_castleName;
    TownState&        m_town;
    ResourcePool&     m_treasury;
    const Hero&       m_hero;
    std::shared_ptr<RecruitResult> m_result;

    // Staged quantities — units queued but not yet purchased.
    std::unordered_map<std::string, int> m_pending;

    // Ordered list of unit types shown in the recruit list (from ResourceManager).
    std::vector<const UnitType*> m_rows;

    SpriteRenderer m_spriteRenderer;
    HUDRenderer    m_hud;
    GLuint         m_interiorTex = 0;
    bool           m_initialized = false;
};
