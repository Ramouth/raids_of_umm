# Raids of Umm'Natur — Graphics Discussion

> **Guiding principle:** "Well enough" means the game is legible and not embarrassing
> in motion. It does not mean complete or polished. Phase 3 visual polish is
> deliberately deferred (see ROADMAP.md). This document covers the practical
> pipeline for generating assets that clear that bar now.

---

## What We Have

### Terrain (hex tiles, 128×128)
All terrain types on the default map are covered:

| Terrain | Files | Notes |
|---------|-------|-------|
| sand | sand.png + sand1–4 variants | ✅ primary ground |
| dune | dune.png + variants | ✅ |
| mountain | mountain.png | ✅ |
| rock | rock/ | ✅ |
| oasis | oasis/ | ✅ |
| ruins | ruins/ | ✅ |
| obsidian | obsidian/ | ✅ |
| river | river/ | ✅ |
| road | road.png | ✅ |
| forest | forest/ | ✅ |
| grass | grass/ | present but unused on default map |
| highland | highland/ | present but unused |
| wall | wall/ | present |

**Verdict:** Terrain coverage is complete for the current map. No urgent work here.

### Map Objects (buildings, resources, special sites)
`assets/textures/objects/` and `assets/textures/buildings/`:

| Object | Status |
|--------|--------|
| town, castle | ✅ |
| dungeon | ✅ |
| goldmine, crystal_mine | ✅ |
| artifact | ✅ |
| sawmill | ⬜ missing |
| quarry | ⬜ missing |
| obsidian_vent | ⬜ missing |

**Verdict:** Three mine-type objects are missing — these are needed for Milestone 1
economy content (sawmills and quarries exist in code, not art).

### Unit Sprites (128×128)
`assets/textures/units/`:

| Unit | Files |
|------|-------|
| armoured_warrior | idle, walk, anim ✅ |
| enemy_scout | single ✅ |
| rider_archer, _banner, _knight, _lance | ✅ |
| djinn | ✅ |
| mummy | ✅ |
| pharaoh_lich | ✅ |
| ancient_guardian | ✅ |
| khet | ✅ |
| kharim | ✅ |
| ushari (SC) | ✅ |
| sekhara (SC) | ✅ |

**Verdict:** The initial faction (desert/undead flavour) has decent coverage.
Full 6-faction rosters are a Phase 3 concern. Combat is legible now.

---

## Generation Tools Available

### 1. PixelLab MCP (`mcp__pixellab__*`)
Claude can call these directly in-session. Async — returns a job ID, then poll
`get_*` to retrieve the result. No Python script needed.

| Tool | Use for |
|------|---------|
| `create_tiles_pro` | Terrain hex tiles — **primary terrain tool** |
| `create_map_object` | Buildings, mines, dungeons, artifacts |
| `create_character` | Unit sprites with directional views |
| `animate_character` | Walk / attack cycles from a base sprite |
| `create_topdown_tileset` | Wang autotile sets (edge blending) — Phase 3 |
| `create_isometric_tile` | Not relevant — we use flat-top hex, not iso |

### 2. `scripts/pixellab_client.py`
CLI wrapper for the HTTP API. Use when batching many assets outside a Claude
session, or when MCP isn't available.

```bash
python3 scripts/pixellab_client.py pixflux "sawmill, egyptian, desert" \
    --size 128x128 --out assets/textures/objects/sawmill.png
```

### 3. PIL post-processing (`scripts/remove_bg.py`)
Removes background pixels via green-screen or threshold logic. Run after
any `create_character` / `create_map_object` result that has a solid background.

---

## Terrain Rendering — Decision Tree

Five independent decisions. Each affects the others. All combinations are valid
but some combinations are incoherent (marked ✗).

---

### Decision 1 — Geometry: what shape is each terrain "tile"?

```
                     ┌─ A. Hex mesh per tile (current)
                     ├─ B. Square quad per tile
Geometry ────────────┤
                     ├─ C. Single large quad (whole map)
                     └─ D. Unified hex mesh (one VBO for all tiles)
```

**A. Hex mesh per tile** *(current)*
- Pro: exact hex shape, click detection matches visual
- Pro: rotation/variant works naturally per tile
- Con: per-tile draw calls (N tiles = N GL calls)
- Con: shared triangle edges cause sub-pixel seams at boundaries
- Con: two-pass base+overlay needed for transparency to work correctly

**B. Square quad per tile**
- Pro: squares pack perfectly, zero seams at square boundaries
- Pro: trivial to implement — just change 6 verts to 4
- Pro: world-space UV + GL_REPEAT works cleanly, no geometry clipping artifacts
- Con: visual square bleeds outside the hex logical boundary (quad corners visible)
- Con: overlapping adjacent quads require sorted draw order or alpha tricks
- Con: click/hover detection decoupled from visual shape

