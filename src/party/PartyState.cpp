#include "PartyState.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <cstdio>
#include <algorithm>

PartyState::PartyState(const Hero& hero) : m_hero(hero) {}

void PartyState::onEnter() {
    m_hud.init();
}

void PartyState::onExit() {}

void PartyState::update(float /*dt*/) {}

// ── Layout constants (all in virtual 600-tall units, scaled by sc) ────────────

namespace {

struct PLayout {
    float sc;
    float pad;
    // Two equal panels side by side with a gap
    float leftX, leftY, leftW, leftH;
    float rightX, rightY, rightW, rightH;
    // EXIT button
    float exitX, exitY, exitW, exitH;
    // Row heights
    float unitRowH;
    float scCardH;
    float barW, barH;
};

PLayout computeLayout(int w, int h) {
    PLayout l;
    l.sc     = h / 600.0f;
    l.pad    = 16.f * l.sc;

    float titleH = 36.f * l.sc;
    float panelY = l.pad + titleH;
    float panelH = h - panelY - l.pad;
    float half   = (w - l.pad * 3.f) / 2.f;

    l.leftX = l.pad;
    l.leftY = panelY;
    l.leftW = half;
    l.leftH = panelH;

    l.rightX = l.pad * 2.f + half;
    l.rightY = panelY;
    l.rightW = half;
    l.rightH = panelH;

    l.unitRowH = 34.f * l.sc;

    // 3 SC cards stacked in the right panel (minus header + gaps)
    float hdrH = 24.f * l.sc;
    float gap  =  8.f * l.sc;
    l.scCardH  = (l.rightH - hdrH - 2.f * gap) / 3.f;

    l.barW = l.rightW * 0.55f;
    l.barH = 9.f * l.sc;

    l.exitW = 72.f * l.sc;
    l.exitH = 24.f * l.sc;
    l.exitX = w - l.exitW - l.pad;
    l.exitY = l.pad + 6.f * l.sc;

    return l;
}

float scCardTopY(const PLayout& l, int i) {
    float hdrH = 24.f * l.sc;
    float gap  =  8.f * l.sc;
    return l.rightY + hdrH + i * (l.scCardH + gap);
}

} // namespace

// ── Render ────────────────────────────────────────────────────────────────────

