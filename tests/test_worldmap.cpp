// test_worldmap.cpp
// Uncomment each SUITE block and add WorldMap.cpp to the test target
// once src/world/WorldMap.h/cpp has been implemented.
//
// Required interface assumed:
//   WorldMap::generateProcedural(int radius, unsigned seed)
//   WorldMap::tileAt(HexCoord) -> const MapTile*
//   WorldMap::objects()        -> const std::vector<MapObject>&
//   WorldMap::saveJson(path)   -> bool
//   WorldMap::loadJson(path)   -> bool
//   WorldMap::tileCount()      -> size_t
//   WorldMap::setTile(HexCoord, MapTile)
//   WorldMap::placeObject(MapObject)
//   WorldMap::removeObjectAt(HexCoord)

#include "test_runner.h"

#ifdef WORLDMAP_IMPL   // flip this define once WorldMap exists

#include "world/WorldMap.h"

SUITE("WorldMap — procedural generation tile count") {
    // A hex grid of radius R has 3R(R+1)+1 tiles
    WorldMap map;
    map.generateProcedural(12, /*seed=*/42);
    int expected = 3 * 12 * 13 + 1;  // = 469
    CHECK_EQ((int)map.tileCount(), expected);
}

SUITE("WorldMap — all generated tiles have valid terrain") {
    WorldMap map;
    map.generateProcedural(5, 1);
    bool allValid = true;
    for (int q = -5; q <= 5; ++q) {
        int r1 = std::max(-5, -q - 5);
        int r2 = std::min( 5, -q + 5);
        for (int r = r1; r <= r2; ++r) {
            const MapTile* t = map.tileAt({q, r});
            if (!t) { allValid = false; continue; }
            int tv = static_cast<int>(t->terrain);
            if (tv < 0 || tv >= static_cast<int>(Terrain::COUNT))
                allValid = false;
        }
    }
    CHECK(allValid);
}

SUITE("WorldMap — object tiles are passable") {
    WorldMap map;
    map.generateProcedural(12, 7);
    for (const auto& obj : map.objects()) {
        const MapTile* t = map.tileAt(obj.pos);
        CHECK(t != nullptr);
        if (t) CHECK(t->passable);
    }
}

SUITE("WorldMap — same seed produces identical maps") {
    WorldMap a, b;
    a.generateProcedural(5, 99);
    b.generateProcedural(5, 99);

    bool identical = true;
    for (int q = -5; q <= 5; ++q) {
        int r1 = std::max(-5, -q - 5);
        int r2 = std::min( 5, -q + 5);
        for (int r = r1; r <= r2; ++r) {
            const MapTile* ta = a.tileAt({q, r});
            const MapTile* tb = b.tileAt({q, r});
            if (!ta || !tb) { identical = false; continue; }
            if (ta->terrain != tb->terrain) identical = false;
        }
    }
    CHECK(identical);
}

SUITE("WorldMap — save and load round-trip") {
    const std::string path = "/tmp/raids_test_map.json";

    WorldMap original;
    original.generateProcedural(5, 123);

    // saveJson / loadJson return nullopt on success, a string on error.
    auto saveErr = original.saveJson(path);
    CHECK(!saveErr);   // nullopt = no error = success

    WorldMap loaded;
    auto loadErr = loaded.loadJson(path);
    CHECK(!loadErr);   // nullopt = no error = success

    // Same tile count
    CHECK_EQ(loaded.tileCount(), original.tileCount());

    // Spot-check a few tiles
    HexCoord spots[] = { {0,0}, {3,-1}, {-2,4} };
    for (const auto& c : spots) {
        const MapTile* to = original.tileAt(c);
        const MapTile* tl = loaded.tileAt(c);
        CHECK(to != nullptr);
        CHECK(tl != nullptr);
        if (to && tl) {
            CHECK(to->terrain  == tl->terrain);
            CHECK(to->passable == tl->passable);
        }
    }

    // Same object count
    CHECK_EQ(loaded.objects().size(), original.objects().size());
}

SUITE("WorldMap — setTile overwrites existing") {
    WorldMap map;
    map.generateProcedural(3, 1);

    MapTile custom;
    custom.terrain  = Terrain::Oasis;
    custom.passable = true;
    map.setTile({0, 0}, custom);

    const MapTile* t = map.tileAt({0, 0});
    CHECK(t != nullptr);
    if (t) CHECK(t->terrain == Terrain::Oasis);
}

SUITE("WorldMap — placeObject and removeObjectAt") {
    WorldMap map;
    map.generateProcedural(3, 1);

    size_t before = map.objects().size();

    MapObjectDef obj;
    obj.pos  = {1, 1};
    obj.type = ObjType::GoldMine;
    obj.name = "Test Mine";
    map.placeObject(obj);
    CHECK_EQ(map.objects().size(), before + 1);

    map.removeObjectAt({1, 1});
    CHECK_EQ(map.objects().size(), before);
}

SUITE("WorldMap — clear removes all tiles and objects") {
    WorldMap map;
    map.generateProcedural(5, 1);

    MapObjectDef obj;
    obj.pos = {0, 0};
    obj.type = ObjType::Town;
    obj.name = "Test Town";
    map.placeObject(obj);

    map.clear(0);

    CHECK_EQ(map.tileCount(), 1);
    CHECK_EQ(map.objects().size(), 0);
}

SUITE("WorldMap — hasTile returns correct presence") {
    WorldMap map;
    map.generateProcedural(3, 1);

    CHECK(map.hasTile({0, 0}));
    CHECK(map.hasTile({-2, 1}));
    CHECK(!map.hasTile({10, 10}));
    CHECK(!map.hasTile({100, -50}));
}

SUITE("WorldMap — removeTile deletes tile") {
    WorldMap map;
    map.generateProcedural(3, 1);

    CHECK(map.hasTile({0, 0}));
    map.removeTile({0, 0});
    CHECK(!map.hasTile({0, 0}));
}

SUITE("WorldMap — objectAt finds placed object") {
    WorldMap map;
    map.generateProcedural(3, 1);

    MapObjectDef obj;
    obj.pos = {2, -1};
    obj.type = ObjType::Dungeon;
    obj.name = "Test Dungeon";
    map.placeObject(obj);

    const MapObjectDef* found = map.objectAt({2, -1});
    CHECK(found != nullptr);
    CHECK(found->type == ObjType::Dungeon);
    CHECK(found->name == "Test Dungeon");

    CHECK(map.objectAt({0, 0}) == nullptr);
}

SUITE("WorldMap — findPath returns valid path") {
    WorldMap map;
    map.generateProcedural(5, 1);

    auto path = map.findPath({0, 0}, {2, 0});

    CHECK(!path.empty());
    CHECK(path.front() == HexCoord(0, 0));
    CHECK(path.back() == HexCoord(2, 0));
}

SUITE("WorldMap — findPathWeighted uses move costs") {
    WorldMap map;
    map.generateProcedural(5, 1);

    MapTile dune;
    dune.terrain = Terrain::Dune;
    dune.passable = true;
    dune.moveCost = 1.5f;
    map.setTile({1, 0}, dune);

    auto path = map.findPathWeighted({0, 0}, {2, 0});

    CHECK(!path.empty());
    CHECK(path.front() == HexCoord(0, 0));
    CHECK(path.back() == HexCoord(2, 0));
}

SUITE("WorldMap — name and radius metadata") {
    WorldMap map;
    map.generateProcedural(3, 1);

    CHECK(map.radius() == 3);
    CHECK(map.name() == "Untitled Map");

    map.setName("Test Map");
    CHECK(map.name() == "Test Map");
}

#endif // WORLDMAP_IMPL
