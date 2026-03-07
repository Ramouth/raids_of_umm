# Test Plan — Raids of Umm'Natur

## Automated tests (logic, no SDL/GL)

Build and run with:
```bash
LOCAL_CMAKE="$(pwd)/local_deps/usr/bin/cmake"
LOCAL_LIBS="$(pwd)/local_deps/usr/lib/x86_64-linux-gnu"
LOCAL_INC="$(pwd)/local_deps/usr/include"

LD_LIBRARY_PATH="$LOCAL_LIBS" "$LOCAL_CMAKE" \
    -S . -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_ROOT="$(pwd)/local_deps/usr/share/cmake-3.28" \
    -DCMAKE_INCLUDE_PATH="$LOCAL_INC"

make -C build raids_tests -j$(nproc)
./build/raids_tests
```

Expected output: `PASSED: N / N` with no FAIL lines.

### Suites currently active

| File | Suite | What it verifies |
|---|---|---|
| test_hex.cpp | HexCoord — cube invariant | s() == -(q+r) always |
| test_hex.cpp | HexCoord — arithmetic | +, -, * operators |
| test_hex.cpp | HexCoord — equality | == and != |
| test_hex.cpp | HexCoord — distance | adjacent=1, known distances |
| test_hex.cpp | HexCoord — neighbor | 6 unique neighbors, wraps at 6 |
| test_hex.cpp | HexCoord — world round-trip | toWorld+fromWorld = identity |
| test_hex.cpp | HexCoord — hash uniqueness | 121 coords = 121 buckets |
| test_hex.cpp | HexGrid — set/get/has/remove | basic CRUD |
| test_hex.cpp | HexGrid — cell count | increments/decrements correctly |
| test_hex.cpp | HexGrid — neighbors (unfiltered) | count == present neighbor tiles |
| test_hex.cpp | HexGrid — neighbors (filtered) | passFunc removes impassable |
| test_hex.cpp | HexGrid — ring | radius 0→1→2→3 tile counts |
| test_hex.cpp | HexGrid — range | cumulative area counts |
| test_hex.cpp | HexGrid::findPath — start==goal | path length 1 |
| test_hex.cpp | HexGrid::findPath — adjacent | path length 2 |
| test_hex.cpp | HexGrid::findPath — known distance | straight path is shortest |
| test_hex.cpp | HexGrid::findPath — obstacle | path avoids blocked tile |
| test_hex.cpp | HexGrid::findPath — no path | returns empty |
| test_hex.cpp | HexGrid::findPath — goal out of grid | returns empty |
| test_resources.cpp | Resource — enum values | Gold=0 … Amber=6, COUNT=7 |
| test_resources.cpp | Resource — resourceName | all 7 names non-empty, 99→"Unknown" |
| test_resources.cpp | ResourcePool — default | all zeros |
| test_resources.cpp | ResourcePool — operator[] | read/write per resource |
| test_resources.cpp | ResourcePool — operator+ | element-wise add, originals unchanged |
| test_resources.cpp | ResourcePool — operator- | element-wise subtract |
| test_resources.cpp | ResourcePool — operator+= | mutates in place |
| test_resources.cpp | ResourcePool — operator-= | mutates in place |
| test_resources.cpp | ResourcePool — canAfford (sufficient) | returns true |
| test_resources.cpp | ResourcePool — canAfford (exact) | returns true |
| test_resources.cpp | ResourcePool — canAfford (one short) | returns false |
| test_resources.cpp | ResourcePool — canAfford (no gold) | returns false |
| test_resources.cpp | ResourcePool — canAfford (zero cost) | always true |
| test_resources.cpp | ResourcePool — clampPositive | negatives become 0 |

### Suites waiting for their module

Activate by adding the define to `CMakeLists.txt` (test target compile options).

| Define | File | Module needed |
|---|---|---|
| `WORLDMAP_IMPL` | test_worldmap.cpp | `src/world/WorldMap.h/cpp` |
| `CAMERA2D_IMPL` | test_camera.cpp | `src/render/Camera2D.h/cpp` |

