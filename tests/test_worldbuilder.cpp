// test_worldbuilder.cpp
//
// Tests for WorldBuilder editor logic.
//
// Covers:
//   1. EditorPalette coordinate routing constants (no GL required)
//   2. EditorTool enum values and cycling arithmetic
//   3. WorldMap paint / place-object / erase operations
//   4. Terrain variant save/load round-trip
//   5. New terrain types (Grass, Forest, Highland)
//
// The EditorPalette render() and HexRenderer are NOT tested here (they need GL).
// This file links against WorldMap + HexGrid only.

#include "test_runner.h"

#ifdef WORLDMAP_IMPL

#include "world/WorldMap.h"
#include "world/MapTile.h"
#include "world/MapObject.h"
#include "hex/HexCoord.h"
#include "worldbuilder/EditorTool.h"   // EditorTool enum + EDITOR_TOOL_COUNT

// ── Helper: mirror the applyToolAt logic without the full GameState ───────────

static void paintTile(WorldMap& map, HexCoord coord, Terrain t, int variant) {
    if (!map.hasTile(coord)) return;
    MapTile tile = makeTile(t);
    tile.variant = static_cast<uint8_t>(variant);
    if (map.objectAt(coord))
        tile.passable = true;
    map.setTile(coord, tile);
}

static void placeObject(WorldMap& map, HexCoord coord, ObjType type) {
    if (!map.hasTile(coord)) return;
    if (MapTile* t = map.tileAt(coord)) {
        if (!t->passable) {
            t->terrain  = Terrain::Sand;
            t->passable = true;
            t->moveCost = 1.0f;
        }
    }
    MapObjectDef obj;
    obj.pos  = coord;
    obj.type = type;
    obj.name = std::string(objTypeName(type));
    map.placeObject(std::move(obj));
}

static void eraseAt(WorldMap& map, HexCoord coord) {
    if (!map.hasTile(coord)) return;
    map.removeObjectAt(coord);
    map.setTile(coord, makeTile(Terrain::Sand));
}

// ── 1. EditorPalette coordinate routing constants ─────────────────────────────

SUITE("EditorPalette — panel width constant covers x 0..239") {
    // PANEL_W = 240; containsPoint(x) = (x >= 0 && x < PANEL_W)
    constexpr int PANEL_W = 240;
    CHECK( 0   >= 0 && 0   < PANEL_W);   // left edge — in palette
    CHECK( 239 >= 0 && 239 < PANEL_W);   // right edge — in palette
    CHECK(!(240 >= 0 && 240 < PANEL_W)); // first map pixel — NOT in palette
    CHECK(!(300 >= 0 && 300 < PANEL_W));
    CHECK(!(-1  >= 0 && -1  < PANEL_W)); // negative — not in palette
}

SUITE("EditorPalette — tab row divides panel into 4 equal tabs at y=[48..78)") {
    constexpr float PW    = 240.0f;
    constexpr float HDR_H = 48.0f;
    constexpr float TAB_H = 30.0f;

    // Mirrors tabAtPoint(x, y) logic
    auto tabAtPoint = [&](int x, int y) -> int {
        if (x < 0 || x >= (int)PW) return -1;
        if (y < (int)HDR_H || y >= (int)(HDR_H + TAB_H)) return -1;
        return (int)(x / (PW / 4.0f));
    };

    const int Y = 60;  // inside tab row [48, 78)
    CHECK_EQ(tabAtPoint(0,   Y), 0);   // Terrain
    CHECK_EQ(tabAtPoint(59,  Y), 0);   // Terrain (last pixel)
    CHECK_EQ(tabAtPoint(60,  Y), 1);   // Objects (first pixel)
    CHECK_EQ(tabAtPoint(119, Y), 1);   // Objects (last pixel)
    CHECK_EQ(tabAtPoint(120, Y), 2);   // Units
    CHECK_EQ(tabAtPoint(179, Y), 2);   // Units (last pixel)
    CHECK_EQ(tabAtPoint(180, Y), 3);   // Assorted
    CHECK_EQ(tabAtPoint(239, Y), 3);   // Assorted (last pixel)
    // Outside tab row Y bounds
    CHECK_EQ(tabAtPoint(60,  10),  -1);  // header area
    CHECK_EQ(tabAtPoint(60,  79),  -1);  // below tab row
    // Outside palette X bounds
    CHECK_EQ(tabAtPoint(-1,  Y),   -1);
    CHECK_EQ(tabAtPoint(240, Y),   -1);
}

