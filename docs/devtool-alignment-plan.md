# Dev-Tool: Tile Alignment Editor (F1)

**Goal:** Fix visual misalignment of terrain tiles and objects by providing an in-editor tool
that lets you nudge any element's world-space position and persist the results to a config file.

---

## Overview

- Press **F1** in WorldBuilderState to toggle the tool overlay
- Click any tile to select it
- Arrow keys nudge the element's X/Z position; `[`/`]` nudge Y (height)
- Two modes: **Global** (all tiles of that terrain/obj type) vs **Per-tile** (one hex only)
- Toggle between editing the **terrain tile** or the **object on that tile**
- Changes auto-save to `assets/render_offsets.json`
- `AdventureState` loads the same config file at startup so the game matches the editor

---

## Step 1 — `RenderOffsets.h/cpp` (new files)

**`src/render/RenderOffsets.h`**

```cpp
#pragma once
#include <array>
#include <unordered_map>
#include "../hex/HexCoord.h"
#include "../world/MapTile.h"    // Terrain, TERRAIN_COUNT
#include "../world/MapObject.h"  // ObjType, OBJ_TYPE_COUNT

struct RenderOffset {
    float dx = 0.0f;   // world-space X shift (left/right on screen)
    float dz = 0.0f;   // world-space Z shift (into/out of screen plane)
    float dy = 0.0f;   // world-space Y shift (stacks on top of terrain height)
};

class RenderOffsetConfig {
public:
    // --- data ------------------------------------------------------------------
    std::array<RenderOffset, TERRAIN_COUNT>   terrainGlobal{};   // per Terrain type
    std::array<RenderOffset, OBJ_TYPE_COUNT>  objectGlobal{};    // per ObjType
    std::unordered_map<HexCoord, RenderOffset> tileOverride;     // per-hex terrain
    std::unordered_map<HexCoord, RenderOffset> objectOverride;   // per-hex object

    // --- lookup ----------------------------------------------------------------
    // Returns per-tile override if present, else the global default for that type.
    RenderOffset forTerrain(const HexCoord& c, Terrain t) const;
    RenderOffset forObject (const HexCoord& c, ObjType  t) const;

    // --- persistence -----------------------------------------------------------
    // Returns nullopt on success, error string on failure (same convention as WorldMap).
    std::optional<std::string> save(const std::string& path) const;
    std::optional<std::string> load(const std::string& path);

    void reset();  // zero all offsets
};
```

**`src/render/RenderOffsets.cpp`**

- Implement `forTerrain` / `forObject` with map-lookup + fallback
- Serialize / deserialize using nlohmann/json
  - JSON layout:
    ```json
    {
      "terrainGlobal":  { "Sand": {"dx":0.0,"dz":0.0,"dy":0.0}, ... },
      "objectGlobal":   { "Town": {"dx":0.0,"dz":0.0,"dy":0.0}, ... },
      "tileOverride":   { "q,r": {"dx":0.05,"dz":0.0,"dy":0.1}, ... },
      "objectOverride": { "q,r": {"dx":0.0,"dz":0.0,"dy":-0.05}, ... }
    }
    ```
  - Key for HexCoord: `std::to_string(c.q)+","+std::to_string(c.r)`
- `reset()` zeroes all arrays and clears both maps
- File path: `assets/render_offsets.json` (created if missing on first save)

---

## Step 2 — Extend `HexRenderer::drawTile` / `drawOutline`

**`src/render/HexRenderer.h`** — add optional offset to both signatures:

```cpp
void drawTile   (const HexCoord& coord,
                 const glm::vec3& color,
                 float visualScale  = 1.0f,
                 float height       = 0.0f,
                 GLuint texId       = 0,
                 const glm::vec2& xzOffset = {0.0f, 0.0f});  // NEW

void drawOutline(const HexCoord& coord,
                 const glm::vec3& color,
                 float visualScale  = 1.0f,
                 float height       = 0.0f,
                 const glm::vec2& xzOffset = {0.0f, 0.0f});  // NEW
```

