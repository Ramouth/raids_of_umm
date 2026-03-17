#include "CastleState.h"
#include "core/Application.h"
#include "core/ResourceManager.h"
#include "render/HUDRenderer.h"
#include "render/Texture.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <cstdio>

// ── Constructor ───────────────────────────────────────────────────────────────

CastleState::CastleState(std::string castleName,
                         TownState& town,
                         ResourcePool& treasury,
                         const Hero& hero,
                         std::shared_ptr<RecruitResult> result)
    : m_castleName(std::move(castleName))
    , m_town(town)
    , m_treasury(treasury)
    , m_hero(hero)
    , m_result(std::move(result))
{}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void CastleState::onEnter() {
    m_spriteRenderer.init();
    m_interiorTex = loadTexturePNG("assets/textures/buildings/castle_interior.png");
    m_hud.init();

    auto& rm = Application::get().resources();
    if (rm.loaded())
        m_rows = rm.unitsByTier();

    m_pending.clear();
    m_initialized = true;
    std::cout << "[Castle] Entered " << m_castleName
              << "  (treasury: " << m_treasury[Resource::Gold] << " gold)\n";
}

void CastleState::onExit() {
    if (m_interiorTex) { glDeleteTextures(1, &m_interiorTex); m_interiorTex = 0; }
    std::cout << "[Castle] Exited — hired " << m_result->hired.size() << " unit type(s).\n";
}

void CastleState::update(float /*dt*/) {}

// ── Staging logic ─────────────────────────────────────────────────────────────

bool CastleState::heroCanAccept(const UnitType* u) const {
    return m_hero.findStack(u->id) != nullptr || !m_hero.armyFull();
}

ResourcePool CastleState::pendingCost() const {
    ResourcePool total;
    for (const auto& u : m_rows) {
        auto it = m_pending.find(u->id);
        if (it == m_pending.end() || it->second <= 0) continue;
        for (int r = 0; r < RESOURCE_COUNT; ++r)
            total.amounts[r] += it->second * u->cost.amounts[r];
    }
    return total;
}

void CastleState::incrementPending(int rowIdx) {
    if (rowIdx < 0 || rowIdx >= static_cast<int>(m_rows.size())) return;
    const UnitType* u = m_rows[rowIdx];

    // Pool availability.
    auto poolIt = m_town.recruitPool.find(u->id);
    int poolAvail = (poolIt != m_town.recruitPool.end()) ? poolIt->second : 0;
    int staged    = m_pending[u->id];
    if (staged >= poolAvail) return;  // nothing left in pool

    // Hero must be able to carry this unit type.
    if (!heroCanAccept(u)) return;

    // Check treasury covers everything already staged PLUS one more of this unit.
    ResourcePool costAfter = pendingCost();
    for (int r = 0; r < RESOURCE_COUNT; ++r)
        costAfter.amounts[r] += u->cost.amounts[r];
    if (!m_treasury.canAfford(costAfter)) return;

    m_pending[u->id]++;
}

void CastleState::decrementPending(int rowIdx) {
    if (rowIdx < 0 || rowIdx >= static_cast<int>(m_rows.size())) return;
    const UnitType* u = m_rows[rowIdx];
    int& staged = m_pending[u->id];
    if (staged > 0) staged--;
}

void CastleState::confirmBuy() {
    bool anyBought = false;

    for (const auto& u : m_rows) {
        auto pendIt = m_pending.find(u->id);
        if (pendIt == m_pending.end() || pendIt->second <= 0) continue;
        int count = pendIt->second;

        auto poolIt = m_town.recruitPool.find(u->id);
        if (poolIt == m_town.recruitPool.end() || poolIt->second < count) {
            std::cout << "  [Castle] Pool shrank — partial buy for " << u->name << "\n";
            count = (poolIt != m_town.recruitPool.end()) ? poolIt->second : 0;
        }
        if (count <= 0) continue;

        ResourcePool rowCost;
        for (int r = 0; r < RESOURCE_COUNT; ++r)
            rowCost.amounts[r] = u->cost.amounts[r] * count;
        if (!m_treasury.canAfford(rowCost)) continue;

        m_treasury -= rowCost;
        poolIt->second -= count;

        bool found = false;
        for (auto& s : m_result->hired) {
            if (s.type == u) { s.count += count; found = true; break; }
        }
        if (!found) m_result->hired.push_back({u, count});

        std::cout << "  [Castle] Bought " << count << "x " << u->name
                  << " — gold now " << m_treasury[Resource::Gold]
                  << ", pool: " << poolIt->second << "\n";
        anyBought = true;
    }

    m_pending.clear();
    if (!anyBought)
        std::cout << "  [Castle] Nothing to buy.\n";
}

