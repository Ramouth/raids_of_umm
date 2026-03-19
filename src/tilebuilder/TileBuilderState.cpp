#include "TileBuilderState.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void TileBuilderState::onEnter() {
    m_hud.init();
    openBrowser();
    std::cout << "[TileBuilder] Open a PNG sheet from assets/.\n";
    std::cout << "  Arrows=pan  Scroll=zoom  R=fit  +/-=cellW  [/]=cellH"
                 "  Click/drag=select  Enter=export  O=open  ESC=back\n";
}

void TileBuilderState::onExit() {
    if (m_sheet.tex) {
        glDeleteTextures(1, &m_sheet.tex);
        m_sheet = {};
    }
    SDL_StopTextInput();
}

// ── Sheet loading ─────────────────────────────────────────────────────────────

void TileBuilderState::loadSheet(const std::string& path) {
    if (m_sheet.tex) glDeleteTextures(1, &m_sheet.tex);
    m_sheet = {};
    m_selected.clear();
    if (!loadPNG(path, m_sheet)) {
        std::cerr << "[TileBuilder] Failed to load: " << path << "\n";
        return;
    }
    m_sheetPath = path;
    fitToScreen();
    std::cout << "[TileBuilder] Sheet: " << path
              << "  (" << m_sheet.w << "x" << m_sheet.h << ")\n";
}

void TileBuilderState::fitToScreen() {
    auto& app   = Application::get();
    float areaW = app.width()  - PREVIEW_W;
    float areaH = app.height() - STATUS_H;
    if (m_sheet.w <= 0 || m_sheet.h <= 0) return;
    float zx = areaW / m_sheet.w;
    float zy = areaH / m_sheet.h;
    m_zoom    = std::min(zx, zy) * 0.92f;
    m_offsetX = (areaW - m_sheet.w * m_zoom) * 0.5f;
    m_offsetY = (areaH - m_sheet.h * m_zoom) * 0.5f;
}

void TileBuilderState::screenToCell(int mx, int my, int& col, int& row) const {
    col = row = -1;
    if (m_sheet.w <= 0 || m_cellW <= 0 || m_cellH <= 0) return;
    float cellPxW = m_cellW * m_zoom;
    float cellPxH = m_cellH * m_zoom;
    float fx = (mx - m_offsetX) / cellPxW;
    float fy = (my - m_offsetY) / cellPxH;
    if (fx < 0 || fy < 0) return;
    int c = static_cast<int>(fx);
    int r = static_cast<int>(fy);
    int maxC = m_sheet.w / m_cellW;
    int maxR = m_sheet.h / m_cellH;
    if (c >= maxC || r >= maxR) return;
    col = c;
    row = r;
}

bool TileBuilderState::inSheetArea(int mx) const {
    return mx < static_cast<int>(Application::get().width() - PREVIEW_W);
}

// ── Update ────────────────────────────────────────────────────────────────────

void TileBuilderState::update(float dt) {
    float spd = PAN_SPEED * dt;
    if (m_keyLeft)  m_offsetX += spd;
    if (m_keyRight) m_offsetX -= spd;
    if (m_keyUp)    m_offsetY += spd;
    if (m_keyDown)  m_offsetY -= spd;
}

// ── Render ────────────────────────────────────────────────────────────────────

void TileBuilderState::render() {
    auto& app = Application::get();
    int sw = app.width(), sh = app.height();

    m_hud.begin(sw, sh);

    // Dark canvas background
    m_hud.drawRect(0, 0, sw - PREVIEW_W, sh - STATUS_H, {0.10f, 0.09f, 0.13f, 1.f});

    if (m_sheet.tex) {
        renderSheet();
        renderGrid();
        renderSelection();
    } else if (!m_showBrowser) {
        // No sheet yet — hint
        m_hud.drawText(40, sh / 2.f - 10, 2.f,
                       "Press O to open a PNG sheet", {0.55f, 0.55f, 0.5f, 1.f});
    }

    renderPreviewPanel();
    renderStatusBar();

    if (m_showExportDialog)
        renderExportDialog(sw, sh);
    else if (m_showBrowser)
        renderBrowser(sw, sh);

    glDisable(GL_BLEND);
}