**`src/render/HexRenderer.cpp`**

Inside both functions, where the world XZ position is computed from `coord`,
add the offset before building the model matrix:

```cpp
glm::vec3 worldPos = hexToWorld(coord);   // existing call
worldPos.x += xzOffset.x;                // NEW
worldPos.z += xzOffset.y;                // NEW (y of vec2 = z in 3D)
// ... then build model matrix using worldPos and height as before
```

The `height` parameter (passed as worldPos.y) is unchanged; only X/Z shifts.

---

## Step 3 — `WorldBuilderState` — dev tool integration

### 3a. New member variables (`WorldBuilderState.h`)

```cpp
// --- Dev Tool (F1) ---
bool            m_devToolActive  = false;
HexCoord        m_devSelected;
bool            m_hasDevSelected = false;

enum class DevMode  { Global, PerTile };
enum class DevEdit  { Terrain, Object  };
DevMode         m_devMode   = DevMode::Global;
DevEdit         m_devEdit   = DevEdit::Terrain;

RenderOffsetConfig m_offsets;                    // loaded on onEnter
static constexpr float NUDGE_STEP = 0.02f;       // world units per key press
```

### 3b. `onEnter()` — load config

```cpp
// After existing setup:
auto err = m_offsets.load("assets/render_offsets.json");
// Ignore file-not-found (first run), log other errors
```

### 3c. `handleEvent()` additions

**F1 (SDLK_F1):**
```
m_devToolActive = !m_devToolActive;
if (!m_devToolActive) m_hasDevSelected = false;
```

**Left-click (when m_devToolActive):**
```
m_devSelected    = m_hovered;
m_hasDevSelected = m_hasHovered;
(do NOT apply the normal paint/erase tool when dev tool is active)
```

**Arrow keys (when m_devToolActive && m_hasDevSelected):**
```
Left  → nudge dx -= NUDGE_STEP
Right → nudge dx += NUDGE_STEP
Up    → nudge dz -= NUDGE_STEP   (screen-up = negative Z)
Down  → nudge dz += NUDGE_STEP
```
`[` → nudge dy -= NUDGE_STEP
`]` → nudge dy += NUDGE_STEP

"Nudge" means: update either the Global or PerTile offset for the selected element type,
then auto-save.

**Key G:** `m_devMode = DevMode::Global`
**Key T:** `m_devMode = DevMode::PerTile`
**Key B:** toggle `m_devEdit` between Terrain and Object
**Key R (with F1 active):** reset offset for selected tile/type to {0,0,0}
**Ctrl+O:** force-save `m_offsets` (also happens automatically after each nudge)

**Normal tool keys (1–0, Tab, O, P, Ctrl+S/L/N) must still work** even when dev tool is
active. Only left-click is intercepted.

### 3d. `renderTerrain()` — pass offsets

```cpp
for (auto& [coord, tile] : m_map.tiles()) {
    RenderOffset off = m_offsets.forTerrain(coord, tile.terrain);
    GLuint tex = m_hexRenderer.terrainTex(tile.terrain);
    m_hexRenderer.drawTile(
        coord,
        terrainColor(tile.terrain),
        HEX_SIZE,
        terrainHeight(tile.terrain) + off.dy,   // stacks
        tex,
        {off.dx, off.dz}                         // NEW xzOffset
    );
}
```

### 3e. `renderObjects()` — pass offsets

```cpp
for (auto& obj : m_map.objects()) {
    RenderOffset off = m_offsets.forObject(obj.pos, obj.type);
    m_hexRenderer.drawTile(
        obj.pos,
        objTypeColor(obj.type),
        HEX_SIZE * 0.55f,
        0.25f + off.dy,
        /*texId=*/0,
        {off.dx, off.dz}
    );
}
```

### 3f. Dev tool HUD panel

