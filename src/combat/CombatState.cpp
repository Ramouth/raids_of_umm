#include "CombatState.h"
#include "CombatAI.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstdlib>

CombatState::CombatState(CombatArmy player, CombatArmy enemy,
                         std::shared_ptr<CombatOutcome>  outcome,
                         std::vector<std::string>        lootTable,
                         std::optional<SpecialCharacter> dungeonSC)
    : m_engine(std::move(player), std::move(enemy))
    , m_outcome(std::move(outcome))
    , m_lootTable(std::move(lootTable))
    , m_dungeonSC(std::move(dungeonSC))
{}

// ── Log helpers ───────────────────────────────────────────────────────────────

void CombatState::pushLog(std::string text, glm::vec4 color) {
    m_log.push_back({ std::move(text), color });
    while (static_cast<int>(m_log.size()) > MAX_LOG)
        m_log.pop_front();
}

void CombatState::renderLog(int screenW, int screenH) {
    if (m_log.empty()) return;

    const float scale    = screenH / 600.0f;
    const float lineH    = 16.f * scale;
    const float padX     = 8.f  * scale;
    const float padY     = 6.f  * scale;
    const float panelW   = 320.f * scale;
    const float panelH   = static_cast<float>(m_log.size()) * lineH + padY * 2.f;
    const float panelX   = screenW - panelW - 8.f * scale;
    const float panelY   = 8.f * scale;

    m_hud.begin(screenW, screenH);

    // Background + top border
    m_hud.drawRect(panelX, panelY, panelW, panelH, {0.0f, 0.0f, 0.0f, 0.72f});
    m_hud.drawRect(panelX, panelY, panelW, 2.f * scale, {0.5f, 0.45f, 0.25f, 1.0f});

    float ts = scale * 1.3f;
    for (int i = 0; i < static_cast<int>(m_log.size()); ++i) {
        float ty = panelY + padY + i * lineH;
        m_hud.drawText(panelX + padX, ty, ts,
                       m_log[i].text.c_str(), m_log[i].color);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void CombatState::onEnter() {
    m_renderer.init();
    m_hud.init();

    // Centre the camera on the combat grid.
    // Grid is 11 cols × 5 rows; centre hex ≈ col 5, row 2.
    // toHex(5, 2) → q=5, r=2-(5-1)/2=0  →  toWorld: x=7.5, z=√3*2.5≈4.33
    m_cam.setPosition({ 7.5f, 4.3f });
    m_cam.setZoom(6.5f);

    std::cout << "[CombatState] Battle started\n";
}

void CombatState::onExit() {
    std::cout << "[CombatState] Battle ended — ";
    switch (m_engine.result()) {
        case CombatResult::PlayerWon: std::cout << "player won\n";  break;
        case CombatResult::EnemyWon:  std::cout << "enemy won\n";   break;
        case CombatResult::Retreated: std::cout << "retreated\n";   break;
        default:                      std::cout << "ongoing\n";     break;
    }

    // Write result + surviving player stacks so the caller can update the hero.
    if (m_outcome) {
        m_outcome->result = m_engine.result();
        m_outcome->survivors.clear();
        for (const auto& unit : m_engine.playerArmy().stacks) {
            if (!unit.isDead())
                m_outcome->survivors.push_back({ unit.type, unit.count });
        }

        // On victory, award one random item from the loot table (if any).
        if (m_engine.result() == CombatResult::PlayerWon && !m_lootTable.empty()) {
            int idx = std::rand() % static_cast<int>(m_lootTable.size());
            m_outcome->itemsFound.push_back(m_lootTable[idx]);
        }

        // On victory, the dungeon's resident SC (if any) joins the hero.
        if (m_engine.result() == CombatResult::PlayerWon && m_dungeonSC.has_value())
            m_outcome->scFound.push_back(*m_dungeonSC);

        // Write SC progression earned during this battle.
        for (const auto& unit : m_engine.playerArmy().stacks) {
            if (!unit.isSpecialCharacter) continue;
            m_outcome->scUpdates.push_back({
                unit.scId, unit.scLevel, unit.scXp,
                unit.scUnlocked, unit.scChosenBranches
            });
        }
    }
}

void CombatState::tickAnimation(float dt) {
    // 1. Drain events. ScChoicePending is intercepted here instead of being
    //    queued for animation — player choices are modal, not animated.
    //    Enemy choices are auto-resolved immediately (first branch).
    for (auto& ev : m_engine.drainEvents()) {
        if (ev.type == CombatEvent::Type::ScChoicePending) {
            if (ev.isPlayer) {
                m_pendingChoiceEvent = ev;  // show overlay
                static constexpr glm::vec4 COL_CHOICE = {0.9f, 0.8f, 0.2f, 1.0f};
                // unitName is defined later in this TU; use army directly.
                const auto& stacks = m_engine.playerArmy().stacks;
                const char* name = (ev.stackIndex >= 0
                    && ev.stackIndex < static_cast<int>(stacks.size())
                    && stacks[ev.stackIndex].type)
                    ? stacks[ev.stackIndex].type->name.c_str() : "?";
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                              "*** %s: choose a path (level %d) ***", name, ev.choiceLevel);
                pushLog(buf, COL_CHOICE);
            } else if (!ev.branchOptions.empty()) {
                // AI: always pick the first branch.
                m_engine.resolveScChoice(false, ev.stackIndex, ev.branchOptions[0].id);
                for (auto& res : m_engine.drainEvents())
                    m_pendingEvents.push(std::move(res));
            }
            continue;  // never enters the animation queue
        }
        m_pendingEvents.push(std::move(ev));
    }

    // 2. Advance running animation.
    if (m_animating) {
        m_animTimer += dt;
        // For move: smoothly interpolate world position.
        if (m_currentEvent.type == CombatEvent::Type::UnitMoved) {
            float t = std::min(m_animTimer / m_animDuration, 1.0f);
            t = t * t * (3.f - 2.f * t);
            m_anim.worldPos = m_animFrom + t * (m_animTo - m_animFrom);
        }
        if (m_animTimer >= m_animDuration) {
            m_animating   = false;
            m_anim.active = false;
        }
        return;  // still animating — don't start a new event or let AI act
    }

    // 3. Start next pending event (one per tick for sequential playback).
    if (!m_pendingEvents.empty()) {
        startEventAnimation(m_pendingEvents.front());
        m_pendingEvents.pop();
        return;
    }

    // 4. Queue empty and game is over — nothing more to drive.
    if (m_engine.isOver()) return;

    // 5. AI turn: enemy always; friendly when auto-battle is active.
    //    Drain immediately into m_pendingEvents to close the race window
    //    (game loop order: processEvents → update → render).
    //    Block AI while waiting for a player branch-choice.
    if (m_pendingChoiceEvent.has_value()) return;

    bool aiShouldAct = !m_engine.currentTurn().isPlayer
                    || m_autoBattle;
    if (aiShouldAct) {
        CombatAI::takeTurn(m_engine);
        for (auto& ev : m_engine.drainEvents())
            m_pendingEvents.push(std::move(ev));
    }
}

// ── startEventAnimation ───────────────────────────────────────────────────────

static const char* unitName(const CombatEngine& eng, bool isPlayer, int idx) {
    const CombatArmy& army = isPlayer ? eng.playerArmy() : eng.enemyArmy();
    if (idx < 0 || idx >= static_cast<int>(army.stacks.size())) return "?";
    const auto& s = army.stacks[idx];
    return s.type ? s.type->name.c_str() : "?";
}

void CombatState::startEventAnimation(const CombatEvent& ev) {
    m_currentEvent = ev;
    m_animating    = true;
    m_animTimer    = 0.f;

    // ── Combat log entry ─────────────────────────────────────────────────────
    char buf[128];
    static constexpr glm::vec4 COL_PLAYER  = {0.6f, 1.0f, 0.65f, 1.0f};  // green
    static constexpr glm::vec4 COL_ENEMY   = {1.0f, 0.5f, 0.4f,  1.0f};  // red
    static constexpr glm::vec4 COL_DAMAGE  = {1.0f, 0.85f, 0.3f, 1.0f};  // yellow
    static constexpr glm::vec4 COL_DEATH   = {0.7f, 0.7f,  0.7f, 1.0f};  // grey
    static constexpr glm::vec4 COL_VICTORY = {1.0f, 0.95f, 0.4f, 1.0f};  // bright gold
    static constexpr glm::vec4 COL_DEFEAT  = {0.9f, 0.2f,  0.2f, 1.0f};  // bright red

    switch (ev.type) {
        case CombatEvent::Type::UnitMoved: {
            const char* side = ev.isPlayer ? "[P]" : "[E]";
            std::snprintf(buf, sizeof(buf), "%s %s moves",
                          side, unitName(m_engine, ev.isPlayer, ev.stackIndex));
            pushLog(buf, ev.isPlayer ? COL_PLAYER : COL_ENEMY);
            break;
        }
        case CombatEvent::Type::UnitAttacked: {
            const char* atk = unitName(m_engine, ev.isPlayer,       ev.stackIndex);
            const char* def = unitName(m_engine, ev.targetIsPlayer, ev.targetIndex);
            const char* verb = ev.isRetaliation ? "retaliates vs" : "attacks";
            if (ev.wasFlanked)
                std::snprintf(buf, sizeof(buf), "%s %s %s [PINNED]", atk, verb, def);
            else
                std::snprintf(buf, sizeof(buf), "%s %s %s", atk, verb, def);
            pushLog(buf, ev.isPlayer ? COL_PLAYER : COL_ENEMY);
            break;
        }
        case CombatEvent::Type::UnitDamaged: {
            const char* name = unitName(m_engine, ev.isPlayer, ev.stackIndex);
            const auto& army = ev.isPlayer ? m_engine.playerArmy() : m_engine.enemyArmy();
            int remaining = (ev.stackIndex >= 0 && ev.stackIndex < static_cast<int>(army.stacks.size()))
                            ? army.stacks[ev.stackIndex].count : 0;
            std::snprintf(buf, sizeof(buf), "  %s: -%d hp (%d left)",
                          name, ev.damage, remaining);
            pushLog(buf, COL_DAMAGE);
            break;
        }
        case CombatEvent::Type::UnitDied: {
            const char* name = unitName(m_engine, ev.isPlayer, ev.stackIndex);
            std::snprintf(buf, sizeof(buf), "  %s wiped out!", name);
            pushLog(buf, COL_DEATH);
            break;
        }
        case CombatEvent::Type::UnitDefended: {
            const char* side = ev.isPlayer ? "[P]" : "[E]";
            std::snprintf(buf, sizeof(buf), "%s %s defends",
                          side, unitName(m_engine, ev.isPlayer, ev.stackIndex));
            pushLog(buf, ev.isPlayer ? COL_PLAYER : COL_ENEMY);
            break;
        }
        case CombatEvent::Type::BattleEnded:
            switch (ev.result) {
                case CombatResult::PlayerWon:
                    pushLog("*** VICTORY! ***", COL_VICTORY); break;
                case CombatResult::EnemyWon:
                    pushLog("*** DEFEAT! ***", COL_DEFEAT); break;
                case CombatResult::Retreated:
                    pushLog("*** RETREATED ***", COL_DEATH); break;
                default: break;
            }
            pushLog("Press any key to continue", COL_DEATH);
            break;
        case CombatEvent::Type::ScXpGained: {
            static constexpr glm::vec4 COL_XP = {0.6f, 0.85f, 1.0f, 1.0f};  // light blue
            const char* name = unitName(m_engine, ev.isPlayer, ev.stackIndex);
            std::snprintf(buf, sizeof(buf), "  %s +%d XP", name, ev.xpAmount);
            pushLog(buf, COL_XP);
            break;
        }
        case CombatEvent::Type::ScLevelUp: {
            static constexpr glm::vec4 COL_LEVEL = {1.0f, 0.9f, 0.2f, 1.0f};  // gold
            const char* name = unitName(m_engine, ev.isPlayer, ev.stackIndex);
            std::snprintf(buf, sizeof(buf), "*** %s reached level %d! ***",
                          name, ev.newLevel);
            pushLog(buf, COL_LEVEL);
            // Track for the post-battle XP overlay (player SCs only).
            if (ev.isPlayer && ev.stackIndex >= 0) {
                const auto& stacks = m_engine.playerArmy().stacks;
                if (ev.stackIndex < static_cast<int>(stacks.size())
                        && stacks[ev.stackIndex].isSpecialCharacter)
                    m_scLevelUpsThisBattle[stacks[ev.stackIndex].scId]++;
            }
            break;
        }
        default: break;
    }

    switch (ev.type) {
        case CombatEvent::Type::UnitMoved: {
            m_animDuration    = MOVE_ANIM_DURATION;
            m_anim.active     = true;
            m_anim.isPlayer   = ev.isPlayer;
            m_anim.stackIndex = ev.stackIndex;
            float fx, fz, tx, tz;
            ev.from.toWorld(HEX_SIZE, fx, fz);
            ev.to.toWorld(HEX_SIZE, tx, tz);
            m_animFrom      = { fx, 0.f, fz };
            m_animTo        = { tx, 0.f, tz };
            m_anim.worldPos = m_animFrom;
            break;
        }
        case CombatEvent::Type::UnitAttacked:
            m_animDuration = ATTACK_ANIM_DURATION;
            m_anim.active  = false;
            break;
        case CombatEvent::Type::UnitDamaged:
            m_animDuration = DAMAGE_ANIM_DURATION;
            m_anim.active  = false;
            break;
        case CombatEvent::Type::UnitDied:
            m_animDuration = DEATH_ANIM_DURATION;
            m_anim.active  = false;
            break;
        default:
            m_animDuration = FLASH_ANIM_DURATION;
            m_anim.active  = false;
            break;
    }
}

void CombatState::update(float dt) {
    // Deferred dismiss — safe to pop here because StateMachine sets m_processing=true
    // during update(), so pop() enqueues rather than destroying the object mid-call.
    if (m_wantsDismiss) {
        Application::get().popState();
        return;
    }

    tickAnimation(dt);

    // Camera pan (normalised so diagonal isn't faster)
    float dx = 0.f, dz = 0.f;
    if (m_panLeft)  dx -= 1.f;
    if (m_panRight) dx += 1.f;
    if (m_panUp)    dz -= 1.f;
    if (m_panDown)  dz += 1.f;
    if (dx != 0.f || dz != 0.f) {
        float len = std::sqrt(dx*dx + dz*dz);
        m_cam.pan(dx / len * CAM_SPEED * dt,
                  dz / len * CAM_SPEED * dt);
    }
}

void CombatState::render() {
    glClearColor(0.05f, 0.04f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto& app = Application::get();
    m_cam.resize(app.width(), app.height());
    m_renderer.render(app.width(), app.height(),
                      m_engine, m_cam, m_hoveredHex, m_hasHovered, m_anim);

    renderLog(app.width(), app.height());
    if (m_engine.isOver() && m_pendingEvents.empty() && !m_animating)
        renderXpOverlay(app.width(), app.height());
    if (m_pendingChoiceEvent.has_value())
        renderBranchChoice(app.width(), app.height());
}

// ── XP overlay ────────────────────────────────────────────────────────────────

void CombatState::renderXpOverlay(int screenW, int screenH) {
    // Collect player SC stacks that have a def (progression capable).
    std::vector<const CombatUnit*> scUnits;
    for (const auto& u : m_engine.playerArmy().stacks)
        if (u.isSpecialCharacter && u.scDef)
            scUnits.push_back(&u);
    if (scUnits.empty()) return;

    const float scale = screenH / 600.0f;
    const float lineH = 18.f * scale;
    const float padX  = 10.f * scale;
    const float padY  = 8.f  * scale;
    const float barW  = 110.f * scale;
    const float barH  = 8.f  * scale;
    const float rowH  = lineH * 2.6f;
    const float panelW = 300.f * scale;
    const float panelH = padY * 2.f + lineH + static_cast<float>(scUnits.size()) * rowH;
    const float panelX = 8.f * scale;
    const float panelY = screenH - panelH - 8.f * scale;

    m_hud.begin(screenW, screenH);

    // Background + top accent border.
    m_hud.drawRect(panelX, panelY, panelW, panelH, {0.0f, 0.0f, 0.0f, 0.82f});
    m_hud.drawRect(panelX, panelY, panelW, 2.f * scale, {0.7f, 0.6f, 0.2f, 1.0f});

    static constexpr glm::vec4 COL_HEADER = {0.9f, 0.85f, 0.4f, 1.0f};
    static constexpr glm::vec4 COL_NAME   = {0.9f, 0.9f,  0.9f, 1.0f};
    static constexpr glm::vec4 COL_XP     = {0.5f, 0.8f,  1.0f, 1.0f};
    static constexpr glm::vec4 COL_LVUP   = {1.0f, 0.9f,  0.2f, 1.0f};

    const float ts = scale * 1.3f;
    m_hud.drawText(panelX + padX, panelY + padY, ts, "SC PROGRESS", COL_HEADER);

    float rowY = panelY + padY + lineH;

    for (const CombatUnit* u : scUnits) {
        // Name + level.
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s  Lv %d", u->type->name.c_str(), u->scLevel);
        m_hud.drawText(panelX + padX, rowY, ts, buf, COL_NAME);

        // XP progress bar.
        float barX = panelX + padX;
        float barY = rowY + lineH * 0.75f;
        m_hud.drawRect(barX, barY, barW, barH, {0.2f, 0.2f, 0.2f, 1.0f});  // track

        float fillFrac = 0.f;
        if (u->scLevel < u->scDef->maxLevel) {
            int prev  = (u->scLevel > 1) ? u->scDef->xpThresholds[u->scLevel - 2] : 0;
            int next  = u->scDef->xpThresholds[u->scLevel - 1];
            int range = next - prev;
            fillFrac  = (range > 0) ? std::min(1.f, static_cast<float>(u->scXp - prev) / range) : 1.f;
            std::snprintf(buf, sizeof(buf), "%d / %d XP", u->scXp, next);
        } else {
            fillFrac = 1.f;
            std::snprintf(buf, sizeof(buf), "MAX  (%d XP)", u->scXp);
        }
        m_hud.drawRect(barX, barY, barW * fillFrac, barH, {0.25f, 0.65f, 0.25f, 1.0f});  // fill
        m_hud.drawRect(barX, barY, barW, 1.f * scale, {0.4f, 0.4f, 0.4f, 0.8f});         // top line
        m_hud.drawText(barX + barW + 6.f * scale, barY - 1.f, ts * 0.9f, buf, COL_XP);

        // Level-up badge.
        auto it = m_scLevelUpsThisBattle.find(u->scId);
        if (it != m_scLevelUpsThisBattle.end() && it->second > 0) {
            std::snprintf(buf, sizeof(buf), "* LEVEL UP x%d!", it->second);
            m_hud.drawText(barX, barY + barH + 2.f * scale, ts, buf, COL_LVUP);
        }

        rowY += rowH;
    }
}

// ── Branch choice overlay ─────────────────────────────────────────────────────
// Thin placeholder — all layout state lives in m_pendingChoiceEvent / m_hoveredBranch.
// Designed to be replaced wholesale during the UI/graphics overhaul.

void CombatState::renderBranchChoice(int screenW, int screenH) {
    if (!m_pendingChoiceEvent.has_value()) return;
    const auto& ev   = *m_pendingChoiceEvent;
    const auto& opts = ev.branchOptions;
    if (opts.empty()) return;

    const float scale  = screenH / 600.0f;
    const float cardW  = 160.f * scale;
    const float cardH  = 120.f * scale;
    const float gap    = 16.f  * scale;
    const int   n      = static_cast<int>(opts.size());
    const float totalW = n * cardW + (n - 1) * gap;
    const float startX = (screenW - totalW) / 2.f;
    const float cardY  = (screenH - cardH) / 2.f + 30.f * scale;
    const float padX   = 8.f  * scale;
    const float padY   = 6.f  * scale;
    const float lineH  = 15.f * scale;
    const float ts     = scale * 1.2f;
    const float tsSmall = scale * 1.0f;

    static constexpr glm::vec4 COL_VEIL   = {0.0f,  0.0f,  0.0f,  0.60f};
    static constexpr glm::vec4 COL_PANEL  = {0.08f, 0.07f, 0.05f, 0.96f};
    static constexpr glm::vec4 COL_ACCENT = {0.75f, 0.62f, 0.18f, 1.0f};
    static constexpr glm::vec4 COL_HOVER  = {0.22f, 0.20f, 0.10f, 1.0f};
    static constexpr glm::vec4 COL_TITLE  = {1.0f,  0.92f, 0.35f, 1.0f};
    static constexpr glm::vec4 COL_NAME   = {0.95f, 0.90f, 0.75f, 1.0f};
    static constexpr glm::vec4 COL_DESC   = {0.70f, 0.70f, 0.65f, 1.0f};
    static constexpr glm::vec4 COL_HINT   = {0.55f, 0.55f, 0.50f, 1.0f};

    m_hud.begin(screenW, screenH);

    // Full-screen dark veil.
    m_hud.drawRect(0.f, 0.f, static_cast<float>(screenW), static_cast<float>(screenH), COL_VEIL);

    // Title banner above cards.
    const char* scName = unitName(m_engine, ev.isPlayer, ev.stackIndex);
    char titleBuf[128];
    std::snprintf(titleBuf, sizeof(titleBuf),
                  "LEVEL UP  —  %s  (level %d)", scName, ev.choiceLevel);
    float titleW = static_cast<float>(strlen(titleBuf)) * 7.f * ts;  // rough estimate
    m_hud.drawText((screenW - titleW) / 2.f,
                   cardY - lineH * 2.4f, ts, titleBuf, COL_TITLE);

    // Branch cards.
    for (int i = 0; i < n; ++i) {
        float cx = startX + i * (cardW + gap);

        // Card background (highlighted if hovered).
        glm::vec4 bg = (m_hoveredBranch == i) ? COL_HOVER : COL_PANEL;
        m_hud.drawRect(cx, cardY, cardW, cardH, bg);

        // Top border accent.
        glm::vec4 border = (m_hoveredBranch == i) ? COL_TITLE : COL_ACCENT;
        m_hud.drawRect(cx, cardY, cardW, 2.f * scale, border);

        // Key hint badge (1 or 2).
        char badge[4];
        std::snprintf(badge, sizeof(badge), "[%d]", i + 1);
        m_hud.drawText(cx + padX, cardY + padY, tsSmall, badge, COL_ACCENT);

        // Option name.
        m_hud.drawText(cx + padX, cardY + padY + lineH * 1.1f,
                       ts, opts[i].name.c_str(), COL_NAME);

        // Description (naive word-wrap: split at ~22 chars per line).
        const std::string& desc = opts[i].description;
        float dy = cardY + padY + lineH * 2.4f;
        size_t pos = 0;
        while (pos < desc.size()) {
            size_t end = std::min(pos + 22u, desc.size());
            // Break at last space before limit.
            if (end < desc.size()) {
                size_t sp = desc.rfind(' ', end);
                if (sp != std::string::npos && sp > pos) end = sp;
            }
            std::string line = desc.substr(pos, end - pos);
            m_hud.drawText(cx + padX, dy, tsSmall, line.c_str(), COL_DESC);
            dy  += lineH * 1.05f;
            pos  = (end < desc.size() && desc[end] == ' ') ? end + 1 : end;
        }
    }

    // "Click or press 1/2" hint below cards.
    const char* hint = "Click a card  or  press 1 / 2  to choose";
    float hintW = static_cast<float>(strlen(hint)) * 6.f * tsSmall;
    m_hud.drawText((screenW - hintW) / 2.f,
                   cardY + cardH + lineH * 0.8f, tsSmall, hint, COL_HINT);
}

bool CombatState::handleEvent(void* sdlEvent) {
    const SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    // ── Battle-over dismissal ─────────────────────────────────────────────────
    // Wait until all queued animations finish before accepting a dismiss input.
    if (m_engine.isOver() && m_pendingEvents.empty() && !m_animating) {
        if (e->type == SDL_KEYDOWN ||
            (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)) {
            m_wantsDismiss = true;
            return true;
        }
        return false;
    }

    // ── Branch choice (modal — consumes all input until resolved) ─────────────
    if (m_pendingChoiceEvent.has_value()) {
        const auto& opts = m_pendingChoiceEvent->branchOptions;
        const float sw = static_cast<float>(Application::get().width());
        const float sh = static_cast<float>(Application::get().height());

        // Card geometry — must match renderBranchChoice exactly.
        auto cardGeom = [&](int i, float& cx, float& cy, float& cw, float& ch) {
            const float scale  = sh / 600.0f;
            cw = 160.f * scale;
            ch = 120.f * scale;
            const float gap    = 16.f  * scale;
            const int   n      = static_cast<int>(opts.size());
            const float totalW = n * cw + (n - 1) * gap;
            cx = (sw - totalW) / 2.f + i * (cw + gap);
            cy = (sh - ch) / 2.f + 30.f * scale;
        };

        // Mouse click: hit-test card buttons.
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            float mx = static_cast<float>(e->button.x);
            float my = static_cast<float>(e->button.y);
            for (int i = 0; i < static_cast<int>(opts.size()); ++i) {
                float cx, cy, cw, ch;
                cardGeom(i, cx, cy, cw, ch);
                if (mx >= cx && mx < cx + cw && my >= cy && my < cy + ch) {
                    const std::string chosen = opts[i].id;
                    const bool isPlayer = m_pendingChoiceEvent->isPlayer;
                    const int  idx      = m_pendingChoiceEvent->stackIndex;
                    m_pendingChoiceEvent.reset();
                    m_hoveredBranch = -1;
                    m_engine.resolveScChoice(isPlayer, idx, chosen);
                    for (auto& res : m_engine.drainEvents())
                        m_pendingEvents.push(std::move(res));
                    return true;
                }
            }
            return true;  // consume click even if outside cards
        }
        // Keyboard shortcut: 1/2 keys choose left/right branch.
        if (e->type == SDL_KEYDOWN) {
            int pick = -1;
            if (e->key.keysym.sym == SDLK_1 && opts.size() >= 1) pick = 0;
            if (e->key.keysym.sym == SDLK_2 && opts.size() >= 2) pick = 1;
            if (pick >= 0) {
                const std::string chosen = opts[pick].id;
                const bool isPlayer = m_pendingChoiceEvent->isPlayer;
                const int  idx      = m_pendingChoiceEvent->stackIndex;
                m_pendingChoiceEvent.reset();
                m_hoveredBranch = -1;
                m_engine.resolveScChoice(isPlayer, idx, chosen);
                for (auto& res : m_engine.drainEvents())
                    m_pendingEvents.push(std::move(res));
                return true;
            }
        }
        // Mouse move: update hover highlight.
        if (e->type == SDL_MOUSEMOTION) {
            float mx = static_cast<float>(e->motion.x);
            float my = static_cast<float>(e->motion.y);
            m_hoveredBranch = -1;
            for (int i = 0; i < static_cast<int>(opts.size()); ++i) {
                float cx, cy, cw, ch;
                cardGeom(i, cx, cy, cw, ch);
                if (mx >= cx && mx < cx + cw && my >= cy && my < cy + ch)
                    m_hoveredBranch = i;
            }
        }
        return true;  // swallow all other input while modal
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            // Camera pan
            case SDLK_UP:    case SDLK_w: m_panUp    = true; return true;
            case SDLK_DOWN:  case SDLK_s: m_panDown  = true; return true;
            case SDLK_LEFT:  case SDLK_a: m_panLeft  = true; return true;
            case SDLK_RIGHT: case SDLK_d: m_panRight = true; return true;

            // Toggle AI control of friendly units (auto-battle).
            case SDLK_TAB:
                m_autoBattle = !m_autoBattle;
                std::cout << "[Combat] Auto-battle: "
                          << (m_autoBattle ? "ON" : "OFF") << "\n";
                return true;

            // Combat actions — only valid on the player's turn, when idle.
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (!m_animating && m_pendingEvents.empty()
                        && m_engine.currentTurn().isPlayer)
                    m_engine.doAttack(0);
                return true;
            case SDLK_SPACE:
                if (!m_animating && m_pendingEvents.empty()
                        && m_engine.currentTurn().isPlayer)
                    m_engine.doDefend();
                return true;
            // ESC always retreats and queues immediate dismiss.
            case SDLK_ESCAPE:
                m_engine.doRetreat();
                m_wantsDismiss = true;
                return true;
            default: break;
        }
    }

    // ── Key up ────────────────────────────────────────────────────────────────
    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_UP:    case SDLK_w: m_panUp    = false; return true;
            case SDLK_DOWN:  case SDLK_s: m_panDown  = false; return true;
            case SDLK_LEFT:  case SDLK_a: m_panLeft  = false; return true;
            case SDLK_RIGHT: case SDLK_d: m_panRight = false; return true;
            default: break;
        }
    }

    // ── Mouse move → hex hover ────────────────────────────────────────────────
    if (e->type == SDL_MOUSEMOTION) {
        glm::vec2 world = m_cam.screenToWorld(
            static_cast<float>(e->motion.x),
            static_cast<float>(e->motion.y));
        HexCoord hc = HexCoord::fromWorld(world.x, world.y, HEX_SIZE);
        if (CombatMap::inBounds(hc)) {
            m_hoveredHex = hc;
            m_hasHovered = true;
        } else {
            m_hasHovered = false;
        }
        return false;  // don't consume — let AdventureState below still see it
    }

    // ── Left click → move or attack (player's turn only, when idle) ──────────
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (m_animating || !m_pendingEvents.empty()
                || !m_engine.currentTurn().isPlayer) return true;

        glm::vec2 world = m_cam.screenToWorld(
            static_cast<float>(e->button.x),
            static_cast<float>(e->button.y));
        HexCoord clicked = HexCoord::fromWorld(world.x, world.y, HEX_SIZE);

        if (!CombatMap::inBounds(clicked)) return true;

        // Try attack first (click on an attackable enemy hex).
        // Engine produces events; tickAnimation() will consume them next update().
        if (m_engine.doAttackAt(clicked)) return true;

        // Try move (click on a reachable empty hex).
        // Engine produces UnitMoved event; tickAnimation() animates it.
        if (m_engine.canMoveTo(clicked))
            m_engine.doMove(clicked);

        return true;
    }

    return false;
}