void TileBuilderState::renderSheet() {
    float dw = m_sheet.w * m_zoom;
    float dh = m_sheet.h * m_zoom;
    // Checkerboard-style background so transparent pixels are visible
    m_hud.drawRect(m_offsetX - 1, m_offsetY - 1, dw + 2, dh + 2,
                   {0.35f, 0.35f, 0.35f, 1.f});
    m_hud.drawTexturedRect(m_offsetX, m_offsetY, dw, dh, m_sheet.tex);
}

void TileBuilderState::renderGrid() {
    if (m_cellW <= 0 || m_cellH <= 0) return;
    int numCols = m_sheet.w / m_cellW;
    int numRows = m_sheet.h / m_cellH;
    if (numCols <= 0 || numRows <= 0) return;

    float cellPxW   = m_cellW * m_zoom;
    float cellPxH   = m_cellH * m_zoom;
    float sheetPxW  = m_sheet.w * m_zoom;
    float sheetPxH  = m_sheet.h * m_zoom;
    float lineW     = std::max(1.f, m_zoom * 0.4f);
    glm::vec4 lineC = {0.2f, 0.85f, 0.9f, 0.45f};

    for (int c = 0; c <= numCols; ++c)
        m_hud.drawRect(m_offsetX + c * cellPxW, m_offsetY, lineW, sheetPxH, lineC);
    for (int r = 0; r <= numRows; ++r)
        m_hud.drawRect(m_offsetX, m_offsetY + r * cellPxH, sheetPxW, lineW, lineC);
}

void TileBuilderState::renderSelection() {
    float cellPxW = m_cellW * m_zoom;
    float cellPxH = m_cellH * m_zoom;
    float brd     = std::max(2.f, m_zoom * 0.6f);

    for (const auto& [col, row] : m_selected) {
        float sx = m_offsetX + col * cellPxW;
        float sy = m_offsetY + row * cellPxH;
        // Fill
        m_hud.drawRect(sx, sy, cellPxW, cellPxH, {0.25f, 0.85f, 0.45f, 0.30f});
        // Border
        m_hud.drawRect(sx,            sy,            cellPxW, brd,     {0.3f, 1.f, 0.5f, 0.9f});
        m_hud.drawRect(sx,            sy+cellPxH-brd, cellPxW, brd,    {0.3f, 1.f, 0.5f, 0.9f});
        m_hud.drawRect(sx,            sy,            brd,     cellPxH, {0.3f, 1.f, 0.5f, 0.9f});
        m_hud.drawRect(sx+cellPxW-brd, sy,           brd,     cellPxH, {0.3f, 1.f, 0.5f, 0.9f});
    }
}