**C. Single large quad (whole map)** *(what Civ and most 2D strategy games do)*
- Pro: one draw call for all terrain, zero seams by construction
- Pro: any tileable texture works, no per-tile UV concerns
- Pro: simplest shader — just `texture(u_Terrain, worldXZ * scale)`
- Con: terrain type changes require a second data source (tilemap texture or separate pass per type)
- Con: hex boundary transitions not handled automatically
- Con: requires a separate grid overlay for hex readability

**D. Unified hex mesh (one VBO, all tiles baked in)**
- Pro: one draw call, exact hex geometry, no seams
- Pro: vertex blend weights enable smooth terrain type transitions (Civ splatting)
- Con: mesh must be rebuilt whenever the map changes (editor becomes harder)
- Con: significant architecture change — replaces the whole per-tile loop
- Con: most complex to implement

---

### Decision 2 — UV space: where do texture coordinates come from?

```
                     ┌─ A. Model-space (normalised to tile bounds, 0→1)
UV Space ────────────┤
                     └─ B. World-space (worldXZ * scale, GL_REPEAT)
```

**A. Model-space** *(current)*
- Pro: texture is always aligned to each tile, rotation/variant works
- Pro: hex-shaped PNGs from PixelLab work as-is
- Con: every tile shows the same crop — stamped appearance
- Con: seam visible at every tile boundary (texture snaps)
- ✗ Incompatible with geometry C or D (whole-map approaches need world-space)

