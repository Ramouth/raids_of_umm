#!/usr/bin/env python3
"""
gen_north_map.py — stage 1 of the demo: "The Old Passage" in the northern marches.

Writes a version-1 map (same format as WorldMap::saveJson) to
data/maps/old_passage.json. The desert draft this replaced lives on as
data/maps/desert_passage.json (stage 2). Re-running overwrites the file;
hand-tune in the World Builder afterwards.

Layout (west → east), see docs/map_density.md for the object budget:

    Varen vale | Coldwater river (bridges) | Hallowmere lowlands | Greyfang range (passes) | Far marches

Coordinates: axial (q, r), flat-top hexes. Screen x grows with q; screen
row is Y = r + q/2. Objects are placed by (q, Y) and converted.

Usage:  python3 scripts/gen_north_map.py [--seed N]
"""
import argparse
import heapq
import json
from pathlib import Path

RADIUS = 14
OUT = Path(__file__).resolve().parent.parent / "data" / "maps" / "old_passage.json"

# (passable, moveCost) — mirror terrainDefaultMoveCost() in src/world/MapTile.h.
TERRAIN = {
    "grass":    (True,  1.0),
    "forest":   (True,  1.5),
    "highland": (True,  1.25),
    "swamp":    (True,  1.75),
    "mountain": (False, 1.8),
    "lake":     (False, 0.0),
    "river":    (False, 0.0),
}

DIRS = [(1, 0), (1, -1), (0, -1), (-1, 0), (-1, 1), (0, 1)]

RIVER_Q = -5          # the Coldwater, between Varen vale and the lowlands
RANGE_Q = (5, 6)      # the Greyfangs, between the lowlands and the far marches
BRIDGES = [0, 8]      # river crossings by screen row Y (north road, old ford)
PASSES = [-1, 7]      # mountain passes by screen row Y (Greyfang Pass, Troll Gate)


def in_map(q, r):
    return max(abs(q), abs(r), abs(q + r)) <= RADIUS


def at(q, y):
    """(q, screen-row Y) → axial (q, r)."""
    return (q, round(y - q / 2))


def dist(a, b):
    dq, dr = a[0] - b[0], a[1] - b[1]
    return max(abs(dq), abs(dr), abs(dq + dr))


def noise(q, r, seed):
    """Deterministic 0..1 hash noise (no external deps)."""
    h = (q * 374761393 + r * 668265263 + seed * 1442695040888963407) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 0xFFFF