void TileBuilderState::renderPreviewPanel() {
    auto& app = Application::get();
    int sw = app.width(), sh = app.height();
    float px = sw - PREVIEW_W;
    float ph = sh - STATUS_H;

    // Panel background + left edge
    m_hud.drawRect(px, 0, PREVIEW_W, ph, {0.07f, 0.06f, 0.10f, 0.97f});
    m_hud.drawRect(px, 0, 2.f, ph, {0.30f, 0.28f, 0.45f, 1.f});

    // Header
    m_hud.drawText(px + 8, 8,  1.3f, "Selected Tiles", {0.80f, 0.80f, 0.50f, 1.f});
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d cell%s",
                  (int)m_selected.size(), m_selected.size() == 1 ? "" : "s");
    m_hud.drawText(px + 8, 22, 1.0f, buf, {0.50f, 0.80f, 0.50f, 1.f});
    m_hud.drawRect(px + 4, 36, PREVIEW_W - 8, 1.f, {0.30f, 0.30f, 0.40f, 1.f});

    // Thumbnail list
    if (m_sheet.tex && m_sheet.w > 0 && m_sheet.h > 0) {
        float sw_  = static_cast<float>(m_sheet.w);
        float sh_  = static_cast<float>(m_sheet.h);
        float thumbH = 48.f;
        float listY  = 42.f;
        float rowH   = thumbH + 18.f;
        int i = 0;
        for (const auto& [col, row] : m_selected) {
            float ty = listY + i * rowH;
            if (ty + rowH > ph - 48.f) {
                // Show overflow count
                int remaining = static_cast<int>(m_selected.size()) - i;
                std::snprintf(buf, sizeof(buf), "... +%d more", remaining);
                m_hud.drawText(px + 8, ty + 4, 1.0f, buf, {0.55f, 0.55f, 0.45f, 1.f});
                break;
            }
            // UV for this cell
            float u0 = col * m_cellW / sw_;
            float v0 = row * m_cellH / sh_;
            float u1 = u0 + m_cellW / sw_;
            float v1 = v0 + m_cellH / sh_;
            // Keep thumb square; fit the smaller axis
            float aspect = (m_cellH > 0) ? (float)m_cellW / m_cellH : 1.f;
            float tw = (aspect >= 1.f) ? thumbH * aspect : thumbH;
            float th = (aspect >= 1.f) ? thumbH : thumbH / aspect;
            tw = std::min(tw, PREVIEW_W - 16.f);
            m_hud.drawRect(px + 6,     ty,     tw, th, {0.15f, 0.12f, 0.18f, 1.f});
            m_hud.drawTexturedRectUV(px + 6, ty, tw, th,
                                     m_sheet.tex, u0, v0, u1, v1);
            std::snprintf(buf, sizeof(buf), "c%d r%d", col, row);
            m_hud.drawText(px + tw + 12, ty + (th - 8) * 0.5f,
                           1.0f, buf, {0.85f, 0.85f, 0.75f, 1.f});
            ++i;
        }
    }

    // Export button (bottom of panel)
    if (!m_selected.empty()) {
        float bx = px + 6.f;
        float by = ph - 36.f;
        float bw = PREVIEW_W - 12.f;
        float bh = 28.f;
        m_hud.drawRect(bx+2, by+2, bw, bh, {0.f, 0.f, 0.f, 0.5f});
        m_hud.drawRect(bx, by, bw, bh, {0.20f, 0.40f, 0.12f, 1.f});
        m_hud.drawRect(bx,      by,      bw, 2.f, {0.35f, 0.80f, 0.25f, 1.f});
        m_hud.drawRect(bx,      by+bh-2, bw, 2.f, {0.35f, 0.80f, 0.25f, 1.f});
        m_hud.drawRect(bx,      by,      2.f, bh, {0.35f, 0.80f, 0.25f, 1.f});
        m_hud.drawRect(bx+bw-2, by,      2.f, bh, {0.35f, 0.80f, 0.25f, 1.f});
        m_hud.drawText(bx + 16, by + 8, 1.3f, "Export  (Enter)", {1.f, 0.95f, 0.75f, 1.f});
    }
}

void TileBuilderState::renderStatusBar() {
    auto& app = Application::get();
    int sw = app.width(), sh = app.height();
    float by = sh - STATUS_H;

    m_hud.drawRect(0, by, sw, STATUS_H, {0.06f, 0.05f, 0.08f, 0.97f});
    m_hud.drawRect(0, by, sw, 1.f, {0.28f, 0.28f, 0.38f, 1.f});

    char buf[320];
    if (m_sheet.tex) {
        int numCols = m_cellW > 0 ? m_sheet.w / m_cellW : 0;
        int numRows = m_cellH > 0 ? m_sheet.h / m_cellH : 0;
        std::snprintf(buf, sizeof(buf),
            "%s  |  %dx%d  |  Cell %dx%d  |  Grid %dx%d  "
            "|  +/-=cellW  [/]=cellH  Arrows=pan  Scroll=zoom  R=fit  O=open  ESC=back",
            m_sheetPath.c_str(),
            m_sheet.w, m_sheet.h,
            m_cellW, m_cellH,
            numCols, numRows);
    } else {
        std::snprintf(buf, sizeof(buf), "No sheet loaded — O = open a PNG  |  ESC = back");
    }
    m_hud.drawText(8, by + 9, 1.0f, buf, {0.65f, 0.65f, 0.55f, 1.f});
}