// ── Layout helpers ────────────────────────────────────────────────────────────
// All Y values are y-DOWN pixel coords (y=0 = screen top).
// The UI shader does `ndc.y = -ndc.y`, so pixel (0,0) maps to the top-left corner.

namespace {

struct Layout {
    float sc;
    float panelX, panelY, panelW, panelH;
    float areaX, areaW, areaH;
    float rowH;
    // Vertical positions (top edges, y-DOWN)
    float titleY, tY, tY2, sepY, hdrY, listStartY;
    // Buttons at the bottom
    float buyX,  buyY,  buyW,  buyH;
    float exitX, exitY, exitW, exitH;
    // Column X positions
    float colUnit, colHave, colPool, colCost;
    float colMinus, colStaged, colPlus;
};

Layout computeLayout(int w, int h) {
    Layout l;
    l.sc     = h / 600.0f;
    l.panelX = 20  * l.sc;
    l.panelY = 20  * l.sc;
    l.panelW = 320 * l.sc;
    l.panelH = h - 40 * l.sc;
    l.areaX  = l.panelX + l.panelW + 15 * l.sc;
    l.areaW  = w - l.areaX - 20 * l.sc;
    l.areaH  = l.panelH;
    l.rowH   = 28 * l.sc;

    // Top-to-bottom (increasing y = further down screen).
    l.titleY     = l.panelY      + 10 * l.sc;
    l.tY         = l.titleY      + 26 * l.sc;
    l.tY2        = l.tY          + 16 * l.sc;
    l.sepY       = l.tY2         + 12 * l.sc;
    l.hdrY       = l.sepY        +  8 * l.sc;
    l.listStartY = l.hdrY        + l.rowH;

    // Bottom buttons — EXIT at very bottom, BUY above it.
    l.exitW = 100 * l.sc;
    l.exitH =  30 * l.sc;
    l.exitX = l.panelX + (l.panelW - l.exitW) / 2.0f;
    l.exitY = l.panelY + l.panelH - l.exitH - 8 * l.sc;

    l.buyW  = l.panelW - 20 * l.sc;
    l.buyH  =  34 * l.sc;
    l.buyX  = l.panelX + 10 * l.sc;
    l.buyY  = l.exitY - l.buyH - 8 * l.sc;

    // Column X positions.
    l.colUnit   = l.panelX +  10 * l.sc;
    l.colHave   = l.panelX + 122 * l.sc;
    l.colPool   = l.panelX + 154 * l.sc;
    l.colCost   = l.panelX + 192 * l.sc;
    // +/- controls for staging
    l.colMinus  = l.panelX + 238 * l.sc;   // "-" button width 22*sc → ends at 260
    l.colStaged = l.panelX + 262 * l.sc;   // count display width 22*sc → ends at 284
    l.colPlus   = l.panelX + 286 * l.sc;   // "+" button width 22*sc → ends at 308
    return l;
}

} // namespace

// ── Render ────────────────────────────────────────────────────────────────────

