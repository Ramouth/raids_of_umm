#!/usr/bin/env python3
"""
Generate data/maps/the_pale_divide.json — campaign map 2 (two factions + wall).

Layout (axial flat-top hex, r increases southward):
  r <= -4          : Verdant Reach territory  (Grass / Forest / Highland)
  r == -3          : Rough northern border     (Highland / Forest)
  r == -2          : THE WALL row 1            (Wall, except gate at q=0)
  r == -1          : THE WALL row 2            (Wall, except gate at q=0)
  r ==  0          : Rough southern border     (Rock / Ruins)
  r >=  1          : Umm'Natur territory       (Sand / Dune)
  |q| or |r| == 8  : Impassable mountain ring  (border)
"""

import json, random, math

RADIUS = 8
SEED   = 42
rng    = random.Random(SEED)

# ── hex helpers ──────────────────────────────────────────────────────────────

def hex_in_radius(radius):
    """Yield all (q, r) axial coords within flat-top hex grid of given radius."""
    for q in range(-radius, radius + 1):
        r1 = max(-radius, -q - radius)
        r2 = min( radius, -q + radius)
        for r in range(r1, r2 + 1):
            yield q, r

# ── terrain assignment ───────────────────────────────────────────────────────

GATE_Q = 0   # q column of the gate gap in the wall

def terrain_for(q, r):
    dist = max(abs(q), abs(r), abs(q + r))

    # Impassable border ring
    if dist == RADIUS:
        return "mountain", False, 2.0

    # ── THE WALL ──────────────────────────────────────────────────────────────
    if r in (-2, -1):
        if q == GATE_Q:
            # Gate passage — grass on north wall row, sand on south wall row
            return ("grass" if r == -2 else "sand"), True, 1.0
        return "wall", False, 0.0

    # ── NORTHERN TERRITORY  (r <= -3) ─────────────────────────────────────────
    if r <= -3:
        if dist >= RADIUS - 1:
            return "mountain", False, 2.0
        if r <= -3:
            n = rng.random()
            if   n < 0.10: return "forest",   True, 1.5
            elif n < 0.20: return "highland",  True, 1.25
            else:           return "grass",     True, 1.0

    # ── SOUTHERN BORDER STRIP (r == 0) ────────────────────────────────────────
    if r == 0:
        n = rng.random()
        if n < 0.5: return "rock",  True, 1.25
        else:       return "ruins", True, 1.25

    # ── SOUTHERN TERRITORY (r >= 1) ───────────────────────────────────────────
    if dist >= RADIUS - 1:
        return "mountain", False, 2.0
    n = rng.random()
    if   n < 0.08: return "oasis",    True, 1.0
    elif n < 0.16: return "rock",     True, 1.25
    elif n < 0.22: return "ruins",    True, 1.25
    elif n < 0.26: return "obsidian", False, 0.0
    elif n < 0.38: return "dune",     True, 1.5
    else:           return "sand",     True, 1.0

    return "sand", True, 1.0


def variant_for(terrain):
    counts = {"grass": 4, "forest": 2, "highland": 1,
              "sand": 5, "dune": 3, "rock": 2, "oasis": 2, "ruins": 2,
              "wall": 1, "mountain": 2, "obsidian": 1, "river": 1,
              "battle": 1, "ruins": 2}
    n = counts.get(terrain, 1)
    return rng.randint(0, max(0, n - 1))

# ── build tile list ──────────────────────────────────────────────────────────

tiles = []
for q, r in hex_in_radius(RADIUS):
    terrain, passable, move_cost = terrain_for(q, r)
    tiles.append({
        "q": q, "r": r,
        "terrain": terrain,
        "passable": passable,
        "moveCost": move_cost,
        "variant": variant_for(terrain)
    })

# ── objects ──────────────────────────────────────────────────────────────────

objects = [
    # ── VERDANT REACH (player, factionId=1) ──────────────────────────────────
    {
        "type": "town", "name": "Dun Daghan",
        "q": -1, "r": -5, "factionId": 1
    },
    {
        "type": "goldmine", "name": "Highland Gold",
        "q": -3, "r": -4, "factionId": 1
    },
    {
        "type": "sawmill", "name": "Northern Timber",
        "q":  3, "r": -5, "factionId": 0
    },
    {
        "type": "dungeon", "name": "The Fae Barrow",
        "q":  2, "r": -4, "factionId": 0
    },
    {
        "type": "artifact", "name": "Stag Crown of Daghan",
        "q": -2, "r": -6, "factionId": 0
    },

    # ── UMM'NATUR (AI, factionId=2) ──────────────────────────────────────────
    {
        "type": "town", "name": "Tharakh-Unm",
        "q":  1, "r":  5, "factionId": 2
    },
    {
        "type": "goldmine", "name": "Desert Gold Seam",
        "q":  3, "r":  3, "factionId": 2
    },
    {
        "type": "crystalmine", "name": "Sunstone Vein",
        "q": -2, "r":  4, "factionId": 0
    },
    {
        "type": "dungeon", "name": "Tomb of Akherath",
        "q": -3, "r":  3, "factionId": 0
    },
    {
        "type": "artifact", "name": "Eye of Djangjix",
        "q":  2, "r":  6, "factionId": 0
    },
]

# Ensure object tiles are passable and correct terrain
object_coords = {(o["q"], o["r"]) for o in objects}
for tile in tiles:
    if (tile["q"], tile["r"]) in object_coords:
        # Force passable and set appropriate terrain
        tile["passable"] = True
        if tile["terrain"] in ("wall", "obsidian", "mountain"):
            tile["terrain"] = "grass" if tile["r"] < 0 else "sand"
            tile["moveCost"] = 1.0

# ── write JSON ───────────────────────────────────────────────────────────────

map_data = {
    "version": 1,
    "name":    "The Pale Divide",
    "radius":  RADIUS,
    "tiles":   tiles,
    "objects": objects
}

out_path = "data/maps/the_pale_divide.json"
with open(out_path, "w") as f:
    json.dump(map_data, f, indent=2)

print(f"Written {len(tiles)} tiles + {len(objects)} objects → {out_path}")
print(f"  Gate at q={GATE_Q}, r=-2 and r=-1")
print(f"  Verdant Reach town: Dun Daghan at (-1,-5)")
print(f"  Umm'Natur town:     Tharakh-Unm at (1,5)")