def smooth(q, r, seed, scale=3):
    """Blobby noise: average over a small neighbourhood so forests clump."""
    total, n = 0.0, 0
    for dq in range(-1, 2):
        for dr in range(-1, 2):
            total += noise((q + dq * scale) // scale, (r + dr * scale) // scale, seed)
            n += 1
    return total / n


# ── Objects ──────────────────────────────────────────────────────────────────
# (type, name, q, Y, factionId, kind). Varenhold stays first: the hero's home.
OBJECTS = [
    # ── Varen vale (home, west) ── learn the controls; light wolves only
    ("town",           "Varenhold",               -11,  0, 1, ""),
    ("gold_mine",      "Varen Gold Mine",         -12, -5, 0, ""),
    ("sawmill",        "Pinewood Sawmill",         -8,  5, 0, ""),
    ("mill",           "Varen Windmill",           -9, -3, 0, "windmill"),
    ("stables",        "Varen Stables",           -10, -2, 0, ""),
    ("pickup",         "Fallen Standing Stone",   -13,  3, 0, "stone"),
    ("pickup",         "Log Pile",                -10,  3, 0, "wood"),
    ("pickup",         "Cut Stone",               -12,  5, 0, "stone"),
    ("pickup",         "Hunters' Camp",            -8, -1, 0, "campfire"),
    ("pickup",         "Wolf-Gnawed Purse",        -7, -8, 0, "gold"),
    ("guard",          "Hill Wolves",              -8, -7, 0, ""),
    ("dwelling",       "Wolf Den",                -11, -8, 0, "grey_wolf"),
    ("guard",          "Den Wolves",              -10, -7, 0, ""),
    ("pickup",         "Brigand Strongbox",        -9,  7, 0, "chest"),
    ("guard",          "Brigand Lookouts",         -8,  8, 0, ""),
    # ── Coldwater crossings ──
    ("guard",          "Bridge Wardens",           -4,  0, 0, ""),
    ("guard",          "Ford Wights",              -4,  8, 0, ""),
    # ── Hallowmere lowlands (middle) ── the cousin's country
    ("town",           "Hallowmere",                0, -1, 1, ""),
    ("quarry",         "Hallow Quarry",            -2, -7, 0, ""),
    ("sawmill",        "Mere Sawmill",              2,  6, 0, ""),
    ("obsidian_vent",  "Blackglass Seam",           4, 10, 0, ""),
    ("old_mine",       "Old Mine of Dunmere",      -2,-11, 0, ""),
    ("old_mine",       "Old Mine of Carrow",       -1, 11, 0, ""),
    ("dungeon",        "Barrow of the Drowned King", 4, 4, 0, ""),
    ("watchtower",     "Hallow Watch",             -3,  3, 0, ""),
    ("mill",           "Mere Watermill",            1,  4, 0, "watermill"),
    ("stables",        "Hale Stables",              3, -4, 0, ""),
    ("pickup",         "A Sergeant's Field Book",  -1, -5, 0, "tome"),     # rare XP: a war journal
    ("dwelling",       "Brigand Camp",              4, -9, 0, "brigand"),
    ("guard",          "Brigand Band",              3, -8, 0, ""),
    ("pickup",         "Tithe Silver",             -3, -3, 0, "gold"),
    ("pickup",         "Felled Oaks",               1, -7, 0, "wood"),
    ("pickup",         "Mason's Cache",            -3,  7, 0, "stone"),
    ("pickup",         "Blackglass Shards",         3,  8, 0, "obsidian"),
    ("pickup",         "Drovers' Camp",             2, -3, 0, "campfire"),
    ("pickup",         "Drowned Chest",            -3, 11, 0, "chest"),
    ("guard",          "Mere Dead",                -2, 10, 0, ""),
    ("guard",          "Highland Bears",            0, -8, 0, ""),
    ("guard",          "Lowland Wolves",           -1,  3, 0, ""),
    ("artifact",       "Barrow Cairn",              2, 11, 0, "barrow_blade"),
    ("guard",          "Cairn Wights",              1, 10, 0, ""),
    # ── Greyfang passes ──
    ("guard",          "Greyfang Pass",             5, -1, 0, ""),
    ("guard",          "Troll Gate",                6,  7, 0, ""),
    # ── Far marches (east) ── the old mines under the mountains
    ("town",           "Shariw War Camp",          11,  1, 2, ""),
    ("quest_giver",    "Kharim's Camp",             8, -5, 0, ""),
    ("crystal_mine",   "Frostglass Cavern",         9,  5, 0, ""),
    ("gold_mine",      "Greyfang Gold Mine",       12, -4, 0, ""),
    ("old_mine",       "Old Mine of Kaldur",       10, -8, 0, ""),
    ("old_mine",       "Old Mine of Brannoc",       8,  9, 0, ""),
    ("watchtower",     "Greyfang Watch",            8, -1, 0, ""),
    ("mill",           "Moor Windmill",            12,  5, 0, "windmill"),
    ("pickup",         "The Greyfang Campaigns",    7,  5, 0, "tome"),     # rare XP: a war journal
    ("guard",          "Treant Grove",              9,  2, 0, ""),
    ("guard",          "Troll Warband",            10, -2, 0, ""),
    ("pickup",         "Raiders' Hoard",           11, -2, 0, "gold"),
    ("pickup",         "Frost Crystals",           10,  7, 0, "crystal"),
    ("pickup",         "Frozen Chest",             13, -3, 0, "chest"),
    ("pickup",         "Abandoned Camp",            7, -8, 0, "campfire"),
    ("pickup",         "Blackglass Heap",           7, -6, 0, "obsidian"),
    ("artifact",       "Moorland Cairn",            13,  6, 0, "bearhide_cloak"),
    ("guard",          "Cairn Bears",              12,  7, 0, ""),
    # ── filler to HoMM3 density (docs/map_density.md) ──
    ("pickup",         "Windfall Timber",          -6,  4, 0, "wood"),
    ("pickup",         "Toll Coins",               -3, -1, 0, "gold"),
    ("pickup",         "Hoarfrost Crystals",        0,-11, 0, "crystal"),
    ("pickup",         "Charcoal Burners",          1,  8, 0, "campfire"),
    ("pickup",         "Hermit's Chest",           -6, -9, 0, "chest"),
    ("guard",          "Hermit's Wolves",          -7,-10, 0, ""),
    ("quest_giver",    "Hooded Stranger",          -7, -8.5, 0, "druid"),   # level 1: the druid with the last pack
    ("pickup",         "Pilgrims' Alms",            9, -2, 0, "gold"),
    ("artifact",       "Oath Cairn",              -13, -3, 0, "oathstone_amulet"),
    ("guard",          "Tarn Brigands",           -12, -2, 0, ""),
    ("guard",          "Marsh Wights",              0,  6, 0, ""),
    # ── the mythos: obelisks (HoMM3 puzzle map → rule out a false mine) and relics ──
    ("obelisk",        "Tarn Obelisk",             -7,  1, 0, ""),
    ("obelisk",        "Quarry Obelisk",           -4, -8, 0, ""),
    ("obelisk",        "Drowned Obelisk",          -1,  7, 0, ""),
    ("obelisk",        "Greyfang Obelisk",         11,  3, 0, ""),
    ("artifact",       "Humming Hollow",           -4,-11, 0, "listening_shard"),
    ("artifact",       "Sinkhole",                  9, -7, 0, "warm_stone"),
]

# Terrain patches: (centre q, Y, radius, terrain)
PATCHES = [
    (-10, -5, 1, "forest"), (-7, 3, 2, "forest"), (-12, 7, 1, "forest"),
    (-6, -10, 1, "forest"), (2, -10, 1, "forest"), (-3, -1, 1, "forest"),
    (2, 1, 1, "lake"),      # Hallow Mere
    (-12, -1, 0, "lake"),   # Varen tarn
    (-2, 5, 1, "swamp"),    (0, 9, 1, "swamp"),  (-4, 9, 0, "swamp"),
    (9, 2, 1, "forest"),    # the treant grove
    (11, -6, 1, "highland"),(12, 3, 1, "highland"), (8, 7, 1, "highland"),
    (7, -3, 0, "lake"),
    (-7, -4, 1, "lake"),    # Coldwater tarn
    (-4, -5, 1, "lake"),
    (-11, 7, 1, "lake"),
    (10, 0, 0, "lake"),
]

# Mountain spurs around old mines: open toward (dq, dr) only.
SPURS = [
    ("Old Mine of Dunmere", (0, 1)), ("Old Mine of Carrow", (0, -1)),
    ("Old Mine of Kaldur", (0, 1)), ("Old Mine of Brannoc", (0, -1)),
]

# Road waypoints (q, Y): Varenhold → north bridge → Hallowmere → Greyfang Pass → war camp
ROAD = [(-11, 0), (-5, 0), (0, -1), (5, -1), (11, 1)]


def build(seed):
    tiles = {}
    for q in range(-RADIUS, RADIUS + 1):
        for r in range(-RADIUS, RADIUS + 1):
            if not in_map(q, r):
                continue
            n = noise(q, r, seed)
            clump = smooth(q, r, seed + 11)
            ring = dist((q, r), (0, 0))
            if ring == RADIUS or (ring == RADIUS - 1 and n < 0.35):
                t = "mountain"                              # frame the map
            elif q > RANGE_Q[1]:
                t = "highland" if n < 0.35 else ("forest" if clump > 0.62 else "grass")
            elif clump > 0.54 or noise(q, r, seed + 21) > 0.84:
                t = "forest"                                # woods clump, copses scatter
            else:
                t = "grass"
            tiles[(q, r)] = t

    # The Coldwater: a continuous river walked north → south through the two
    # bridge points, wandering a hex either side of RIVER_Q.
    river = river_walk(seed)
    for cell in river:
        if cell in tiles:
            tiles[cell] = "river"

    # The Greyfangs: a two-thick mountain wall with ragged spurs.
    for (q, r) in list(tiles):
        if q in RANGE_Q:
            tiles[(q, r)] = "mountain"
        elif q in (RANGE_Q[0] - 1, RANGE_Q[1] + 1) and noise(q, r, seed + 1) < 0.25:
            tiles[(q, r)] = "mountain"

    for cq, cy, rad, t in PATCHES:
        c = at(cq, cy)
        for cell in tiles:
            if dist(cell, c) <= rad and tiles[cell] not in ("mountain", "river"):
                tiles[cell] = t

    # Bridges: a river cell stays river but is passable road (see road pass below).
    bridges = {at(RIVER_Q, y) for y in BRIDGES}
    for b in bridges:
        for dq, dr in ((-1, 0), (1, 0), (-1, 1), (1, -1)):   # clear the banks
            bank = (b[0] + dq, b[1] + dr)
            if tiles.get(bank) in ("mountain", "lake"):
                tiles[bank] = "grass"
    # Passes: open each column of the range, and the spur cells either side.
    for y in PASSES:
        for q in range(RANGE_Q[0] - 1, RANGE_Q[1] + 2):
            tiles[at(q, y)] = "grass"

    objects = []
    for typ, name, q, y, fac, kind in OBJECTS:
        cell = at(q, y)
        assert cell in tiles, f"{name} at {cell} is outside the map"
        assert tiles[cell] != "river", f"{name} at {cell} sits in the river"
        if not TERRAIN[tiles[cell]][0] and cell not in bridges:
            tiles[cell] = "grass"
        o = {"q": cell[0], "r": cell[1], "type": typ, "name": name}
        if fac:
            o["factionId"] = fac
        if kind:
            o["kind"] = kind
        objects.append(o)
    by_name = {o["name"]: (o["q"], o["r"]) for o in objects}
    occupied = set(by_name.values())

    for name, (oq, orr) in SPURS:
        c = by_name[name]
        entry = (c[0] + oq, c[1] + orr)
        for dq, dr in DIRS:
            cell = (c[0] + dq, c[1] + dr)
            if cell in tiles and cell != entry and cell not in occupied:
                tiles[cell] = "mountain"
        if tiles.get(entry) in ("mountain", "lake", "river"):
            tiles[entry] = "grass"

    passable = lambda cell: cell in bridges or TERRAIN[tiles[cell]][0]
    roads = set()
    for a, b in zip(ROAD, ROAD[1:]):
        path = route(tiles, bridges, at(*a), at(*b))
        assert path, f"No route {a} → {b}"
        roads.update(path)

    out_tiles = []
    for (q, r), t in sorted(tiles.items()):
        ok, cost = TERRAIN[t]
        tile = {"q": q, "r": r, "terrain": t, "passable": ok, "moveCost": cost, "variant": 0}
        if (q, r) in bridges:
            tile.update(passable=True, road=True, moveCost=0.5)
        elif (q, r) in roads and ok:
            tile["road"] = True
            tile["moveCost"] = min(cost, 0.5)
        out_tiles.append(tile)

    check_reachable(tiles, bridges, objects)
    return {"version": 1, "name": "The Old Passage", "radius": RADIUS,
            "ground": "grass", "tiles": out_tiles, "objects": objects}


def river_walk(seed):
    """Cells of a gap-free river: each step moves to a neighbour further south."""
    waypoints = [(RIVER_Q, -RADIUS - RIVER_Q)] + [at(RIVER_Q, y) for y in BRIDGES] + [(RIVER_Q, RADIUS)]
    moves = [(0, 1), (1, 0), (-1, 1)]          # south, south-east, south-west
    cell, cells = waypoints[0], [waypoints[0]]
    for goal in waypoints[1:]:
        while cell != goal:
            def score(m):
                nxt = (cell[0] + m[0], cell[1] + m[1])
                wander = noise(*nxt, seed + 5) * 0.9 if abs(nxt[0] - RIVER_Q) <= 1 else 9
                return dist(nxt, goal) + wander
            m = min(moves, key=score)
            cell = (cell[0] + m[0], cell[1] + m[1])
            cells.append(cell)
            assert len(cells) < 200, "river walk lost its way"
    return cells


def route(tiles, bridges, start, goal):
    """Cheapest path over passable terrain (Dijkstra); prefers open grass."""
    best = {start: 0.0}
    prev = {}
    pq = [(0.0, start)]
    while pq:
        c, cell = heapq.heappop(pq)
        if cell == goal:
            path = [cell]
            while cell in prev:
                cell = prev[cell]
                path.append(cell)
            return path
        if c > best[cell]:
            continue
        for dq, dr in DIRS:
            nxt = (cell[0] + dq, cell[1] + dr)
            if nxt not in tiles:
                continue
            if nxt not in bridges and not TERRAIN[tiles[nxt]][0]:
                continue
            nc = c + (0.5 if nxt in bridges else TERRAIN[tiles[nxt]][1])
            if nc < best.get(nxt, 1e9):
                best[nxt] = nc
                prev[nxt] = cell
                heapq.heappush(pq, (nc, nxt))
    return None


def check_reachable(tiles, bridges, objects):
    """Every object must be reachable from Varenhold (fail loudly if not)."""
    start = (objects[0]["q"], objects[0]["r"])
    seen, stack = {start}, [start]
    while stack:
        cell = stack.pop()
        for dq, dr in DIRS:
            nxt = (cell[0] + dq, cell[1] + dr)
            if nxt in tiles and nxt not in seen and (nxt in bridges or TERRAIN[tiles[nxt]][0]):
                seen.add(nxt)
                stack.append(nxt)
    missing = [o["name"] for o in objects if (o["q"], o["r"]) not in seen]
    assert not missing, f"Unreachable from Varenhold: {missing}"
    cells = [(o["q"], o["r"]) for o in objects]
    dupes = {c for c in cells if cells.count(c) > 1}
    assert not dupes, f"Objects share a cell: {dupes}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    m = build(args.seed)
    OUT.write_text(json.dumps(m, indent=2))
    walk = sum(1 for t in m["tiles"] if t["passable"])
    print(f"Wrote {OUT.relative_to(OUT.parent.parent.parent)}: {len(m['tiles'])} tiles "
          f"({walk} walkable), {len(m['objects'])} objects "
          f"(1 per {walk / len(m['objects']):.1f} walkable hexes)")


if __name__ == "__main__":
    main()