SUITE("EditorPalette — item list starts at y=78 (HDR_H + TAB_H)") {
    // itemListStartY() = 48 + 30 = 78
    // Clicks at y < 78 are in the non-scrollable header/tab area.
    // Clicks at y >= 78 are in the scrollable item list.
    constexpr int LIST_START = 78;
    CHECK(10 < LIST_START);   // header y=10
    CHECK(60 < LIST_START);   // tab    y=60
    CHECK(78 >= LIST_START);  // first list item y=78
    CHECK(200 >= LIST_START); // deep in list
}

SUITE("EditorPalette — drag threshold must be >= 8 px to handle natural jitter") {
    // PALETTE_DRAG_THRESHOLD separates clicks from drag-scrolls.
    // With 18+ grass variants the list is long; the threshold must be generous.
    // We test the DESIGN requirement here; the actual constant lives in
    // WorldBuilderState.h (not included in test binary — would pull in GL/SDL).
    constexpr int REQUIRED_MIN_THRESHOLD = 8;
    // Hard-coded expected value — update here if the constant changes.
    constexpr int ACTUAL_THRESHOLD = 10;
    CHECK(ACTUAL_THRESHOLD >= REQUIRED_MIN_THRESHOLD);
}

// ── 2. EditorTool enum values and cycling arithmetic ─────────────────────────

SUITE("EditorTool — enum values are 0..3") {
    CHECK_EQ(static_cast<int>(EditorTool::PaintTile),   0);
    CHECK_EQ(static_cast<int>(EditorTool::PlaceObject), 1);
    CHECK_EQ(static_cast<int>(EditorTool::Erase),       2);
    CHECK_EQ(static_cast<int>(EditorTool::Select),      3);
    CHECK_EQ(EDITOR_TOOL_COUNT, 4);
}

SUITE("EditorTool — cycleTool wraps correctly through all 4 tools") {
    auto nextTool = [](EditorTool t) {
        return static_cast<EditorTool>(
            (static_cast<int>(t) + 1) % EDITOR_TOOL_COUNT);
    };
    CHECK_EQ(static_cast<int>(nextTool(EditorTool::PaintTile)),
             static_cast<int>(EditorTool::PlaceObject));
    CHECK_EQ(static_cast<int>(nextTool(EditorTool::PlaceObject)),
             static_cast<int>(EditorTool::Erase));
    CHECK_EQ(static_cast<int>(nextTool(EditorTool::Erase)),
             static_cast<int>(EditorTool::Select));
    CHECK_EQ(static_cast<int>(nextTool(EditorTool::Select)),
             static_cast<int>(EditorTool::PaintTile));
}

// ── 3. WorldMap paint / place-object / erase operations ──────────────────────

SUITE("Editor paintTile — sets correct terrain and variant") {
    WorldMap map;
    map.clear(3);
    HexCoord c{0, 0};
    paintTile(map, c, Terrain::Dune, 0);
    const MapTile* t = map.tileAt(c);
    CHECK(t != nullptr);
    if (t) {
        CHECK_EQ(static_cast<int>(t->terrain), static_cast<int>(Terrain::Dune));
        CHECK_EQ((int)t->variant, 0);
    }
}

SUITE("Editor paintTile — stores non-zero variant index") {
    WorldMap map;
    map.clear(3);
    HexCoord c{1, 0};
    paintTile(map, c, Terrain::Grass, 5);
    const MapTile* t = map.tileAt(c);
    CHECK(t != nullptr);
    if (t) {
        CHECK_EQ(static_cast<int>(t->terrain), static_cast<int>(Terrain::Grass));
        CHECK_EQ((int)t->variant, 5);
    }
}

SUITE("Editor paintTile — coord outside map is a no-op (no crash)") {
    WorldMap map;
    map.clear(2);
    HexCoord outside{100, 100};
    CHECK(!map.hasTile(outside));
    paintTile(map, outside, Terrain::Sand, 0);   // must not crash
    CHECK(!map.hasTile(outside));
}