// ── File browser ──────────────────────────────────────────────────────────────

void TileBuilderState::openBrowser() {
    namespace fs = std::filesystem;
    m_browserFiles.clear();
    if (fs::is_directory("assets")) {
        for (const auto& e : fs::recursive_directory_iterator("assets")) {
            if (e.is_regular_file() && e.path().extension() == ".png")
                m_browserFiles.push_back(e.path().string());
        }
    }
    std::sort(m_browserFiles.begin(), m_browserFiles.end());
    m_browserScroll  = 0;
    m_browserHovered = -1;
    m_showBrowser    = true;
}

void TileBuilderState::renderBrowser(int sw, int sh) {
    namespace fs = std::filesystem;
    const float scale = sh / 600.f;
    const float rowH  = 24.f * scale;
    const float panW  = sw * 0.65f;
    const float panX  = (sw - panW) * 0.5f;
    const float panY  = sh * 0.12f;
    const int   rows  = std::min(BROWSER_ROWS,
                                  static_cast<int>(m_browserFiles.size()));
    const float listY = panY + 2 * rowH;
    const float panH  = (rows + 3) * rowH;
    const float brd   = 2.f * scale;

    // Veil
    m_hud.drawRect(0, 0, sw, sh, {0.f, 0.f, 0.f, 0.6f});

    // Panel
    m_hud.drawRect(panX, panY, panW, panH, {0.08f, 0.06f, 0.12f, 0.97f});
    m_hud.drawRect(panX,          panY,          panW, brd,  {0.4f, 0.3f, 0.7f, 1.f});
    m_hud.drawRect(panX,          panY+panH-brd, panW, brd,  {0.4f, 0.3f, 0.7f, 1.f});
    m_hud.drawRect(panX,          panY,          brd,  panH, {0.4f, 0.3f, 0.7f, 1.f});
    m_hud.drawRect(panX+panW-brd, panY,          brd,  panH, {0.4f, 0.3f, 0.7f, 1.f});

    m_hud.drawText(panX + 8*scale, panY + 6*scale,
                   1.8f*scale, "Open Spritesheet", {0.85f, 0.75f, 1.0f, 1.f});

    if (m_browserFiles.empty()) {
        m_hud.drawText(panX + 8*scale, listY + 4*scale,
                       1.2f*scale, "(no PNG files found in assets/)",
                       {0.55f, 0.55f, 0.5f, 1.f});
    }

    for (int i = 0; i < rows; ++i) {
        int   idx = i + m_browserScroll;
        if (idx >= static_cast<int>(m_browserFiles.size())) break;
        float ry  = listY + i * rowH;
        bool  hov = (idx == m_browserHovered);
        bool  cur = (m_browserFiles[idx] == m_sheetPath);

        m_hud.drawRect(panX + 4*scale, ry + 2*scale,
                       panW - 8*scale, rowH - 4*scale,
                       hov ? glm::vec4{0.30f, 0.22f, 0.48f, 1.f}
                           : glm::vec4{0.13f, 0.10f, 0.18f, 1.f});

        std::string label = m_browserFiles[idx];
        if (cur) label = "> " + label;
        m_hud.drawText(panX + 10*scale, ry + 6*scale,
                       1.2f*scale, label.c_str(),
                       cur ? glm::vec4{0.9f, 0.85f, 1.0f, 1.f}
                           : glm::vec4{0.75f, 0.72f, 0.65f, 1.f});
    }

    // Hint
    float hintY = listY + rows * rowH + 4*scale;
    m_hud.drawText(panX + 8*scale, hintY, scale,
                   "Click to load   |   Scroll / Arrow Up/Down to browse   |   ESC = cancel",
                   {0.50f, 0.50f, 0.40f, 0.85f});
}

// ── Export dialog ─────────────────────────────────────────────────────────────