Add `renderDevToolHUD()`, called at the end of `render()` when `m_devToolActive`:

```
drawRect(10, 10, 300, 200, semi-transparent dark overlay)
drawText "=== ALIGNMENT EDITOR (F1=close) ==="
drawText "Click tile to select"
drawText "Arrows=X/Z   [/]=Y(height)"
drawText "G=Global mode  T=Per-tile mode"
drawText "B=Terrain/Object  R=Reset"

If m_hasDevSelected:
  drawText "Tile: q=%d r=%d  (%s)"  coord, terrain name
  drawText "Mode: %s   Editing: %s"  Global/PerTile, Terrain/Object
  drawText "dx=%.3f  dz=%.3f  dy=%.3f"  current offset values
  drawOutline(m_devSelected, {1,1,0}, 1.02f)   // bright yellow selection ring
```

Use `HUDRenderer::drawRect` and `HUDRenderer::drawText` for all of this.
(HUDRenderer::begin(w,h) must be called beforehand.)

---

## Step 4 — `AdventureState` — load and apply offsets

### 4a. `AdventureState.h`

```cpp
#include "../render/RenderOffsets.h"
// ...
RenderOffsetConfig m_offsets;
```

### 4b. `AdventureState::onEnter()`

```cpp
m_offsets.load("assets/render_offsets.json");  // ignore missing-file
```

### 4c. Terrain and object draw calls in `AdventureState::render()`

Same pattern as WorldBuilderState — look up offset via `m_offsets.forTerrain()`
and `m_offsets.forObject()`, pass `xzOffset` and stack `dy` onto height.

---

## Step 5 — `CMakeLists.txt`

Add `src/render/RenderOffsets.cpp` to the `raids_of_umm` sources list.
Add it to `raids_tests` sources too (so offset loading can be tested if desired).

---

## File Change Summary

| File | Action | What changes |
|------|--------|-------------|
| `src/render/RenderOffsets.h` | **CREATE** | data struct + config class declaration |
| `src/render/RenderOffsets.cpp` | **CREATE** | lookup, JSON save/load, reset |
| `src/render/HexRenderer.h` | **MODIFY** | add `xzOffset` param to drawTile/drawOutline |
| `src/render/HexRenderer.cpp` | **MODIFY** | apply xzOffset to world position before model matrix |
| `src/worldbuilder/WorldBuilderState.h` | **MODIFY** | add dev tool members |
| `src/worldbuilder/WorldBuilderState.cpp` | **MODIFY** | F1 toggle, click select, arrow nudge, renderDevToolHUD, pass offsets to draw calls |
| `src/adventure/AdventureState.h` | **MODIFY** | add m_offsets member |
| `src/adventure/AdventureState.cpp` | **MODIFY** | load offsets in onEnter, pass to draw calls |
| `CMakeLists.txt` | **MODIFY** | add RenderOffsets.cpp to both targets |

---

## Control Reference Card

| Key | Action |
|-----|--------|
| F1 | Toggle dev tool overlay |
| Left-click | Select tile under cursor |
| ← → ↑ ↓ | Nudge X/Z by 0.02 world units |
| `[` / `]` | Nudge Y (height) by 0.02 world units |
| G | Switch to Global mode |
| T | Switch to Per-tile mode |
| B | Toggle editing Terrain / Object on selected tile |
| R | Reset current offset to 0,0,0 |
| Ctrl+O | Force-save render_offsets.json |

Normal editor keys (1–0, Tab, O, P, Ctrl+S/L/N, WASD) work as usual.

---

## Deferred (noted for later)

- Terrain variants (m1, m2, m3) — require adding a `variant` field to `MapTile`
  and storing per-variant textures; `drawTile` would need a variant ID param
- Object sprite rendering via `SpriteRenderer` instead of `drawTile` coloured quads
  (once per-object sprites are wired into AdventureState, the offset system above
  extends naturally — just pass `xzOffset` to `SpriteRenderer::draw`)