void CastleState::render() {
    auto& app = Application::get();
    int w = app.width();
    int h = app.height();

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const Layout l = computeLayout(w, h);
    m_hud.begin(w, h);

    // ── Left panel background ─────────────────────────────────────────────────
    m_hud.drawRect(l.panelX, l.panelY, l.panelW, l.panelH, {0.1f, 0.06f, 0.03f, 0.92f});
    m_hud.drawRect(l.panelX, l.panelY,                      l.panelW, 5 * l.sc, {0.65f, 0.45f, 0.2f, 1.0f});
    m_hud.drawRect(l.panelX, l.panelY + l.panelH - 5*l.sc,  l.panelW, 5 * l.sc, {0.65f, 0.45f, 0.2f, 1.0f});

    // ── Title ─────────────────────────────────────────────────────────────────
    m_hud.drawText(l.panelX + 10*l.sc, l.titleY, l.sc * 2.0f,
                   m_castleName.c_str(), {1.0f, 0.9f, 0.5f, 1.0f});

    // ── Treasury ──────────────────────────────────────────────────────────────
    char buf[128];
    int totalGold = pendingCost()[Resource::Gold];
    if (totalGold > 0)
        std::snprintf(buf, sizeof(buf), "Gold: %d  (-pending: %d)",
                      m_treasury[Resource::Gold], totalGold);
    else
        std::snprintf(buf, sizeof(buf), "Gold: %d", m_treasury[Resource::Gold]);
    m_hud.drawText(l.panelX + 10*l.sc, l.tY, l.sc * 1.2f, buf, {1.0f, 0.85f, 0.2f, 1.0f});

    std::snprintf(buf, sizeof(buf), "Wood:%d  Stone:%d  Obsidian:%d  Crystal:%d",
                  m_treasury[Resource::Wood],
                  m_treasury[Resource::Stone],
                  m_treasury[Resource::Obsidian],
                  m_treasury[Resource::Crystal]);
    m_hud.drawText(l.panelX + 10*l.sc, l.tY2, l.sc * 1.0f, buf, {0.75f, 0.75f, 0.65f, 1.0f});

    // ── Separator ─────────────────────────────────────────────────────────────
    m_hud.drawRect(l.panelX + 10*l.sc, l.sepY, l.panelW - 20*l.sc, 2*l.sc,
                   {0.5f, 0.4f, 0.2f, 1.0f});

    // ── Column header ─────────────────────────────────────────────────────────
    m_hud.drawText(l.colUnit,   l.hdrY, l.sc * 0.9f, "UNIT",  {0.55f, 0.55f, 0.45f, 1.0f});
    m_hud.drawText(l.colHave,   l.hdrY, l.sc * 0.9f, "HAVE",  {0.45f, 0.65f, 0.45f, 1.0f});
    m_hud.drawText(l.colPool,   l.hdrY, l.sc * 0.9f, "POOL",  {0.55f, 0.55f, 0.45f, 1.0f});
    m_hud.drawText(l.colCost,   l.hdrY, l.sc * 0.9f, "COST",  {0.55f, 0.55f, 0.45f, 1.0f});
    m_hud.drawText(l.colMinus,  l.hdrY, l.sc * 0.9f, "-  N  +", {0.55f, 0.55f, 0.45f, 1.0f});

    // ── Unit rows ─────────────────────────────────────────────────────────────
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        const UnitType* u  = m_rows[i];
        float           ry = l.listStartY + i * l.rowH;

        auto poolIt = m_town.recruitPool.find(u->id);
        int avail   = (poolIt != m_town.recruitPool.end()) ? poolIt->second : 0;
        int staged  = 0;
        { auto pit = m_pending.find(u->id); if (pit != m_pending.end()) staged = pit->second; }

        const ArmySlot* existing = m_hero.findStack(u->id);
        int inArmy = existing ? existing->count : 0;

        bool canAdd = (staged < avail) && heroCanAccept(u) && ([&]{
            ResourcePool costAfter = pendingCost();
            for (int r = 0; r < RESOURCE_COUNT; ++r)
                costAfter.amounts[r] += u->cost.amounts[r];
            return m_treasury.canAfford(costAfter);
        })();
        bool canSub = staged > 0;

        // Row background
        glm::vec4 rowBg = (i % 2 == 0)
            ? glm::vec4{0.13f, 0.08f, 0.04f, 0.6f}
            : glm::vec4{0.10f, 0.06f, 0.03f, 0.4f};
        if (staged > 0)
            rowBg = {0.10f, 0.16f, 0.08f, 0.7f};  // green tint when staged
        m_hud.drawRect(l.panelX + 6*l.sc, ry, l.panelW - 12*l.sc, l.rowH - 2*l.sc, rowBg);

        // Unit name
        glm::vec4 nameCol = (avail > 0 && heroCanAccept(u))
            ? glm::vec4{0.95f, 0.9f, 0.7f, 1.0f}
            : glm::vec4{0.45f, 0.45f, 0.40f, 1.0f};
        m_hud.drawText(l.colUnit, ry + 8*l.sc, l.sc * 1.1f, u->name.c_str(), nameCol);

        // HAVE
        std::snprintf(buf, sizeof(buf), "%d", inArmy);
        m_hud.drawText(l.colHave, ry + 8*l.sc, l.sc * 1.1f, buf,
                       inArmy > 0 ? glm::vec4{0.35f, 0.85f, 0.35f, 1.0f}
                                  : glm::vec4{0.35f, 0.35f, 0.30f, 1.0f});

        // POOL
        std::snprintf(buf, sizeof(buf), "%d", avail);
        m_hud.drawText(l.colPool, ry + 8*l.sc, l.sc * 1.1f, buf,
                       avail > 0 ? glm::vec4{0.45f, 0.9f, 0.45f, 1.0f}
                                 : glm::vec4{0.4f, 0.35f, 0.35f, 1.0f});

        // COST
        std::snprintf(buf, sizeof(buf), "%dg", u->cost[Resource::Gold]);
        m_hud.drawText(l.colCost, ry + 8*l.sc, l.sc * 1.1f, buf, {1.0f, 0.85f, 0.2f, 1.0f});

        // "-" button
        float btnH = l.rowH - 8*l.sc;
        float btnY = ry + 4*l.sc;
        float btnW = 22*l.sc;
        m_hud.drawRect(l.colMinus, btnY, btnW, btnH,
                       canSub ? glm::vec4{0.50f, 0.25f, 0.08f, 1.0f}
                              : glm::vec4{0.20f, 0.18f, 0.16f, 0.9f});
        m_hud.drawText(l.colMinus + 7*l.sc, btnY + 4*l.sc, l.sc * 1.1f, "-",
                       {1.0f, 1.0f, 1.0f, 1.0f});

        // Staged count
        std::snprintf(buf, sizeof(buf), "%d", staged);
        glm::vec4 stagedCol = staged > 0
            ? glm::vec4{0.35f, 1.0f, 0.35f, 1.0f}
            : glm::vec4{0.45f, 0.45f, 0.40f, 1.0f};
        m_hud.drawText(l.colStaged + 6*l.sc, btnY + 4*l.sc, l.sc * 1.1f, buf, stagedCol);

        // "+" button
        m_hud.drawRect(l.colPlus, btnY, btnW, btnH,
                       canAdd ? glm::vec4{0.22f, 0.50f, 0.16f, 1.0f}
                              : glm::vec4{0.20f, 0.18f, 0.16f, 0.9f});
        m_hud.drawText(l.colPlus + 7*l.sc, btnY + 4*l.sc, l.sc * 1.1f, "+",
                       {1.0f, 1.0f, 1.0f, 1.0f});
    }

    // ── BUY button ────────────────────────────────────────────────────────────
    int pendGold  = pendingCost()[Resource::Gold];
    bool hasPending = pendGold > 0;
    glm::vec4 buyBg = hasPending
        ? glm::vec4{0.22f, 0.52f, 0.16f, 1.0f}
        : glm::vec4{0.18f, 0.18f, 0.16f, 0.9f};
    m_hud.drawRect(l.buyX, l.buyY, l.buyW, l.buyH, buyBg);
    if (hasPending)
        std::snprintf(buf, sizeof(buf), "BUY  (cost: %d gold)", pendGold);
    else
        std::snprintf(buf, sizeof(buf), "BUY");
    m_hud.drawText(l.buyX + 10*l.sc, l.buyY + 10*l.sc, l.sc * 1.3f, buf,
                   hasPending ? glm::vec4{1.0f, 0.95f, 0.75f, 1.0f}
                              : glm::vec4{0.45f, 0.45f, 0.40f, 1.0f});

    // ── EXIT button ───────────────────────────────────────────────────────────
    m_hud.drawRect(l.exitX, l.exitY, l.exitW, l.exitH, {0.55f, 0.2f, 0.08f, 1.0f});
    m_hud.drawRect(l.exitX, l.exitY + l.exitH - 3*l.sc, l.exitW, 3*l.sc,
                   {0.35f, 0.1f, 0.02f, 1.0f});
    m_hud.drawText(l.exitX + (l.exitW - 30*l.sc) / 2.0f, l.exitY + 9*l.sc,
                   l.sc * 1.2f, "EXIT", {1.0f, 0.95f, 0.75f, 1.0f});

    // ── Right castle area ─────────────────────────────────────────────────────
    if (m_interiorTex)
        m_hud.drawTexturedRect(l.areaX, l.panelY, l.areaW, l.areaH, m_interiorTex);
    else
        m_hud.drawRect(l.areaX, l.panelY, l.areaW, l.areaH, {0.05f, 0.08f, 0.12f, 1.0f});

    m_hud.drawRect(l.areaX, l.panelY,                     l.areaW, 4*l.sc, {0.45f, 0.35f, 0.2f, 1.0f});
    m_hud.drawRect(l.areaX, l.panelY + l.areaH - 4*l.sc,  l.areaW, 4*l.sc, {0.45f, 0.35f, 0.2f, 1.0f});

    glDisable(GL_BLEND);
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool CastleState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE:
                Application::get().popState();
                return true;
            case SDLK_RETURN:
                confirmBuy();
                return true;
            default: break;
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        auto& app = Application::get();
        const Layout l = computeLayout(app.width(), app.height());

        // y-DOWN pixel coords — same as SDL, no flip needed.
        float mx = static_cast<float>(e->button.x);
        float my = static_cast<float>(e->button.y);

        // BUY button
        if (mx >= l.buyX  && mx <= l.buyX  + l.buyW &&
            my >= l.buyY  && my <= l.buyY  + l.buyH) {
            confirmBuy();
            return true;
        }

        // EXIT button
        if (mx >= l.exitX && mx <= l.exitX + l.exitW &&
            my >= l.exitY && my <= l.exitY + l.exitH) {
            Application::get().popState();
            return true;
        }

        // Per-row "-" and "+" buttons
        float btnW = 22 * l.sc;
        for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
            float ry   = l.listStartY + i * l.rowH;
            float btnY = ry + 4 * l.sc;
            float btnH = l.rowH - 8 * l.sc;

            if (my < btnY || my > btnY + btnH) continue;

            if (mx >= l.colMinus && mx <= l.colMinus + btnW) {
                decrementPending(i);
                return true;
            }
            if (mx >= l.colPlus  && mx <= l.colPlus  + btnW) {
                incrementPending(i);
                return true;
            }
        }
    }

    return false;
}
