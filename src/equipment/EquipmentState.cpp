#include "EquipmentState.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <cstdio>
#include <algorithm>

// ── Construction ──────────────────────────────────────────────────────────────

EquipmentState::EquipmentState(Hero& hero) : m_hero(hero) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void EquipmentState::onEnter() {
    m_hud.init();
    m_selectedItem = nullptr;
    std::cout << "[Equipment] Opened. Bag: " << m_hero.items.size()
              << " item(s), Party: " << m_hero.specials.size() << " SC(s).\n";
}

void EquipmentState::onExit() {
    std::cout << "[Equipment] Closed.\n";
}

void EquipmentState::update(float /*dt*/) {}

// ── Slot helpers ──────────────────────────────────────────────────────────────

int EquipmentState::slotIndex(const std::string& s) {
    if (s == "weapon")  return 0;
    if (s == "armor")   return 1;
    if (s == "amulet")  return 2;
    if (s == "trinket") return 3;
    return -1;
}

const char* EquipmentState::slotLabel(int idx) {
    switch (idx) {
        case 0: return "WPN";
        case 1: return "ARM";
        case 2: return "AMU";
        case 3: return "TRK";
        default: return "???";
    }
}

// ── Equipment logic ───────────────────────────────────────────────────────────

void EquipmentState::equipItem(int scIdx) {
    if (!m_selectedItem) return;
    if (scIdx < 0 || scIdx >= static_cast<int>(m_hero.specials.size())) return;

    SpecialCharacter& sc = m_hero.specials[scIdx];
    if (sc.isEmpty()) return;

    int idx = slotIndex(m_selectedItem->slot);
    if (idx < 0 || idx >= 4) return;
    if (sc.equipped[idx] != nullptr) return;  // slot occupied

    sc.equipped[idx] = m_selectedItem;

    auto& inv = m_hero.items;
    inv.erase(std::remove(inv.begin(), inv.end(), m_selectedItem), inv.end());

    std::cout << "[Equipment] Equipped '" << m_selectedItem->name
              << "' on " << sc.name << " [slot " << idx << "]\n";
    m_selectedItem = nullptr;
}

void EquipmentState::unequipSlot(int scIdx, int slotIdx) {
    if (scIdx < 0 || scIdx >= static_cast<int>(m_hero.specials.size())) return;
    if (slotIdx < 0 || slotIdx >= 4) return;

    SpecialCharacter& sc = m_hero.specials[scIdx];
    const WondrousItem* item = sc.equipped[slotIdx];
    if (!item) return;

    if (item->cursed) {
        std::cout << "[Equipment] Cannot unequip cursed item '" << item->name << "'!\n";
        return;
    }

    m_hero.items.push_back(item);
    sc.equipped[slotIdx] = nullptr;
    std::cout << "[Equipment] Unequipped '" << item->name
              << "' from " << sc.name << "\n";
}

// ── Layout ────────────────────────────────────────────────────────────────────
// All coordinates are y-DOWN pixel coords (y=0 = top of screen).

