#!/usr/bin/env python3
"""
gen_old_passage_map.py — draft layout for the demo map "The Old Passage".

Writes a version-1 map (same format as WorldMap::saveJson) to
data/maps/old_passage.json. This is a *starting point*: open it in the
World Builder and hand-tune from there. Re-running overwrites the file.

Layout (west → east, see ROADMAP.md "Demo — The Old Passage"):

    Home valley | Ridge Pass (G1) | Contested middle | Canyon Ford (G3) | Far reach

Coordinates: axial (q, r), flat-top hexes. Screen x grows with q; screen
row is Y = r + q/2. Objects below are placed by (q, Y) and converted.

Usage:  python3 scripts/gen_old_passage_map.py [--seed N]
"""
import argparse
import heapq
import json
from pathlib import Path

RADIUS = 13
OUT = Path(__file__).resolve().parent.parent / "data" / "maps" / "old_passage.json"

# Terrain defaults — mirror terrainDefaultMoveCost() in src/world/MapTile.h.
# Mountain/river are authored impassable here (as in default.json) so they
# act as hard walls between zones.
TERRAIN = {
    "sand":     (True,  1.0),
    "dune":     (True,  1.5),
    "rock":     (True,  1.25),
    "ruins":    (True,  1.25),
    "oasis":    (True,  1.2),
    "obsidian": (False, 0.0),
    "mountain": (False, 1.8),
    "river":    (False, 0.0),
}

DIRS = [(1, 0), (1, -1), (0, -1), (-1, 0), (-1, 1), (0, 1)]

RIDGE_Q  = (-7, -6)   # two-thick mountain wall between home and middle
CANYON_Q = 5          # river between middle and far reach


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


# ── Objects ──────────────────────────────────────────────────────────────────
# (type, name, q, Y, factionId). Order matters: the Godot slice spawns the
# hero at the first town, so Khemret stays first.
OBJECTS = [
    # Home valley
    ("town",          "Khemret",               -10,  0,  1),
    ("gold_mine",     "Khemret Gold Mine",     -11, -4,  0),
    ("sawmill",       "Palm Grove Sawmill",     -9,  5,  0),
    # Ridge Pass — the single gap in the ridge
    ("guard",         "Ridge Pass Guard",       -6,  0,  0),
    # Contested middle
    ("town",          "Tharakh",                 0, -1,  0),
    ("quarry",        "Desert Quarry",          -3, -7,  0),
    ("obsidian_vent", "Obsidian Vent",           2,  7,  0),
    ("old_mine",      "Old Mine of Sehet",      -3,  7,  0),
    ("old_mine",      "Old Mine of Nebu",        3, -8,  0),
    ("dungeon",       "Collapsed Shrine",       -1, 10,  0),
    ("guard",         "Scorpion Nest",           1,  3,  0),
    # Canyon Ford — the single crossing of the river
    ("guard",         "Canyon Ford Guard",       5, -1,  0),
    # Far reach
    ("crystal_mine",  "Crystal Cavern",          8,  5,  0),
    ("quest_giver",   "Kharim's Camp",           7, -5,  0),
    ("old_mine",      "Old Mine of Djer",       10, -3,  0),
    ("old_mine",      "Old Mine of Anhur",       9,  6,  0),
    ("town",          "Ain Sharu",              11,  1,  2),   # Shariw
    ("guard",         "Wraith Host",             7,  2,  0),
]

# Terrain patches around landmarks: (centre q, Y, radius, terrain)
PATCHES = [
    (-9, 5, 1, "oasis"),       # palm grove by the sawmill
    (-12, 2, 1, "oasis"),
    (2, 7, 1, "obsidian"),     # vent field (ring only — vent tile stays passable)
    (-3, 7, 1, "ruins"),
    (3, -8, 1, "ruins"),
    (10, -3, 1, "ruins"),
    (9, 6, 1, "ruins"),
    (-3, -7, 1, "rock"),
    (8, 5, 1, "rock"),
    (-1, 10, 1, "ruins"),
]

# Road waypoints (q, Y): Khemret → Ridge Pass → Tharakh → Canyon Ford → Ain Sharu
ROAD = [(-10, 0), (-6, 0), (0, -1), (5, -1), (11, 1)]