void PartyState::render() {
    auto& app = Application::get();
    int w = app.width();
    int h = app.height();

    glClearColor(0.04f, 0.03f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const PLayout l = computeLayout(w, h);
    m_hud.begin(w, h);

    char buf[256];
    static constexpr glm::vec4 COL_GOLD   = {0.9f,  0.85f, 0.4f,  1.0f};
    static constexpr glm::vec4 COL_WHITE  = {0.9f,  0.9f,  0.9f,  1.0f};
    static constexpr glm::vec4 COL_DIM    = {0.45f, 0.45f, 0.4f,  1.0f};
    static constexpr glm::vec4 COL_XP     = {0.5f,  0.8f,  1.0f,  1.0f};
    static constexpr glm::vec4 COL_LVUP   = {1.0f,  0.9f,  0.2f,  1.0f};
    static constexpr glm::vec4 COL_BORDER = {0.55f, 0.42f, 0.18f, 1.0f};

    // ── Title bar ─────────────────────────────────────────────────────────────
    m_hud.drawRect(0.f, 0.f, static_cast<float>(w), l.pad + 36.f * l.sc,
                   {0.06f, 0.04f, 0.10f, 1.0f});
    m_hud.drawRect(0.f, l.pad + 36.f * l.sc, static_cast<float>(w), 2.f * l.sc,
                   COL_BORDER);
    m_hud.drawText(l.pad, l.pad, l.sc * 2.0f, "PARTY", COL_GOLD);

    // ── EXIT button ───────────────────────────────────────────────────────────
    m_hud.drawRect(l.exitX, l.exitY, l.exitW, l.exitH,
                   {0.45f, 0.15f, 0.05f, 1.0f});
    m_hud.drawText(l.exitX + (l.exitW - 28.f * l.sc) / 2.f,
                   l.exitY + 5.f * l.sc, l.sc * 1.1f, "EXIT", COL_GOLD);

    // ── LEFT PANEL — army ─────────────────────────────────────────────────────
    m_hud.drawRect(l.leftX, l.leftY, l.leftW, l.leftH,
                   {0.07f, 0.05f, 0.03f, 0.88f});
    m_hud.drawRect(l.leftX, l.leftY, l.leftW, 3.f * l.sc, COL_BORDER);
    m_hud.drawText(l.leftX + 10.f * l.sc, l.leftY + 6.f * l.sc,
                   l.sc * 1.3f, "ARMY", COL_GOLD);

    // Column headers
    float hdrY = l.leftY + 22.f * l.sc;
    m_hud.drawText(l.leftX + 10.f * l.sc, hdrY, l.sc * 0.85f, "Unit", COL_DIM);
    m_hud.drawText(l.leftX + l.leftW * 0.5f, hdrY, l.sc * 0.85f, "Cnt", COL_DIM);
    m_hud.drawText(l.leftX + l.leftW * 0.63f, hdrY, l.sc * 0.85f, "Atk", COL_DIM);
    m_hud.drawText(l.leftX + l.leftW * 0.75f, hdrY, l.sc * 0.85f, "Def", COL_DIM);
    m_hud.drawText(l.leftX + l.leftW * 0.87f, hdrY, l.sc * 0.85f, "HP", COL_DIM);

    float rowY = hdrY + 14.f * l.sc;
    bool anyUnit = false;
    for (int i = 0; i < Hero::ARMY_SLOTS; ++i) {
        const ArmySlot& slot = m_hero.army[i];
        if (slot.isEmpty()) continue;
        anyUnit = true;

        glm::vec4 rowBg = (i % 2 == 0)
            ? glm::vec4{0.11f, 0.08f, 0.05f, 0.7f}
            : glm::vec4{0.08f, 0.06f, 0.04f, 0.5f};
        m_hud.drawRect(l.leftX + 6.f * l.sc, rowY,
                       l.leftW - 12.f * l.sc, l.unitRowH - 2.f * l.sc, rowBg);

        m_hud.drawText(l.leftX + 10.f * l.sc, rowY + 9.f * l.sc,
                       l.sc * 1.1f, slot.unitType->name.c_str(), COL_WHITE);

        std::snprintf(buf, sizeof(buf), "%d", slot.count);
        m_hud.drawText(l.leftX + l.leftW * 0.5f, rowY + 9.f * l.sc,
                       l.sc * 1.1f, buf, COL_WHITE);

        std::snprintf(buf, sizeof(buf), "%d", slot.unitType->attack);
        m_hud.drawText(l.leftX + l.leftW * 0.63f, rowY + 9.f * l.sc,
                       l.sc * 1.1f, buf, {0.7f, 0.9f, 0.6f, 1.0f});

        std::snprintf(buf, sizeof(buf), "%d", slot.unitType->defense);
        m_hud.drawText(l.leftX + l.leftW * 0.75f, rowY + 9.f * l.sc,
                       l.sc * 1.1f, buf, {0.6f, 0.8f, 1.0f, 1.0f});

        std::snprintf(buf, sizeof(buf), "%d", slot.unitType->hitPoints);
        m_hud.drawText(l.leftX + l.leftW * 0.87f, rowY + 9.f * l.sc,
                       l.sc * 1.1f, buf, {1.0f, 0.6f, 0.6f, 1.0f});

        rowY += l.unitRowH;
    }

    if (!anyUnit) {
        m_hud.drawText(l.leftX + 10.f * l.sc, rowY + 10.f * l.sc,
                       l.sc, "(no units)", COL_DIM);
    }

    // ── RIGHT PANEL — special characters ─────────────────────────────────────
    m_hud.drawRect(l.rightX, l.rightY, l.rightW, l.rightH,
                   {0.05f, 0.04f, 0.09f, 0.88f});
    m_hud.drawRect(l.rightX, l.rightY, l.rightW, 3.f * l.sc, COL_BORDER);
    m_hud.drawText(l.rightX + 10.f * l.sc, l.rightY + 6.f * l.sc,
                   l.sc * 1.3f, "SPECIAL CHARACTERS", COL_GOLD);

    for (int si = 0; si < Hero::SC_SLOTS; ++si) {
        float cy = scCardTopY(l, si);

        bool hasSC = si < static_cast<int>(m_hero.specials.size());
        const SpecialCharacter* sc = hasSC ? &m_hero.specials[si] : nullptr;

        // Card background
        glm::vec4 cardBg = sc
            ? glm::vec4{0.10f, 0.07f, 0.14f, 0.85f}
            : glm::vec4{0.07f, 0.06f, 0.08f, 0.45f};
        m_hud.drawRect(l.rightX + 6.f * l.sc, cy,
                       l.rightW - 12.f * l.sc, l.scCardH, cardBg);
        m_hud.drawRect(l.rightX + 6.f * l.sc, cy,
                       l.rightW - 12.f * l.sc, 2.f * l.sc,
                       {0.45f, 0.3f, 0.6f, 1.0f});

        if (!sc) {
            m_hud.drawText(l.rightX + 14.f * l.sc, cy + 8.f * l.sc,
                           l.sc * 1.0f, "-- empty --", COL_DIM);
            continue;
        }

        // Name, archetype, level
        std::snprintf(buf, sizeof(buf), "%s", sc->name.c_str());
        m_hud.drawText(l.rightX + 14.f * l.sc, cy + 6.f * l.sc,
                       l.sc * 1.4f, buf, COL_WHITE);

        std::snprintf(buf, sizeof(buf), "[%s]  Lv %d", sc->archetype.c_str(), sc->level);
        m_hud.drawText(l.rightX + 14.f * l.sc, cy + 22.f * l.sc,
                       l.sc * 0.95f, buf, COL_DIM);

        // XP bar
        float barX = l.rightX + 14.f * l.sc;
        float barY = cy + 36.f * l.sc;

        m_hud.drawRect(barX, barY, l.barW, l.barH, {0.15f, 0.15f, 0.15f, 1.0f});

        float fillFrac = 0.f;
        if (sc->def && sc->level < sc->def->maxLevel) {
            int prev  = (sc->level > 1) ? sc->def->xpThresholds[sc->level - 2] : 0;
            int next  = sc->def->xpThresholds[sc->level - 1];
            int range = next - prev;
            fillFrac  = (range > 0)
                ? std::min(1.f, static_cast<float>(sc->xp - prev) / range) : 1.f;
            std::snprintf(buf, sizeof(buf), "%d / %d XP", sc->xp, next);
        } else {
            fillFrac = 1.f;
            std::snprintf(buf, sizeof(buf), "MAX (%d XP)", sc->xp);
        }
        m_hud.drawRect(barX, barY, l.barW * fillFrac, l.barH,
                       {0.25f, 0.60f, 0.25f, 1.0f});
        m_hud.drawRect(barX, barY, l.barW, 1.f * l.sc, {0.35f, 0.35f, 0.35f, 0.8f});
        m_hud.drawText(barX + l.barW + 8.f * l.sc, barY, l.sc * 0.9f, buf, COL_XP);

        // Combat stats
        float statY = barY + l.barH + 8.f * l.sc;
        std::snprintf(buf, sizeof(buf), "ATK %d  DEF %d  HP %d  SPD %d",
                      sc->combatStats.attack, sc->combatStats.defense,
                      sc->combatStats.hitPoints, sc->combatStats.speed);
        m_hud.drawText(l.rightX + 14.f * l.sc, statY, l.sc * 0.85f, buf, COL_DIM);

        // Unlocked abilities
        if (!sc->unlockedAbilities.empty()) {
            std::string abilLine = "Abilities: ";
            for (int ai = 0; ai < static_cast<int>(sc->unlockedAbilities.size()); ++ai) {
                if (ai) abilLine += ", ";
                abilLine += sc->unlockedAbilities[ai];
            }
            float abilY = statY + 14.f * l.sc;
            m_hud.drawText(l.rightX + 14.f * l.sc, abilY,
                           l.sc * 0.85f, abilLine.c_str(), COL_LVUP);
        }
    }

    glDisable(GL_BLEND);
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool PartyState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    if (e->type == SDL_KEYDOWN && !e->key.repeat) {
        const auto sym = e->key.keysym.sym;
        if (sym == SDLK_ESCAPE || sym == SDLK_f) {
            Application::get().popState();
            return true;
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        auto& app = Application::get();
        const PLayout l = computeLayout(app.width(), app.height());
        float mx = static_cast<float>(e->button.x);
        float my = static_cast<float>(e->button.y);
        if (mx >= l.exitX && mx <= l.exitX + l.exitW &&
            my >= l.exitY && my <= l.exitY + l.exitH) {
            Application::get().popState();
            return true;
        }
    }

    return false;
}