void TileBuilderState::openExportDialog() {
    if (m_selected.empty()) return;
    // Pre-fill with last export name, or derive from sheet filename
    if (m_exportName.empty()) {
        namespace fs = std::filesystem;
        m_exportName = fs::path(m_sheetPath).stem().string();
    }
    m_showExportDialog = true;
    SDL_StartTextInput();
}

void TileBuilderState::commitExport() {
    namespace fs = std::filesystem;
    std::string name = m_exportName;
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (name.empty()) name = "tileset";

    std::string outDir  = "assets/textures/custom/" + name;
    std::string jPath   = "data/tilesets/" + name + ".json";
    fs::create_directories(outDir);
    fs::create_directories("data/tilesets");

    nlohmann::json j;
    j["name"]  = name;
    j["sheet"] = m_sheetPath;
    j["cellW"] = m_cellW;
    j["cellH"] = m_cellH;
    j["cells"] = nlohmann::json::array();

    int exported = 0;
    for (const auto& [col, row] : m_selected) {
        int px0 = col * m_cellW;
        int py0 = row * m_cellH;
        int pw  = std::min(m_cellW, m_sheet.w - px0);
        int ph  = std::min(m_cellH, m_sheet.h - py0);
        if (pw <= 0 || ph <= 0) continue;

        // Crop from raw pixel buffer
        std::vector<uint8_t> crop(pw * ph * 4);
        for (int y = 0; y < ph; ++y) {
            for (int x = 0; x < pw; ++x) {
                int src = ((py0 + y) * m_sheet.w + (px0 + x)) * 4;
                int dst = (y * pw + x) * 4;
                crop[dst+0] = m_sheet.pixels[src+0];
                crop[dst+1] = m_sheet.pixels[src+1];
                crop[dst+2] = m_sheet.pixels[src+2];
                crop[dst+3] = m_sheet.pixels[src+3];
            }
        }

        char fname[64];
        std::snprintf(fname, sizeof(fname), "c%d_r%d.png", col, row);
        std::string fpath = outDir + "/" + fname;
        savePNG(fpath, crop.data(), pw, ph);

        nlohmann::json cell;
        cell["col"]  = col;
        cell["row"]  = row;
        cell["file"] = fpath;
        j["cells"].push_back(cell);
        ++exported;
    }

    std::ofstream jf(jPath);
    jf << j.dump(2) << "\n";

    std::cout << "[TileBuilder] Exported " << exported
              << " tiles  →  " << outDir << "\n";
    std::cout << "[TileBuilder] Tileset JSON: " << jPath << "\n";

    m_showExportDialog = false;
    SDL_StopTextInput();
}