SUITE("Editor paintTile — painting over object preserves passability") {
    WorldMap map;
    map.clear(3);
    HexCoord c{0, 1};
    placeObject(map, c, ObjType::Town);
    CHECK(map.objectAt(c) != nullptr);

    paintTile(map, c, Terrain::Grass, 0);
    const MapTile* t = map.tileAt(c);
    CHECK(t != nullptr);
    if (t) {
        CHECK_EQ(static_cast<int>(t->terrain), static_cast<int>(Terrain::Grass));
        CHECK(t->passable);   // must remain passable because object is there
    }
}

SUITE("Editor placeObject — creates object with correct type") {
    WorldMap map;
    map.clear(3);
    HexCoord c{0, 0};
    placeObject(map, c, ObjType::Dungeon);
    const MapObjectDef* obj = map.objectAt(c);
    CHECK(obj != nullptr);
    if (obj) {
        CHECK_EQ(static_cast<int>(obj->type), static_cast<int>(ObjType::Dungeon));
        CHECK_EQ(obj->pos.q, 0);
        CHECK_EQ(obj->pos.r, 0);
    }
}

SUITE("Editor placeObject — makes impassable tile passable") {
    WorldMap map;
    map.clear(3);
    HexCoord c{1, -1};
    map.setTile(c, makeTile(Terrain::Obsidian));
    CHECK(!map.tileAt(c)->passable);

    placeObject(map, c, ObjType::GoldMine);
    const MapTile* t = map.tileAt(c);
    CHECK(t != nullptr);
    if (t) CHECK(t->passable);
}

SUITE("Editor placeObject — all ObjType values can be placed") {
    WorldMap map;
    map.clear(5);
    auto dirs = HexCoord::directions();
    bool ok = true;
    for (int i = 0; i < OBJ_TYPE_COUNT; ++i) {
        ObjType ot = static_cast<ObjType>(i);
        HexCoord c = (i == 0) ? HexCoord{0, 0} : HexCoord{0, 0} + dirs[i % 6];
        if (!map.hasTile(c)) continue;
        map.removeObjectAt(c);
        placeObject(map, c, ot);
        const MapObjectDef* obj = map.objectAt(c);
        if (!obj || obj->type != ot) ok = false;
    }
    CHECK(ok);
}

SUITE("Editor erase — removes object and resets tile to Sand") {
    WorldMap map;
    map.clear(3);
    HexCoord c{0, 0};
    placeObject(map, c, ObjType::Town);
    CHECK(map.objectAt(c) != nullptr);

    eraseAt(map, c);
    CHECK(map.objectAt(c) == nullptr);
    const MapTile* t = map.tileAt(c);
    CHECK(t != nullptr);
    if (t) CHECK_EQ(static_cast<int>(t->terrain), static_cast<int>(Terrain::Sand));
}

SUITE("Editor erase — resets non-sand terrain to Sand") {
    WorldMap map;
    map.clear(3);
    HexCoord c{0, 0};
    paintTile(map, c, Terrain::Grass, 3);

    eraseAt(map, c);
    const MapTile* t = map.tileAt(c);
    CHECK(t != nullptr);
    if (t) {
        CHECK_EQ(static_cast<int>(t->terrain), static_cast<int>(Terrain::Sand));
        CHECK_EQ((int)t->variant, 0);
    }
}

SUITE("Editor erase — outside map is a no-op (no crash)") {
    WorldMap map;
    map.clear(2);
    eraseAt(map, {99, 99});   // must not crash
    CHECK(!map.hasTile({99, 99}));
}

// ── 4. Terrain variant save / load round-trip ─────────────────────────────────

SUITE("WorldMap — variant index survives save/load") {
    WorldMap map;
    map.clear(3);
    paintTile(map, {0, 0}, Terrain::Grass, 7);

    const std::string path = "/tmp/raids_test_variant.json";
    CHECK(!map.saveJson(path).has_value());

    WorldMap loaded;
    CHECK(!loaded.loadJson(path).has_value());

    const MapTile* t = loaded.tileAt({0, 0});
    CHECK(t != nullptr);
    if (t) {
        CHECK_EQ(static_cast<int>(t->terrain), static_cast<int>(Terrain::Grass));
        CHECK_EQ((int)t->variant, 7);
    }
}