To activate once WorldMap is implemented, add to CMakeLists.txt raids_tests target:
```cmake
target_compile_definitions(raids_tests PRIVATE WORLDMAP_IMPL)
```
And add `src/world/WorldMap.cpp` to the raids_tests source list.

---

## Manual / visual tests (require SDL window + OpenGL)

Run with: `./build/raids_of_umm`

These tests are checked by eye and console output. Each has a clear pass criterion.

---

### M-01 — Window opens correctly
**Steps:** Launch the binary.
**Pass:** 1280×720 window appears. Title bar reads "Raids of Umm'Natur".
No crash. Console prints OpenGL renderer and version.

---

### M-02 — Hex grid renders
**Steps:** Launch. Look at the map.
**Pass:**
- Hex tiles fill the visible area.
- At least 3 distinct terrain colours are visible (sand, dune, oasis, rock, obsidian).
- No Z-fighting / flickering.
- Tile edges are cleanly separated (no gaps wider than 1px).

---

### M-03 — Camera pan
**Steps:** Hold W / A / S / D or arrow keys.
**Pass:** Map scrolls smoothly in the expected direction.
Releasing a key stops movement immediately.

---

### M-04 — Camera zoom
**Steps:** Scroll mouse wheel up and down.
**Pass:**
- Scroll up → map grows (zooms in). Tiles get larger.
- Scroll down → map shrinks (zooms out). More tiles visible.
- Zoom has a minimum (can't zoom in past the point of seeing ~3 tiles).
- Zoom has a maximum (can't zoom out past seeing the whole map as a dot).

---

### M-05 — Hover highlight
**Steps:** Move mouse over different tiles.
**Pass:** A yellow outline follows the mouse cursor tile-by-tile with no lag.
Moving outside the map area: outline disappears.

---

### M-06 — Map objects visible
**Steps:** Pan around the map.
**Pass:** At least 6 coloured tokens are visible above terrain tiles:
- Cyan token = Town
- Red token = Dungeon
- Gold token = Gold Mine
- Light blue token = Crystal Mine
- White tokens = Artifacts

---

### M-07 — Hero sprite visible
**Steps:** Launch.
**Pass:** Animated rider sprite is visible at the map centre (0,0).
Sprite is billboard (always faces camera). Animation cycles (at least 2 frames).

---

### M-08 — Hero selection
**Steps:** Left-click the hero sprite.
**Pass:** A white ring appears around the hero. Console prints "Hero selected".
Clicking the hero again: ring disappears. Console prints "Hero deselected".

---

### M-09 — Path preview
**Steps:** Select hero. Move mouse to a distant tile.
**Pass:**
- Blue outlines trace the A* path from hero to hovered tile.
- Tiles within move range: blue outline.
- Tiles beyond move range: red outline.
- Path avoids Obsidian (impassable) tiles.

---

### M-10 — Hero movement
**Steps:** Select hero. Left-click a reachable tile.
**Pass:**
- Hero sprite glides smoothly to the destination.
- Camera follows the sprite during movement.
- Console prints destination coords and remaining moves.
- Move counter decrements correctly.
- Clicking an out-of-range tile: console prints "Not enough movement".

---

### M-11 — Object visit
**Steps:** Move hero to a tile with a coloured token.
**Pass:** Token dims (visited). Console prints the object type and name.
Gold Mine: prints "+500 Gold per turn secured!".

---

### M-12 — End turn
**Steps:** Press SPACE.
**Pass:** Console prints "Day 2 begins. Moves restored."
Hero move counter resets to maximum. Hero deselects.
Pressing SPACE again: day increments to 3, etc.

---

### M-13 — Window resize
**Steps:** Drag the window to a different size.
**Pass:** Map redraws to fill the new viewport. Aspect ratio is correct (hex tiles remain regular hexagons, not stretched).

---

### M-14 — Clean exit
**Steps:** Press ESC, or close the window.
**Pass:** Process exits with code 0. No crash. No console error output.

---

## Adding a new visual test

When a new visual feature is implemented, add a row here following the `M-NN` format:
- **Steps:** minimal reproducible actions
- **Pass:** observable, unambiguous criterion (not "looks good")
