#!/usr/bin/env python3
"""
Generate data/maps/cradle_of_umm.json — campaign map 3.

Layout (axial flat-top hex, r increases southward):
  NW quadrant  (q < -1, r < 0)  : Verdant Reach — Grass / Forest / Highland
  SE quadrant  (q > 1,  r > 0)  : Umm'Natur    — Sand / Dune / Rock
  Centre       (dist <= 3)      : Contested Oasis Heartland — Oasis / Ruins
  Flanks                        : Mixed transition terrain
  Mountain ring + interior ridges form natural chokepoints
"""

import json, math, random

RADIUS = 9
SEED   = 77
rng    = random.Random(SEED)

# ── hex helpers ───────────────────────────────────────────────────────────────

def hex_in_radius(radius):
    for q in range(-radius, radius + 1):
        r1 = max(-radius, -q - radius)
        r2 = min( radius, -q + radius)
        for r in range(r1, r2 + 1):
            yield q, r

def hex_dist(q, r):
    return max(abs(q), abs(r), abs(q + r))

def dot(q, r, aq, ar):
    """Dot product in axial space (approximation of directionality)."""
    return q * aq + r * ar

# ── terrain assignment ────────────────────────────────────────────────────────

def terrain_for(q, r):
    dist = hex_dist(q, r)

    # Impassable border ring
    if dist >= RADIUS:
        return "mountain", False, 2.0

    # Inner mountain ring at radius 8 (sparse)
    if dist == RADIUS - 1:
        if rng.random() < 0.70:
            return "mountain", False, 2.0

    # Mountain ridges — two diagonal spurs creating chokepoints
    # NE spur: q == 3, r <= 0
    if q == 3 and -5 <= r <= -1 and rng.random() < 0.75:
        return "mountain", False, 2.0
    # SW spur: q == -3, r >= 1
    if q == -3 and 1 <= r <= 5 and rng.random() < 0.75:
        return "mountain", False, 2.0

    # ── Centre oasis heartland (dist <= 3) ───────────────────────────────────
    if dist <= 3:
        n = rng.random()
        if   n < 0.20: return "oasis",  True,  1.0
        elif n < 0.45: return "ruins",  True,  1.25
        elif n < 0.60: return "grass",  True,  1.0
        elif n < 0.70: return "rock",   True,  1.25
        else:           return "sand",   True,  1.0

    # ── Directional faction zones ─────────────────────────────────────────────
    # Use axial dot product to determine faction lean.
    # NW direction = (-1, -1) in axial space → Verdant Reach
    # SE direction = (+1, +1) in axial space → Umm'Natur
    lean = q + r   # positive = SE (desert), negative = NW (verdant)

    # ── Verdant Reach (strong lean NW: lean <= -3) ───────────────────────────
    if lean <= -3:
        if dist >= RADIUS - 2 and rng.random() < 0.5:
            return "mountain", False, 2.0
        n = rng.random()
        if   n < 0.15: return "forest",   True,  1.5
        elif n < 0.30: return "highland",  True,  1.25
        elif n < 0.38: return "rock",      True,  1.25
        else:           return "grass",    True,  1.0

    # ── Umm'Natur (strong lean SE: lean >= 3) ────────────────────────────────
    if lean >= 3:
        if dist >= RADIUS - 2 and rng.random() < 0.5:
            return "mountain", False, 2.0
        n = rng.random()
        if   n < 0.10: return "oasis",    True,  1.0
        elif n < 0.20: return "ruins",    True,  1.25
        elif n < 0.28: return "rock",     True,  1.25
        elif n < 0.34: return "obsidian", False, 0.0
        elif n < 0.50: return "dune",     True,  1.5
        else:           return "sand",    True,  1.0

    # ── Transition belt (-2 <= lean <= 2) ────────────────────────────────────
    n = rng.random()
    if lean < 0:
        # Leaning verdant
        if   n < 0.25: return "grass",    True,  1.0
        elif n < 0.40: return "highland",  True,  1.25
        elif n < 0.55: return "ruins",    True,  1.25
        elif n < 0.65: return "rock",     True,  1.25
        else:           return "sand",    True,  1.0
    else:
        # Leaning desert
        if   n < 0.30: return "sand",     True,  1.0
        elif n < 0.45: return "ruins",    True,  1.25
        elif n < 0.55: return "rock",     True,  1.25
        elif n < 0.65: return "grass",    True,  1.0
        else:           return "dune",    True,  1.5