namespace {

struct EqLayout {
    float sc;
    // Left panel — SC party
    float leftX, leftY, leftW, leftH;
    // Right panel — item bag
    float rightX, rightY, rightW, rightH;
    // Per-SC card dimensions
    float cardH;
    // Per-slot box within a card
    float slotW, slotH;
    // Item row height in the bag list
    float itemRowH;
    // EXIT button
    float exitX, exitY, exitW, exitH;
};

EqLayout computeLayout(int w, int h) {
    EqLayout l;
    l.sc = h / 600.0f;

    l.leftX = 20  * l.sc;
    l.leftY = 20  * l.sc;
    l.leftW = 360 * l.sc;
    l.leftH = h - 40 * l.sc;

    l.rightX = l.leftX + l.leftW + 15 * l.sc;
    l.rightY = l.leftY;
    l.rightW = w - l.rightX - 20 * l.sc;
    l.rightH = l.leftH;

    // 3 SC cards stacked, 20*sc title bar at top, 8*sc gaps between cards.
    float titleH = 20 * l.sc;
    float gap    =  8 * l.sc;
    l.cardH = (l.leftH - titleH - 2 * gap) / 3.0f;

    // 4 slot boxes in the bottom portion of each card.
    float slotPad = 8 * l.sc;
    l.slotW = (l.leftW - 2 * slotPad - 3 * 6 * l.sc) / 4.0f;
    l.slotH = 52 * l.sc;

    l.itemRowH = 30 * l.sc;

    l.exitW = 80 * l.sc;
    l.exitH = 28 * l.sc;
    l.exitX = l.rightX + l.rightW - l.exitW - 6 * l.sc;
    l.exitY = l.rightY + l.rightH - l.exitH - 6 * l.sc;

    return l;
}

// Top-left Y of SC card at index [i].
float cardTopY(const EqLayout& l, int i) {
    float titleH = 20 * l.sc;
    float gap    =  8 * l.sc;
    return l.leftY + titleH + i * (l.cardH + gap);
}

// Top-left X of slot [sj] within a card.
float slotX(const EqLayout& l, int sj) {
    float slotPad = 8 * l.sc;
    return l.leftX + slotPad + sj * (l.slotW + 6 * l.sc);
}

// Top-left Y of slot row within card starting at cy.
float slotY(const EqLayout& l, float cy) {
    return cy + l.cardH - l.slotH - 8 * l.sc;
}

} // namespace

// ── Render ────────────────────────────────────────────────────────────────────