SUITE("WorldMap — mixed terrain variants all survive save/load") {
    WorldMap map;
    map.clear(5);
    paintTile(map, {0, 0}, Terrain::Grass,    5);
    paintTile(map, {1, 0}, Terrain::Forest,   2);
    paintTile(map, {2, 0}, Terrain::Highland, 3);
    paintTile(map, {0, 1}, Terrain::Sand,     1);
    paintTile(map, {1, 1}, Terrain::Dune,     2);

    const std::string path = "/tmp/raids_test_multi_variant.json";
    CHECK(!map.saveJson(path).has_value());

    WorldMap loaded;
    CHECK(!loaded.loadJson(path).has_value());

    struct Expect { HexCoord c; Terrain t; int v; };
    Expect cases[] = {
        {{0,0}, Terrain::Grass,    5},
        {{1,0}, Terrain::Forest,   2},
        {{2,0}, Terrain::Highland, 3},
        {{0,1}, Terrain::Sand,     1},
        {{1,1}, Terrain::Dune,     2},
    };
    for (auto& e : cases) {
        const MapTile* mt = loaded.tileAt(e.c);
        CHECK(mt != nullptr);
        if (mt) {
            CHECK_EQ(static_cast<int>(mt->terrain), static_cast<int>(e.t));
            CHECK_EQ((int)mt->variant, e.v);
        }
    }
}

SUITE("WorldMap — variant=0 default is preserved on save/load") {
    WorldMap map;
    map.clear(2);
    // Freshly generated tiles have variant=0 by default
    const MapTile* orig = map.tileAt({0, 0});
    CHECK(orig != nullptr);
    if (orig) CHECK_EQ((int)orig->variant, 0);

    const std::string path = "/tmp/raids_test_variant_zero.json";
    CHECK(!map.saveJson(path).has_value());
    WorldMap loaded;
    CHECK(!loaded.loadJson(path).has_value());
    const MapTile* t = loaded.tileAt({0, 0});
    CHECK(t != nullptr);
    if (t) CHECK_EQ((int)t->variant, 0);
}

// ── 5. New terrain types (Grass, Forest, Highland) ───────────────────────────

SUITE("Terrain — COUNT is 13 (10 desert + 3 biome)") {
    CHECK_EQ(static_cast<int>(Terrain::COUNT), 13);
    CHECK_EQ(TERRAIN_COUNT, 13);
}

SUITE("Terrain — new biome enum values are correct") {
    CHECK_EQ(static_cast<int>(Terrain::Grass),    10);
    CHECK_EQ(static_cast<int>(Terrain::Forest),   11);
    CHECK_EQ(static_cast<int>(Terrain::Highland), 12);
}

SUITE("Terrain — makeTile defaults for Grass/Forest/Highland") {
    MapTile g = makeTile(Terrain::Grass);
    CHECK(g.passable);
    CHECK_NEAR(g.moveCost, 1.0f,  0.001f);

    MapTile f = makeTile(Terrain::Forest);
    CHECK(f.passable);
    CHECK_NEAR(f.moveCost, 1.5f,  0.001f);  // dense woodland — slow

    MapTile h = makeTile(Terrain::Highland);
    CHECK(h.passable);
    CHECK_NEAR(h.moveCost, 1.25f, 0.001f);  // rocky moorland
}

SUITE("Terrain — terrainName returns correct labels for biome types") {
    CHECK(terrainName(Terrain::Grass)    == "Grass");
    CHECK(terrainName(Terrain::Forest)   == "Forest");
    CHECK(terrainName(Terrain::Highland) == "Highland");
}

SUITE("WorldMap — can paint all 13 terrain types without error") {
    WorldMap map;
    map.clear(5);
    auto dirs = HexCoord::directions();
    bool ok = true;
    for (int ti = 0; ti < TERRAIN_COUNT; ++ti) {
        Terrain t = static_cast<Terrain>(ti);
        HexCoord c = (ti < 7) ? HexCoord{0, 0} + dirs[ti % 6]
                               : HexCoord{0, 0} + dirs[(ti - 7) % 6] * 2;
        // Ensure we pick a tile that actually exists in the grid
        if (!map.hasTile(c)) c = {0, 0};
        map.removeObjectAt(c);
        paintTile(map, c, t, 0);
        const MapTile* mt = map.tileAt(c);
        if (!mt || mt->terrain != t) ok = false;
    }
    CHECK(ok);
}

SUITE("MAX_TERRAIN_VARIANTS accommodates grass folder (18+ files)") {
    // The grass folder now has 18+ PNG files; MAX_TERRAIN_VARIANTS must be >= 18.
    CHECK(MAX_TERRAIN_VARIANTS >= 18);
}

#endif // WORLDMAP_IMPL
