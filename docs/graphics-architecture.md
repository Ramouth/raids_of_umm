# Graphics Architecture — Raids of Umm'Natur

## Overview

The renderer is deliberately simple: OpenGL 3.3 Core Profile, no engine, no scene
graph. Each visual layer is handled by a dedicated C++ class with its own shader pair.
There are three independent layers drawn every frame, in order:

```
1. HexRenderer    — terrain tiles, hex grid, outlines          (hex.vert / hex.frag)
2. SpriteRenderer — world-space billboards (units, objects)    (sprite.vert / sprite.frag)
3. HUDRenderer    — 2D screen-space UI                        (ui.vert / ui.frag)
```

Each layer is self-contained: its own VAO(s), its own shader, its own GL state
management. They do not share buffers or shader programs.

---

## Camera (`src/render/Camera2D.h/cpp`)

Despite the name, the camera produces a genuine 3D view: a perspective `lookAt` paired
with an orthographic projection. "2D" refers to the game-world plane navigated (XZ, Y=0).

```
eye    = (pos.x,  zoom × 1.8,  pos.z + zoom × 0.5)
target = (pos.x,  0,           pos.z)
up     = (0, 0, -1)                  // world -Z is "screen up"
```

For the default zoom of 8 this places the camera roughly 74° above horizontal —
steep enough to read the map clearly, shallow enough to give a strategy-game feel.

The projection is orthographic with half-extents derived from zoom:

```
hw = zoom × aspect,  hh = zoom
glm::ortho(-hw, hw, -hh, hh, 0.1, 200)
```

Orthographic is chosen over perspective so tile sizes stay consistent regardless of
where the camera looks, matching the visual language of HoMM3/Wesnoth.

**Picking** (`screenToWorld`) unprojects two NDC points to build a world-space ray,
then intersects it with the Y=0 plane. This maps any pixel click to a world XZ
coordinate without any hex-specific math.

---

## Layer 1 — Hex Terrain (`HexRenderer`)

### Geometry

All tiles share one VAO: a unit flat-top hexagon mesh (7 vertices, 12 triangles,
plus a separate 6-vertex line loop for outlines). Each tile is drawn individually
with a per-draw-call MVP matrix. This is intentionally simple — "hundreds of tiles"
is the expected scale, not hundreds of thousands.

Hex coordinates (`HexCoord`) are converted to world XZ using axial→Cartesian:

```
x = size × (3/2 × q)
z = size × (√3 × r  +  √3/2 × q)
```

### Shader (`hex.vert` / `hex.frag`)

Uniforms per draw call: `u_MVP`, `u_Model`, `u_NormalMatrix`, `u_TileColor`,
`u_Height` (Y extrusion so mountains stand taller).

Lighting model in `hex.frag`:

| Component | Formula |
|-----------|---------|
| Diffuse | Lambertian: `albedo × sunColor × max(N·L, 0)` |
| Ambient | Hemisphere: lerp between sky tint and ground bounce by `N.y` |
| Edge darken | `smoothstep(0.35, 0.5, edgeDist) × 0.5` — subtle vignette per tile |
| Fog | Exponential: `1 − e^(−density × dist)`, capped at 0.7 |

`u_TileColor` tints the texture sample so the terrain colour table
(`TileVisuals.h::terrainColor()`) drives the base hue without separate textures for
every variant.

### Textures

Terrain PNGs are loaded once at startup via `HexRenderer::loadTerrainTextures()`.
The filename is simply `lowercase(terrainName()) + ".png"`. Missing files fall back
to the white 1×1 placeholder texture, so the colour tint still appears correctly.

### `TileVisuals.h` — separation of concerns

All mappings from world enums (`Terrain`, `ObjType`) to rendering values (GLM
colour, height offset, visual scale) live in this single header. World-layer code
(`WorldMap`, `MapTile`) has zero dependency on GLM or any render type.

---

## Layer 2 — World-Space Sprites (`SpriteRenderer` / `AnimatedSprite`)

### Billboarding technique — cylindrical with elevation compensation

Sprites must appear upright on the ground regardless of camera angle. A naive
spherical billboard (full camera-facing) works but causes visual floating: the
`camUp` vector from the view matrix at 74° elevation is mostly horizontal
`(0, 0.27, −0.96)`, so the sprite tilts backward into the ground plane.

The solution is **cylindrical billboarding with elevation compensation**:

```glsl
// Horizontal extent — camera-aligned, projected onto XZ only (no tilt)
vec3 camRight = normalize(vec3(u_View[0][0], 0.0, u_View[2][0]));

// Vertical extent — world Y, scaled up to cancel foreshortening
vec3 worldUp    = vec3(0.0, 1.0, 0.0);
vec3 camUpWorld = vec3(u_View[0][1], u_View[1][1], u_View[2][1]); // view row 1
float cosEl     = max(dot(worldUp, camUpWorld), 0.15);
worldUp         = worldUp / cosEl;   // stretch so screen height = intended height
```

Without the compensation factor a sprite rendered at height `h` appears at
`h × cos(74°) ≈ 0.27h` on screen — nearly invisible. Dividing by `cosEl` restores
the intended screen size while keeping the sprite standing vertically in world space
(feet at Y=0, grounded correctly).

The clamp of 0.15 prevents extreme stretch if the camera ever approaches top-down
(90°).

### Animation — horizontal spritesheet