**B. World-space**
- Pro: texture tiles continuously — no repeat per-tile
- Pro: adjacent tiles of same type look seamless
- Pro: texture scale tunable without regenerating assets
- Con: requires full-bleed tileable square PNGs (hex-shaped PNGs break badly)
- Con: tile rotation has no effect (world doesn't rotate)
- ✓ Required for geometry C or D

---

### Decision 3 — Texture shape: what do the terrain PNG files look like?

```
                     ┌─ A. Hex-shaped (art in hex, transparent/filled corners)
Texture Shape ───────┤
                     └─ B. Full-bleed tileable square (entire image is texture)
```

**A. Hex-shaped PNGs** *(current, from `create_tiles_pro tile_type=hex`)*
- Pro: PixelLab generates them directly, no post-processing
- Pro: corners can be base colour so model-space UV looks clean
- Con: transparent/masked corners cause holes when tiled in world-space
- ✗ Incompatible with UV-B (world-space)

**B. Full-bleed tileable square PNGs** *(from `create_tiles_pro tile_type=square_topdown`)*
- Pro: works cleanly with world-space UV and GL_REPEAT
- Pro: simpler to generate — no hex shape logic needed
- Pro: standard texture format — any texture source works
- Con: PixelLab doesn't guarantee seamless edges (may need manual fix or different tool)
- Con: all existing terrain textures need to be regenerated
- ✓ Required for UV-B

---

### Decision 4 — Terrain transitions: what happens at the boundary between sand and dune?

```
                     ┌─ A. Hard cut (just switch textures at the hex edge)
                     ├─ B. Soft edge fade (current — alpha feather at hex rim)
Transitions ─────────┤
                     ├─ C. Wang autotile (pre-baked transition PNGs per terrain pair)
                     ├─ D. Vertex blend weights (shader mixes two terrain textures)
                     └─ E. Grid overlay masks the cut (draw visible hex lines so the
                           hard cut reads as intentional boundary, not artifact)
```

**A. Hard cut**
- Pro: zero implementation cost
- Pro: with a strong art style, sharp terrain boundaries read as intentional regions
- Con: terrain boundary is a visible straight line — looks unnatural up close
- Con: especially bad with photorealistic textures; acceptable with flat pixel art

**B. Soft edge fade** *(current softEdge pass)*
- Pro: already implemented, works for model-space UV
- Con: creates visible hex outline (the problem we've been fighting)
- Con: fades TO the base colour, so you always see the hex grid
- ✗ Incompatible with UV-B world-space (causes hex-stamp artifacts)

**C. Wang autotile**
- Pro: HoMM3 uses exactly this — pre-baked directional edge tiles per terrain pair
- Pro: already partially implemented (`loadEdgeTiles`, `grassSandEdgeTex`)
- Pro: looks great, very readable terrain boundaries
- Con: N terrain types = N² transition tile sets needed
- Con: each set needs 6 directional variants (for flat-top hexes) = expensive art asset count

**D. Vertex blend weights**
- Pro: smooth organic transitions, no tile-count explosion
- Pro: handles arbitrary multi-terrain corners
- Con: requires geometry D (unified mesh) — can't do per-tile
- Con: most complex shader; vertex data must be pre-computed per edge

**E. Grid overlay masks the cut**
- Pro: works with any approach — just draw hex lines on top
- Pro: the hex grid is part of the game's visual language anyway (HoMM3 shows it)
- Pro: completely free — hard cut A + grid overlay E = perfectly readable terrain
- Con: if the design goal is an invisible grid, this is a non-answer

---

### Decision 5 — Grid visibility: when is the hex grid shown?

```
                     ┌─ A. Always visible (HoMM3 style)
Grid Visibility ─────┼─ B. Never (terrain-only, Civ style)
                     └─ C. On hover/select only
```

**A. Always visible**
- Pro: hex grid is part of the game's visual identity in this genre
- Pro: makes terrain type transitions legible even with hard cuts
- Pro: no extra per-frame cost if drawn as a line overlay once
- Con: busier screen, especially at low zoom

**B. Never**
- Pro: clean painterly terrain look
- Con: requires smooth terrain transitions (C or D above) to compensate
- Con: hard to read hex boundaries for unit movement planning

**C. On hover/select only** *(Civ, Age of Wonders)*
- Pro: clean at a glance, informative on interaction
- Pro: best of both worlds if transitions are smooth
- Con: medium implementation cost (needs per-hex hover state passed to renderer)

---

### Decision 6 — Pass structure: how many draw calls per frame for terrain?

```
                     ┌─ A. Two-pass: base colour + texture overlay (current)
Pass Structure ──────┤
                     └─ B. Single-pass: texture IS the colour
```

**A. Two-pass** *(current)*
- Pro: base colour fallback when no texture is loaded
- Con: base colour bleeds through at hex edges whenever texture fades
- Con: double the draw calls; causes the "space between tiles" problem

**B. Single-pass**
- Pro: no bleed, no overlay seam
- Pro: half the draw calls for terrain
- Con: need a solid fallback for missing textures (just use a solid colour in the shader)
- ✓ Required for geometry C (single large quad)

---

### Viable Combinations

| Name | Geometry | UV | Texture | Transitions | Grid | Passes | Cost |
|------|----------|----|---------|-------------|------|--------|------|
| **Current** | A hex/tile | A model | A hex-shaped | B soft fade | A always | A two | — |
| **Quick fix** | A hex/tile | B world | B full-bleed | A hard cut | A always | B single | Low |
| **HoMM3-like** | A hex/tile | B world | B full-bleed | C Wang | A always | B single | Medium |
| **Civ-lite** | C map quad | B world | B full-bleed | A hard cut + E grid | C on demand | B single | Medium |
| **Civ-proper** | D unified | B world | B full-bleed | D vertex blend | C on demand | B single | High |

**Recommended path:**
- Now → **Quick fix**: geometry A, UV B, texture B, transitions A+E, grid A, single-pass.
  Seamless within terrain type. Hard cut at boundaries hidden by always-on grid.
  Generate full-bleed tileable PNGs for each terrain type. ~1 day work.
- Phase 3 → **HoMM3-like**: add Wang autotile edge tiles per terrain pair.
  Already partially implemented. Upgrade transition quality without touching geometry.

---

## The Terrain Texture Problem

### Why the cookie cutter approach failed

`scripts/hex_tile_cutter.py` is a GIMP plugin that overlays a hex grid on a
source image, crops each cell, and burns a hex-shaped alpha mask into it. It was
abandoned for several reasons:

- **Stamped appearance.** Every tile of the same type gets the same crop from the
  same position in the source image. The terrain looks copy-pasted.
- **GIMP dependency.** Fragile install, interactive-only, not scriptable from
  the build.
- **Seam at the alpha edge.** Hard transparency cuts leave a visible fringe where
  the hex mask was applied, especially noticeable on dunes and sand.
- **Wrong tool for the job.** Pre-cutting a hex shape into the PNG means the
  renderer is sampling a pre-clipped image. The GPU never gets to tile the texture
  naturally — the clipping was done offline at the wrong stage.

### The correct approach: world-space UV tiling

The hex mesh already clips the texture to the hex shape for free — the triangles
don't extend beyond the hex boundary. You don't need a pre-masked PNG at all.

Instead of model-space UVs (current: `u = 0.5 + 0.5*cos(angle)`, stretches the
image once across the hex), derive UVs from **world XZ position** and load the
texture with `GL_REPEAT`. The texture tiles continuously in world space; the hex
mesh just cuts a hex-shaped window into it. Adjacent tiles of the same terrain
type look seamless.

**Shader change required (hex.vert):**

Pass world position through and scale it to control tile frequency:

```glsl
// In hex.vert — instead of v_TexCoord = a_TexCoord:
v_TexCoord = v_WorldPos.xz * u_TexScale;   // u_TexScale ~ 0.5–1.0
```

**Texture loading change (Texture.cpp / loadTerrainTextures):**

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
```

**What this gives you:**
- No pre-processing of any PNG needed
- Any seamless tileable image works as input
- Texture scale is a shader uniform — tweak without re-generating assets
- Tiles of the same terrain type are seamless across boundaries
- The hex clipping is a geometric fact, not baked into the PNG

**What source textures to generate:**

For world-space tiling you want a **seamless (tileable) square PNG**, not a
hex-shaped image. Use `create_tiles_pro` with `tile_type=square_topdown` to
generate tileable terrain textures, or use the existing hex-shaped PNGs as-is
(they still look fine; the UVs just repeat the hex-masked image across the tile,
which is acceptable for pixel art).

The cleaner long-term path is tileable squares — but that is a Phase 3 texture
re-bake. **For now, the existing hex PNGs with model-space UVs are good enough.**

---

## The Hex Tile Pipeline (current, works now)

`create_tiles_pro` with `tile_type=hex` (flat-top) and `tile_size=128` is the
confirmed working path for terrain. Key parameters:

```
tile_type    = "hex"           # flat-top hexagon shape
tile_size    = 128             # matches our tile resolution
outline_mode = "segmentation"  # cleaner edges, less outline artifact
```

**Numbering tip:** Prompts like `"1). cracked desert sand 2). windswept dune"` give
better per-tile control than free-form descriptions.

**Style anchoring:** When generating a batch that should match existing tiles,
pass a completed tile as `style_images`. This is the main lever for visual
consistency without manual editing.

---

## The Map Object Pipeline

`create_map_object` generates a top-down sprite on transparent background.
Typical prompt pattern: `"sawmill, rough-cut timber, desert adobe, top-down view"`.
Size: 128×128. Run through `remove_bg.py` if the background isn't clean.

---

## What "Well Enough" Means Here

**Legible:** The player can tell sand from mountain, dungeon from town, warrior from
scout. That bar is already cleared.

**Consistent enough:** Tiles generated in the same session with the same style params
will match reasonably well. Tiles generated across different sessions or different
prompts won't. This is acceptable for a pre-Phase-3 build — the whole render
pipeline gets audited in Phase 3 anyway (GL_NEAREST, FBO integer scaling,
SpriteBatcher).

**Not worth perfecting now:**
- Hex edge blending (Wang autotile via `create_topdown_tileset`) — Phase 3 item
- Hit-flash / death animations — Phase 3 item  
- HUD pixel-art frame — Phase 3 item
- Full 6-faction unit rosters — Phase 3 item

---

## Immediate Asset Gaps (pre-Phase-3 playability)

These are the only gaps that currently block content on the default map:

| Asset | Tool | Priority |
|-------|------|----------|
| sawmill (object, 128×128) | `create_map_object` | High — needed for economy map |
| quarry (object, 128×128) | `create_map_object` | High |
| obsidian_vent (object, 128×128) | `create_map_object` | High |
| Ushari SC portrait (128×128) | `create_character` | Medium — she has a unit sprite; portrait is for UI |
| Sekhara SC portrait (128×128) | `create_character` | Medium |

Everything else in the asset queue (ROADMAP.md §Asset Queue) is Phase 3 work.

---

## Style Reference

**Visual target:** "Diablo II Act II meets LucasArts." Desert/Egyptian. Dark outlines,
muted earthy palette, no anti-aliasing, no gradients. The mood is dry, ancient,
slightly ominous — not bright or cartoony.

**Palette constraints:** ~24 colours per sprite. 1px dark outline. Transparent
background on all units and objects. Hex terrain tiles fill their full canvas
(no transparency needed — the hex mask is applied in the renderer).

**Prompting anchors that work:**
- `"pixel art, top-down view, desert egypt, dark outline, muted palette"`
- `"HoMM3 style, 16-bit era, no anti-aliasing"`
- Avoid: `"fantasy"`, `"vibrant"`, `"detailed shading"` — these push toward
  over-rendered non-pixel-art output.

---

## Phase 3 Checklist (do not start until Milestone 4 is done)

1. `glTexParameteri` audit — ensure `GL_NEAREST` on every texture
2. Offscreen FBO + integer-scale blit pass
3. SpriteBatcher (one draw call per texture atlas)
4. **World-space UV tiling** — switch `hex.vert` to `v_WorldPos.xz * u_TexScale`;
   add `u_TexScale` uniform; load terrain textures with `GL_REPEAT`;
   re-generate terrain textures as seamless square PNGs
5. Hex edge blending shader (sand↔dune soft transitions)
6. Full 6-faction unit rosters via `create_character` + `animate_character`
7. HUD pixel-art frame, minimap, portrait slots
8. Combat hit-flash + death animation clips