def build(seed):
    tiles = {}
    for q in range(-RADIUS, RADIUS + 1):
        for r in range(-RADIUS, RADIUS + 1):
            if not in_map(q, r):
                continue
            n = noise(q, r, seed)
            ring = dist((q, r), (0, 0))
            if ring == RADIUS:
                t = "mountain"                      # frame the map
            elif q > CANYON_Q:
                t = "dune" if n < 0.55 else "sand"  # far reach: deep desert
            elif q < RIDGE_Q[0]:
                t = "dune" if n < 0.20 else "sand"  # home valley: easy going
            else:
                t = "dune" if n < 0.35 else ("rock" if n > 0.93 else "sand")
            tiles[(q, r)] = t

    # Ridge: full mountain wall, two thick, with ragged spurs.
    for (q, r) in list(tiles):
        if q in RIDGE_Q:
            tiles[(q, r)] = "mountain"
        elif q in (RIDGE_Q[0] - 1, RIDGE_Q[1] + 1) and noise(q, r, seed + 1) < 0.3:
            tiles[(q, r)] = "mountain"

    # Canyon: river column with rocky banks.
    for (q, r) in list(tiles):
        if q == CANYON_Q:
            tiles[(q, r)] = "river"
        elif q in (CANYON_Q - 1, CANYON_Q + 1) and noise(q, r, seed + 2) < 0.4:
            tiles[(q, r)] = "rock"

    for cq, cy, rad, t in PATCHES:
        c = at(cq, cy)
        for cell in tiles:
            if 0 < dist(cell, c) <= rad or (dist(cell, c) == 0 and t != "obsidian"):
                if tiles[cell] not in ("mountain", "river"):
                    # Obsidian is impassable — scatter it so the vent stays reachable.
                    if t == "obsidian" and noise(*cell, seed + 3) > 0.5:
                        tiles[cell] = "rock"
                    else:
                        tiles[cell] = t

    # Open the Ridge Pass (one cell per ridge column) and the Canyon Ford.
    pass_west = at(RIDGE_Q[0], 0.5)
    pass_east = at(RIDGE_Q[1], 0)
    assert pass_east in [(pass_west[0] + dq, pass_west[1] + dr) for dq, dr in DIRS]
    for cell in (pass_west, pass_east, at(CANYON_Q, -1)):
        tiles[cell] = "sand"
    # Clear spurs directly either side of the pass so it's reachable.
    for cell in (at(RIDGE_Q[0] - 1, 0.5), at(RIDGE_Q[1] + 1, 0),
                 at(CANYON_Q - 1, -1), at(CANYON_Q + 1, -1)):
        tiles[cell] = "sand"

    objects = []
    for typ, name, q, y, fac in OBJECTS:
        cell = at(q, y)
        assert cell in tiles, f"{name} at {cell} is outside the map"
        if tiles[cell] in ("mountain", "river", "obsidian"):
            tiles[cell] = "sand"
        o = {"q": cell[0], "r": cell[1], "type": typ, "name": name}
        if fac:
            o["factionId"] = fac
        objects.append(o)

    roads = set()
    for a, b in zip(ROAD, ROAD[1:]):
        path = route(tiles, at(*a), at(*b))
        assert path, f"No route {a} → {b}"
        roads.update(path)

    out_tiles = []
    for (q, r), t in sorted(tiles.items()):
        passable, cost = TERRAIN[t]
        tile = {"q": q, "r": r, "terrain": t, "passable": passable,
                "moveCost": cost, "variant": 0}
        if (q, r) in roads and passable:
            tile["road"] = True
            tile["moveCost"] = min(cost, 0.5)
        out_tiles.append(tile)

    check_reachable(tiles, objects)
    return {"version": 1, "name": "The Old Passage", "radius": RADIUS,
            "tiles": out_tiles, "objects": objects}


def route(tiles, start, goal):
    """Cheapest path over passable terrain (Dijkstra)."""
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
            if nxt not in tiles or not TERRAIN[tiles[nxt]][0]:
                continue
            nc = c + TERRAIN[tiles[nxt]][1]
            if nc < best.get(nxt, 1e9):
                best[nxt] = nc
                prev[nxt] = cell
                heapq.heappush(pq, (nc, nxt))
    return None


def check_reachable(tiles, objects):
    """Every object must be reachable from Khemret (fail loudly if not)."""
    start = (objects[0]["q"], objects[0]["r"])
    seen, stack = {start}, [start]
    while stack:
        cell = stack.pop()
        for dq, dr in DIRS:
            nxt = (cell[0] + dq, cell[1] + dr)
            if nxt in tiles and nxt not in seen and TERRAIN[tiles[nxt]][0]:
                seen.add(nxt)
                stack.append(nxt)
    missing = [o["name"] for o in objects if (o["q"], o["r"]) not in seen]
    assert not missing, f"Unreachable from Khemret: {missing}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    m = build(args.seed)
    OUT.write_text(json.dumps(m, indent=2))
    print(f"Wrote {OUT.relative_to(OUT.parent.parent.parent)}: "
          f"{len(m['tiles'])} tiles, {len(m['objects'])} objects")


if __name__ == "__main__":
    main()