void TileBuilderState::renderExportDialog(int sw, int sh) {
    const float scale = sh / 600.f;
    const float panW  = sw * 0.44f;
    const float panH  = 120.f * scale;
    const float panX  = (sw - panW) * 0.5f;
    const float panY  = sh * 0.38f;
    const float brd   = 2.f * scale;

    // Veil
    m_hud.drawRect(0, 0, sw, sh, {0.f, 0.f, 0.f, 0.60f});

    // Panel
    m_hud.drawRect(panX, panY, panW, panH, {0.07f, 0.05f, 0.10f, 0.97f});
    m_hud.drawRect(panX,          panY,          panW, brd,  {0.4f, 0.3f, 0.7f, 1.f});
    m_hud.drawRect(panX,          panY+panH-brd, panW, brd,  {0.4f, 0.3f, 0.7f, 1.f});
    m_hud.drawRect(panX,          panY,          brd,  panH, {0.4f, 0.3f, 0.7f, 1.f});
    m_hud.drawRect(panX+panW-brd, panY,          brd,  panH, {0.4f, 0.3f, 0.7f, 1.f});

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Export %d tile%s as tileset:",
                  (int)m_selected.size(), m_selected.size() == 1 ? "" : "s");
    m_hud.drawText(panX + 10*scale, panY + 8*scale,
                   1.7f*scale, buf, {0.85f, 0.75f, 1.0f, 1.f});

    // Text field
    const float fx = panX + 10*scale;
    const float fy = panY + 38*scale;
    const float fw = panW - 20*scale;
    const float fh = 28*scale;
    m_hud.drawRect(fx, fy, fw, fh, {0.12f, 0.09f, 0.16f, 1.f});
    m_hud.drawRect(fx,      fy,      fw, brd,  {0.55f, 0.40f, 0.80f, 1.f});
    m_hud.drawRect(fx,      fy+fh-brd, fw, brd, {0.55f, 0.40f, 0.80f, 1.f});
    m_hud.drawRect(fx,      fy,      brd, fh,  {0.55f, 0.40f, 0.80f, 1.f});
    m_hud.drawRect(fx+fw-brd, fy,    brd, fh,  {0.55f, 0.40f, 0.80f, 1.f});

    std::string display = m_exportName + "_";
    m_hud.drawText(fx + 6*scale, fy + 8*scale,
                   1.5f*scale, display.c_str(), {1.f, 0.95f, 0.75f, 1.f});

    m_hud.drawText(panX + 10*scale, panY + 82*scale,
                   scale, "Enter = export   ESC = cancel   Backspace = delete",
                   {0.50f, 0.50f, 0.42f, 0.85f});
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool TileBuilderState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);
    auto& app    = Application::get();

    // ── Export dialog modal ──────────────────────────────────────────────────
    if (m_showExportDialog) {
        if (e->type == SDL_KEYDOWN) {
            switch (e->key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER: commitExport();             return true;
                case SDLK_ESCAPE:
                    m_showExportDialog = false;
                    SDL_StopTextInput();
                    return true;
                case SDLK_BACKSPACE:
                    if (!m_exportName.empty()) m_exportName.pop_back();
                    return true;
                default: break;
            }
        }
        if (e->type == SDL_TEXTINPUT) {
            for (char c : std::string(e->text.text))
                if (std::isalnum(static_cast<unsigned char>(c)) ||
                    c == '_' || c == '-' || c == ' ')
                    m_exportName += c;
            return true;
        }
        return true;  // swallow all other input
    }

    // ── File browser modal ───────────────────────────────────────────────────
    if (m_showBrowser) {
        const int sw = app.width(), sh = app.height();
        const float scale = sh / 600.f;
        const float rowH  = 24.f * scale;
        const float panW  = sw * 0.65f;
        const float panX  = (sw - panW) * 0.5f;
        const float panY  = sh * 0.12f;
        const float listY = panY + 2 * rowH;
        const int   rows  = std::min(BROWSER_ROWS,
                                      static_cast<int>(m_browserFiles.size()));

        if (e->type == SDL_KEYDOWN) {
            switch (e->key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_o:
                    m_showBrowser = false;
                    return true;
                case SDLK_UP:
                    if (m_browserScroll > 0) --m_browserScroll;
                    return true;
                case SDLK_DOWN:
                    if (m_browserScroll + rows <
                        static_cast<int>(m_browserFiles.size()))
                        ++m_browserScroll;
                    return true;
                default: break;
            }
        }
        if (e->type == SDL_MOUSEWHEEL) {
            m_browserScroll = std::max(0, std::min(
                m_browserScroll - e->wheel.y,
                static_cast<int>(m_browserFiles.size()) - rows));
            return true;
        }
        if (e->type == SDL_MOUSEMOTION) {
            float mx = e->motion.x, my = e->motion.y;
            m_browserHovered = -1;
            if (mx >= panX && mx <= panX + panW) {
                int rel = static_cast<int>((my - listY) / rowH);
                int idx = rel + m_browserScroll;
                if (rel >= 0 && idx < static_cast<int>(m_browserFiles.size()))
                    m_browserHovered = idx;
            }
            return true;
        }
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            float mx = e->button.x, my = e->button.y;
            if (mx >= panX && mx <= panX + panW) {
                int rel = static_cast<int>((my - listY) / rowH);
                int idx = rel + m_browserScroll;
                if (rel >= 0 && idx < static_cast<int>(m_browserFiles.size())) {
                    loadSheet(m_browserFiles[idx]);
                    m_showBrowser = false;
                    return true;
                }
            }
            // Click outside = cancel
            m_showBrowser = false;
            return true;
        }
        return true;
    }

    // ── Normal input ─────────────────────────────────────────────────────────

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode sym = e->key.keysym.sym;

        if (sym == SDLK_ESCAPE && !e->key.repeat) { Application::get().popState(); return true; }
        if (sym == SDLK_o)       { openBrowser(); return true; }
        if (sym == SDLK_r && m_sheet.tex) { fitToScreen(); return true; }

        // Cell size
        if (sym == SDLK_EQUALS || sym == SDLK_PLUS)
            m_cellW = std::min(m_cellW + CELL_STEP, CELL_MAX);
        if (sym == SDLK_MINUS)
            m_cellW = std::max(m_cellW - CELL_STEP, CELL_MIN);
        if (sym == SDLK_RIGHTBRACKET)
            m_cellH = std::min(m_cellH + CELL_STEP, CELL_MAX);
        if (sym == SDLK_LEFTBRACKET)
            m_cellH = std::max(m_cellH - CELL_STEP, CELL_MIN);

        // Pan keys
        if (sym == SDLK_LEFT)  { m_keyLeft  = true; return true; }
        if (sym == SDLK_RIGHT) { m_keyRight = true; return true; }
        if (sym == SDLK_UP)    { m_keyUp    = true; return true; }
        if (sym == SDLK_DOWN)  { m_keyDown  = true; return true; }

        // Export
        if ((sym == SDLK_RETURN || sym == SDLK_KP_ENTER) && !m_selected.empty())
            openExportDialog();

        return true;
    }

    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_LEFT:  m_keyLeft  = false; return true;
            case SDLK_RIGHT: m_keyRight = false; return true;
            case SDLK_UP:    m_keyUp    = false; return true;
            case SDLK_DOWN:  m_keyDown  = false; return true;
            default: break;
        }
    }

    // ── Zoom (scroll wheel) ──────────────────────────────────────────────────
    if (e->type == SDL_MOUSEWHEEL && m_sheet.tex) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (inSheetArea(mx)) {
            float sx = (mx - m_offsetX) / m_zoom;
            float sy = (my - m_offsetY) / m_zoom;
            int delta = (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                        ? -e->wheel.y : e->wheel.y;
            m_zoom = std::clamp(m_zoom * (1.f + delta * ZOOM_STEP),
                                ZOOM_MIN, ZOOM_MAX);
            m_offsetX = mx - sx * m_zoom;
            m_offsetY = my - sy * m_zoom;
        }
        return true;
    }

    // ── Mouse hover (browser hovered not active here) ────────────────────────
    if (e->type == SDL_MOUSEMOTION) {
        if (m_leftHeld && m_sheet.tex) {
            int mx = e->motion.x, my = e->motion.y;
            if (inSheetArea(mx)) {
                int col, row;
                screenToCell(mx, my, col, row);
                if (col >= 0 && row >= 0 &&
                    (col != m_lastDragCol || row != m_lastDragRow)) {
                    m_lastDragCol = col;
                    m_lastDragRow = row;
                    auto cell = std::make_pair(col, row);
                    if (m_toggleState) m_selected.insert(cell);
                    else               m_selected.erase(cell);
                }
            }
        }
        return false;
    }

    // ── Mouse click ──────────────────────────────────────────────────────────
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (inSheetArea(mx) && m_sheet.tex) {
            m_leftHeld = true;
            int col, row;
            screenToCell(mx, my, col, row);
            if (col >= 0 && row >= 0) {
                auto cell = std::make_pair(col, row);
                m_toggleState = !m_selected.count(cell);
                m_lastDragCol = col;
                m_lastDragRow = row;
                if (m_toggleState) m_selected.insert(cell);
                else               m_selected.erase(cell);
            }
        }
        return true;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        m_leftHeld    = false;
        m_lastDragCol = m_lastDragRow = -1;
        return false;
    }

    if (e->type == SDL_WINDOWEVENT &&
        e->window.event == SDL_WINDOWEVENT_RESIZED) {
        if (m_sheet.tex) fitToScreen();
    }

    return false;
}