void EquipmentState::render() {
    auto& app = Application::get();
    int w = app.width();
    int h = app.height();

    glClearColor(0.04f, 0.04f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const EqLayout l = computeLayout(w, h);
    m_hud.begin(w, h);

    char buf[256];

    // ── Left panel ────────────────────────────────────────────────────────────
    m_hud.drawRect(l.leftX, l.leftY, l.leftW, l.leftH,
                   {0.08f, 0.05f, 0.04f, 0.92f});
    m_hud.drawRect(l.leftX, l.leftY, l.leftW, 4 * l.sc,
                   {0.65f, 0.45f, 0.2f, 1.0f});
    m_hud.drawText(l.leftX + 10 * l.sc, l.leftY + 5 * l.sc, l.sc * 1.4f,
                   "PARTY", {1.0f, 0.9f, 0.5f, 1.0f});

    // ── SC cards ──────────────────────────────────────────────────────────────
    for (int si = 0; si < Hero::SC_SLOTS; ++si) {
        float cy   = cardTopY(l, si);
        float sy   = slotY(l, cy);
        bool  hasSC = si < static_cast<int>(m_hero.specials.size());
        const SpecialCharacter* sc = hasSC ? &m_hero.specials[si] : nullptr;

        // Card background
        glm::vec4 cardBg = sc
            ? glm::vec4{0.12f, 0.08f, 0.05f, 0.85f}
            : glm::vec4{0.07f, 0.06f, 0.05f, 0.55f};
        m_hud.drawRect(l.leftX + 6 * l.sc, cy,
                       l.leftW - 12 * l.sc, l.cardH, cardBg);
        m_hud.drawRect(l.leftX + 6 * l.sc, cy,
                       l.leftW - 12 * l.sc, 2 * l.sc,
                       {0.5f, 0.35f, 0.15f, 1.0f});

        // SC name / archetype
        if (sc) {
            m_hud.drawText(l.leftX + 12 * l.sc, cy + 5 * l.sc, l.sc * 1.2f,
                           sc->name.c_str(), {1.0f, 0.9f, 0.75f, 1.0f});
            std::snprintf(buf, sizeof(buf), "[%s]", sc->archetype.c_str());
            m_hud.drawText(l.leftX + 12 * l.sc, cy + 19 * l.sc, l.sc * 0.9f,
                           buf, {0.6f, 0.6f, 0.55f, 1.0f});
        } else {
            m_hud.drawText(l.leftX + 12 * l.sc, cy + 5 * l.sc, l.sc * 1.0f,
                           "-- empty slot --", {0.35f, 0.35f, 0.3f, 1.0f});
        }

        // 4 equipment slot boxes
        for (int sj = 0; sj < 4; ++sj) {
            float sx = slotX(l, sj);
            const WondrousItem* equipped = sc ? sc->equipped[sj] : nullptr;

            // Highlight green when a compatible item is selected and slot is free.
            bool canReceive = m_selectedItem && !equipped && sc &&
                              (slotIndex(m_selectedItem->slot) == sj);

            glm::vec4 borderCol =
                (equipped && equipped->cursed)  ? glm::vec4{0.75f, 0.1f,  0.1f,  1.0f}
              : canReceive                       ? glm::vec4{0.25f, 0.65f, 0.25f, 1.0f}
                                                 : glm::vec4{0.45f, 0.35f, 0.2f,  1.0f};

            m_hud.drawRect(sx, sy, l.slotW, l.slotH, borderCol);
            m_hud.drawRect(sx + 2 * l.sc, sy + 2 * l.sc,
                           l.slotW - 4 * l.sc, l.slotH - 4 * l.sc,
                           {0.06f, 0.05f, 0.04f, 0.92f});

            // Slot type label (top of box)
            m_hud.drawText(sx + 4 * l.sc, sy + 4 * l.sc, l.sc * 0.85f,
                           slotLabel(sj), {0.5f, 0.45f, 0.4f, 1.0f});

            if (equipped) {
                m_hud.drawText(sx + 4 * l.sc, sy + 18 * l.sc, l.sc * 0.85f,
                               equipped->name.c_str(), {0.95f, 0.85f, 0.6f, 1.0f});
                if (equipped->cursed)
                    m_hud.drawText(sx + 4 * l.sc, sy + 32 * l.sc, l.sc * 0.8f,
                                   "CURSED", {0.85f, 0.2f, 0.2f, 1.0f});
            }
        }
    }

    // ── Right panel (item bag) ────────────────────────────────────────────────
    m_hud.drawRect(l.rightX, l.rightY, l.rightW, l.rightH,
                   {0.06f, 0.04f, 0.08f, 0.92f});
    m_hud.drawRect(l.rightX, l.rightY, l.rightW, 4 * l.sc,
                   {0.5f, 0.3f, 0.6f, 1.0f});
    m_hud.drawText(l.rightX + 10 * l.sc, l.rightY + 5 * l.sc, l.sc * 1.4f,
                   "ITEM BAG", {0.9f, 0.75f, 1.0f, 1.0f});

    float listY = l.rightY + 22 * l.sc;

    for (int ii = 0; ii < static_cast<int>(m_hero.items.size()); ++ii) {
        const WondrousItem* item = m_hero.items[ii];
        float ry = listY + ii * l.itemRowH;
        bool  selected = (item == m_selectedItem);

        glm::vec4 rowBg = selected
            ? glm::vec4{0.25f, 0.18f, 0.32f, 0.9f}
            : (ii % 2 == 0
                ? glm::vec4{0.10f, 0.07f, 0.14f, 0.7f}
                : glm::vec4{0.08f, 0.06f, 0.11f, 0.5f});
        m_hud.drawRect(l.rightX + 6 * l.sc, ry,
                       l.rightW - 12 * l.sc, l.itemRowH - 2 * l.sc, rowBg);

        glm::vec4 nameCol = selected
            ? glm::vec4{1.0f,  0.9f,  1.0f,  1.0f}
            : (item->cursed
                ? glm::vec4{0.85f, 0.35f, 0.35f, 1.0f}
                : glm::vec4{0.9f,  0.8f,  0.55f, 1.0f});
        m_hud.drawText(l.rightX + 12 * l.sc, ry + 8 * l.sc, l.sc * 1.1f,
                       item->name.c_str(), nameCol);

        std::snprintf(buf, sizeof(buf), "[%s]%s",
                      item->slot.c_str(), item->cursed ? " CURSED" : "");
        m_hud.drawText(l.rightX + l.rightW - 115 * l.sc, ry + 8 * l.sc,
                       l.sc * 0.9f, buf,
                       item->cursed
                           ? glm::vec4{0.85f, 0.2f, 0.2f, 1.0f}
                           : glm::vec4{0.55f, 0.55f, 0.5f, 1.0f});
    }

    if (m_hero.items.empty()) {
        m_hud.drawText(l.rightX + 12 * l.sc, listY + 10 * l.sc, l.sc * 1.0f,
                       "(no items in bag)", {0.35f, 0.35f, 0.3f, 1.0f});
    }

    // Hint line above EXIT
    if (m_selectedItem) {
        std::snprintf(buf, sizeof(buf), "Selected: %s  — click a [%s] slot to equip",
                      m_selectedItem->name.c_str(), m_selectedItem->slot.c_str());
        m_hud.drawText(l.rightX + 10 * l.sc, l.exitY - 22 * l.sc,
                       l.sc * 0.95f, buf, {0.6f, 0.9f, 0.6f, 1.0f});
    } else {
        m_hud.drawText(l.rightX + 10 * l.sc, l.exitY - 22 * l.sc,
                       l.sc * 0.9f,
                       "Click item to select, then click a slot to equip",
                       {0.4f, 0.4f, 0.38f, 1.0f});
    }

    // EXIT button
    m_hud.drawRect(l.exitX, l.exitY, l.exitW, l.exitH,
                   {0.55f, 0.2f, 0.08f, 1.0f});
    m_hud.drawText(l.exitX + (l.exitW - 30 * l.sc) / 2.0f,
                   l.exitY + 8 * l.sc, l.sc * 1.1f,
                   "EXIT", {1.0f, 0.95f, 0.75f, 1.0f});

    glDisable(GL_BLEND);
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool EquipmentState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    if (e->type == SDL_KEYDOWN && !e->key.repeat) {
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            Application::get().popState();
            return true;
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        auto& app = Application::get();
        const EqLayout l = computeLayout(app.width(), app.height());
        float mx = static_cast<float>(e->button.x);
        float my = static_cast<float>(e->button.y);

        // EXIT button
        if (mx >= l.exitX && mx <= l.exitX + l.exitW &&
            my >= l.exitY && my <= l.exitY + l.exitH) {
            Application::get().popState();
            return true;
        }

        // SC equipment slots (left panel)
        for (int si = 0; si < Hero::SC_SLOTS; ++si) {
            if (si >= static_cast<int>(m_hero.specials.size())) continue;

            float cy = cardTopY(l, si);
            float sy = slotY(l, cy);

            for (int sj = 0; sj < 4; ++sj) {
                float sx = slotX(l, sj);
                if (mx >= sx && mx <= sx + l.slotW &&
                    my >= sy && my <= sy + l.slotH) {
                    const WondrousItem* equipped = m_hero.specials[si].equipped[sj];
                    if (m_selectedItem) {
                        if (slotIndex(m_selectedItem->slot) == sj)
                            equipItem(si);
                    } else if (equipped) {
                        unequipSlot(si, sj);
                    }
                    return true;
                }
            }
        }

        // Item rows (right panel)
        float listY = l.rightY + 22 * l.sc;
        for (int ii = 0; ii < static_cast<int>(m_hero.items.size()); ++ii) {
            float ry = listY + ii * l.itemRowH;
            if (mx >= l.rightX + 6 * l.sc && mx <= l.rightX + l.rightW - 6 * l.sc &&
                my >= ry && my <= ry + l.itemRowH - 2 * l.sc) {
                const WondrousItem* clicked = m_hero.items[ii];
                m_selectedItem = (m_selectedItem == clicked) ? nullptr : clicked;
                return true;
            }
        }

        // Click elsewhere → deselect
        m_selectedItem = nullptr;
    }

    return false;
}