def variant_for(terrain):
    counts = {"grass": 4, "forest": 3, "highland": 2,
              "sand": 6, "dune": 4, "rock": 3, "oasis": 2,
              "ruins": 3, "wall": 1, "mountain": 3, "obsidian": 1}
    return rng.randint(0, max(0, counts.get(terrain, 1) - 1))

# ── build tile list ───────────────────────────────────────────────────────────

tiles = []
for q, r in hex_in_radius(RADIUS):
    terrain, passable, move_cost = terrain_for(q, r)
    tiles.append({
        "q": q, "r": r,
        "terrain":  terrain,
        "passable": passable,
        "moveCost": move_cost,
        "variant":  variant_for(terrain)
    })

# ── objects ───────────────────────────────────────────────────────────────────

objects = [
    # ── VERDANT REACH (player, factionId=1) ──────────────────────────────────
    { "type": "town",       "name": "Caer Morvyn",      "q": -5, "r": -2, "factionId": 1 },
    { "type": "goldmine",   "name": "Ironvein Ridge",   "q": -4, "r": -4, "factionId": 1 },
    { "type": "dungeon",    "name": "The Hollow Mound",  "q": -2, "r": -5, "factionId": 0 },
    { "type": "artifact",   "name": "Cloak of the Mist","q": -6, "r": -1, "factionId": 0 },
    { "type": "sawmill",    "name": "Ashwood Mill",      "q": -4, "r": -3, "factionId": 1 },

    # ── NEUTRAL CENTRE ────────────────────────────────────────────────────────
    { "type": "crystalmine","name": "Heartspring Vein",  "q":  0, "r":  0, "factionId": 0 },
    { "type": "dungeon",    "name": "Vault of the Ancients","q": 2,"r": -2, "factionId": 0 },
    { "type": "dungeon",    "name": "The Sand Crypt",    "q": -2, "r":  2, "factionId": 0 },

    # ── UMM'NATUR (AI, factionId=2) ───────────────────────────────────────────
    { "type": "town",       "name": "Khet-Unmar",        "q":  5, "r":  2, "factionId": 2 },
    { "type": "goldmine",   "name": "Gilded Seam",       "q":  4, "r":  3, "factionId": 2 },
    { "type": "dungeon",    "name": "Tomb of Sekhamun",  "q":  3, "r":  5, "factionId": 0 },
    { "type": "artifact",   "name": "Eye of the Dune",   "q":  6, "r":  1, "factionId": 0 },
]

# Force passable + correct terrain under objects
object_coords = {(o["q"], o["r"]) for o in objects}
for tile in tiles:
    if (tile["q"], tile["r"]) in object_coords:
        tile["passable"] = True
        if tile["terrain"] in ("mountain", "obsidian", "wall"):
            tile["terrain"] = "grass" if (tile["q"] + tile["r"]) < 0 else "sand"
            tile["moveCost"] = 1.0

# ── write JSON ────────────────────────────────────────────────────────────────

map_data = {
    "version": 1,
    "name":    "The Cradle of Umm",
    "radius":  RADIUS,
    "tiles":   tiles,
    "objects": objects
}

out = "data/maps/cradle_of_umm.json"
with open(out, "w") as f:
    json.dump(map_data, f, indent=2)

print(f"Written {len(tiles)} tiles + {len(objects)} objects → {out}")