`SpriteRenderer` supports a horizontal sprite strip natively via two uniforms:

```glsl
uniform int u_Frame;      // 0-based current frame index
uniform int u_NumFrames;  // total columns in the strip
```

UV remapping: `u = (a_TexCoord.x + float(u_Frame)) / float(u_NumFrames)`.

Default values are `frame=0, numFrames=1`, so a static single-frame sprite works
without calling `setFrame()`.

### `AnimatedSprite` — clip-based wrapper

`AnimatedSprite` wraps `SpriteRenderer` and adds named animation clips:

```cpp
struct AnimClip { int firstFrame; int numFrames; float fps; bool loop; };
```

`update(dt)` advances the elapsed counter and advances the local frame index.
Non-looping clips stop on the last frame and set `isPlaying() = false`.

`draw()` converts `localFrame` to an absolute sheet frame
(`clip.firstFrame + localFrame`), calls `setFrame()`, then delegates to
`SpriteRenderer::draw()`.

**Backward compatibility**: all existing code that calls `SpriteRenderer::draw()`
directly still works unchanged — `setFrame()` only needs to be called when animation
is required.

### Spritesheet contract

```
horizontal strip — N frames, each 64×64 px
image dimensions: width = N × 64,  height = 64
```

Planned clips per unit:

| Clip | Frames | FPS | Loop |
|------|--------|-----|------|
| idle | 4 | 6 | yes |
| walk | 4 | 8 | yes |
| attack | 4 | 12 | no |

---

## Layer 3 — Screen-Space HUD (`HUDRenderer`)

The HUD is entirely 2D. It does **not** use the Camera2D matrices.

### Coordinate system

`ui.vert` converts pixel coordinates to NDC directly:

```glsl
vec2 ndc = (a_Position / u_ScreenSize) * 2.0 - 1.0;
ndc.y    = -ndc.y;   // pixel 0 = top, NDC +1 = top
```

All draw calls take pixel coordinates. `u_ScreenSize` is set once per frame by
`HUDRenderer::begin(screenW, screenH)`.

### Initialisation pattern

`begin()` must be called before any draw call each frame. It:
- Binds the UI shader
- Sets `u_ScreenSize`
- Disables depth test and face culling
- Enables alpha blending

This is critical on strict GL implementations (llvmpipe / Mesa software renderer)
where drawing without a bound shader is a silent no-op.

### Draw calls

| Method | VAO | Description |
|--------|-----|-------------|
| `drawRect` | `m_vao` | Flat coloured quad (`GL_TRIANGLE_FAN`, 2-float positions) |
| `drawTexturedRect` | `m_texVao` | Textured quad (position + UV), `u_UseTexture=1`, `u_Color=white` |
| `drawText` | `m_textVao` | `stb_easy_font` quads baked into a VBO each call |

### `ui.frag` branch

```glsl
if (u_UseTexture)
    FragColor = texture(u_Texture, v_TexCoord) * u_Color;
else
    FragColor = u_Color;
```

`u_Color` drives both the flat colour and the texture tint. The per-vertex `a_Color`
attribute (location 2) is declared in the shader but intentionally unused by the
C++ side — uniforms are more reliable across GL implementations.

---

## Shader inventory

| File | Used by | Purpose |
|------|---------|---------|
| `hex.vert` / `hex.frag` | `HexRenderer` | Hex tile geometry, Lambertian + hemisphere lighting, fog |
| `sprite.vert` / `sprite.frag` | `SpriteRenderer`, `AnimatedSprite` | Cylindrical billboard, spritesheet UV, alpha cutout |
| `ui.vert` / `ui.frag` | `HUDRenderer` | Screen-space 2D, flat + textured rects, text |
| `outline.vert` / `outline.frag` | `HexRenderer` | Wireframe hex outlines |
| `model.vert` / `model.frag` | (unused — reserved for future 3D models) | |

---

## Render order and GL state

Each frame in `AdventureState::render()`:

```
glClear(COLOR | DEPTH)

── Hex tiles ──────────────────────────────────────────────────────────────────
glEnable(GL_DEPTH_TEST)
hexRenderer.beginFrame(viewProj, ...)
  for each tile: hexRenderer.drawTile(...)
  for highlights: hexRenderer.drawOutline(...)
hexRenderer.endFrame()

── Sprites (world-space) ──────────────────────────────────────────────────────
glEnable(GL_BLEND)
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
spriteRenderer.draw(heroPos, ...)
enemySpriteRenderer.draw(enemyPos, ...)
glDisable(GL_BLEND)

── HUD (screen-space) ─────────────────────────────────────────────────────────
hud.begin(screenW, screenH)          // binds shader, disables depth test
hud.drawRect(...)                    // backgrounds, bars
hud.drawText(...)                    // stats, coordinates
```

Depth testing is left enabled for the sprite pass so sprites correctly occlude one
another by world Y=0 depth. The HUD pass disables depth test so UI always renders
on top regardless of fragment depth.

---

## Adding a new renderer

1. Create `src/render/MyRenderer.h/cpp` with its own `init()`, `draw()`.
2. Add the `.cpp` to `SOURCES` in `CMakeLists.txt`.
3. Write `assets/shaders/my.vert` and `my.frag`.
4. Call `begin()` / `draw()` from the appropriate `GameState::render()` at the
   correct layer position (terrain → sprites → HUD).
5. Do not share VAOs or shader state between renderers.
