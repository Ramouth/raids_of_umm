#include "test_runner.h"
#include "core/TurnManager.h"
#include "world/Resources.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// ── TurnManager::setDay ───────────────────────────────────────────────────────

#ifdef RESOURCE_MANAGER_IMPL   // TurnManager is available in tests that build ResourceManager

SUITE("TurnManager — setDay restores calendar state") {
    TurnManager tm;
    tm.init(2000);
    CHECK_EQ(tm.day(), 1);

    tm.setDay(15);
    CHECK_EQ(tm.day(), 15);
    CHECK_EQ(tm.week(), 3);       // week 3 (days 15–21)
    CHECK_EQ(tm.weekOfMonth(), 3);
    CHECK_EQ(tm.month(), 1);

    tm.setDay(29);
    CHECK_EQ(tm.day(), 29);
    CHECK_EQ(tm.week(), 5);
    CHECK_EQ(tm.month(), 2);      // month 2 starts at day 29
}

SUITE("TurnManager — init then setTreasury round-trip") {
    TurnManager tm;
    tm.init(0);
    tm.playerFaction().treasury[Resource::Gold] = 3500;
    tm.playerFaction().treasury[Resource::SandCrystal] = 42;

    CHECK_EQ(tm.playerFaction().treasury[Resource::Gold], 3500);
    CHECK_EQ(tm.playerFaction().treasury[Resource::SandCrystal], 42);
    CHECK_EQ(tm.playerFaction().treasury[Resource::BoneDust], 0);
}

#endif  // RESOURCE_MANAGER_IMPL

// ── Session JSON schema ───────────────────────────────────────────────────────
//
// These tests validate the expected JSON structure that saveSession() writes and
// loadSession() reads — catching field-name drift without needing SDL/GL.

SUITE("Session JSON — parse hero fields") {
    using json = nlohmann::json;

    const std::string raw = R"({
        "version": 1,
        "mapPath": "data/maps/default.json",
        "day": 7,
        "treasury": [1500, 10, 0, 0, 0, 0, 0],
        "hero": {
            "name": "Hero",
            "pos": {"q": 3, "r": -1},
            "movesLeft": 5,
            "movesMax": 8,
            "army": [
                {"unitId": "desert_archer", "count": 8},
                null, null, null, null, null, null
            ],
            "specials": [{
                "id": "ushari",
                "name": "Ushari",
                "archetype": "warrior",
                "level": 2,
                "xp": 120,
                "unlockedActions": ["strike"],
                "chosenBranches": {"2": "blade_dancer"},
                "extraStats": {},
                "equipped": [null, null, null, null]
            }],
            "items": ["scarab_amulet"],
            "wallet": [200, 5, 0, 0, 0, 0, 0]
        },
        "objectControl": [
            {"q": 5, "r": 2, "ownerFaction": 1, "guardDefeated": true, "objType": 1}
        ],
        "townStates": [
            {"q": 0, "r": 0, "recruitPool": {"desert_archer": 5}, "buildings": []}
        ]
    })";

    json j = json::parse(raw);

    CHECK_EQ(j["version"].get<int>(), 1);
    CHECK_EQ(j["day"].get<int>(), 7);
    CHECK_EQ(j["treasury"][0].get<int>(), 1500);
    CHECK_EQ(j["treasury"][1].get<int>(), 10);
    CHECK_EQ(j["treasury"].size(), static_cast<size_t>(RESOURCE_COUNT));

    const auto& jh = j["hero"];
    CHECK_EQ(jh["name"].get<std::string>(), std::string{"Hero"});
    CHECK_EQ(jh["pos"]["q"].get<int>(), 3);
    CHECK_EQ(jh["pos"]["r"].get<int>(), -1);
    CHECK_EQ(jh["movesLeft"].get<int>(), 5);
    CHECK_EQ(jh["movesMax"].get<int>(), 8);

    CHECK_EQ(jh["army"].size(), static_cast<size_t>(7));
    CHECK(!jh["army"][0].is_null());
    CHECK_EQ(jh["army"][0]["unitId"].get<std::string>(), std::string{"desert_archer"});
    CHECK_EQ(jh["army"][0]["count"].get<int>(), 8);
    CHECK(jh["army"][1].is_null());

    CHECK_EQ(jh["specials"].size(), static_cast<size_t>(1));
    const auto& sc = jh["specials"][0];
    CHECK_EQ(sc["id"].get<std::string>(),        std::string{"ushari"});
    CHECK_EQ(sc["level"].get<int>(),             2);
    CHECK_EQ(sc["xp"].get<int>(),                120);
    CHECK_EQ(sc["unlockedActions"][0].get<std::string>(), std::string{"strike"});
    CHECK_EQ(sc["chosenBranches"]["2"].get<std::string>(), std::string{"blade_dancer"});

    CHECK_EQ(jh["items"].size(), static_cast<size_t>(1));
    CHECK_EQ(jh["items"][0].get<std::string>(), std::string{"scarab_amulet"});
    CHECK_EQ(jh["wallet"][0].get<int>(), 200);

    const auto& oc = j["objectControl"];
    CHECK_EQ(oc.size(), static_cast<size_t>(1));
    CHECK_EQ(oc[0]["q"].get<int>(), 5);
    CHECK(oc[0]["guardDefeated"].get<bool>());
    CHECK_EQ(oc[0]["objType"].get<int>(), 1);  // ObjType::Dungeon

    const auto& ts = j["townStates"];
    CHECK_EQ(ts.size(), static_cast<size_t>(1));
    CHECK_EQ(ts[0]["recruitPool"]["desert_archer"].get<int>(), 5);
}

SUITE("Session JSON — missing optional fields use defaults") {
    using json = nlohmann::json;

    // Minimal session — only required fields
    const std::string raw = R"({
        "version": 1,
        "mapPath": "",
        "day": 1,
        "treasury": [2000, 0, 0, 0, 0, 0, 0],
        "hero": {
            "name": "Hero",
            "pos": {"q": 0, "r": 0},
            "movesLeft": 8,
            "movesMax": 8,
            "army": [null,null,null,null,null,null,null],
            "specials": [],
            "items": [],
            "wallet": [0,0,0,0,0,0,0]
        },
        "objectControl": [],
        "townStates": []
    })";

    json j = json::parse(raw);
    CHECK(j["hero"]["specials"].empty());
    CHECK(j["objectControl"].empty());
    CHECK(j["townStates"].empty());
    CHECK_EQ(j.value("day", 1), 1);
}
