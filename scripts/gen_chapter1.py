#!/usr/bin/env python3
"""
gen_chapter1.py — the four maps of chapter 1, "The Marches".

    1.0 The Last Hunt   hunting woods cut by narrow trails (a labyrinth); two packs of mad dogs
    1.1 Varenhold       a closed bowl of hills; one way out, east, over a bridge the keeper shuts
    1.2 The Camps       wide river fields; one bridge over the burn
    1.3 The East Road   a long ribbon climbing into the foothills; the Greyfangs at its end

Each map takes the land's own outline (no hexagon cut, no mountain frame). The edge
is whatever stops a traveller there: thick forest, a wide river, a tarn, crags;
mountains only at the end of 1.3. Where the story wants a fight or a meeting to be
unavoidable (the red banner at the bridge, the guards on the east road) the land is
pinched so the road runs through that one cell; chapter1_story.py's check proves it.

Writes version-1 maps (WorldMap::saveJson format) to DIR/ch1_*.json. The story
(triggers, encounters, campaign.json) comes from scripts/chapter1_story.py.

Coordinates: axial (q, r), flat-top hexes. Screen row Y = r + q/2. Shapes are
drawn in (q, Y); X = 0.866 q makes them round on screen.

Usage:  python3 scripts/gen_chapter1.py [--out DIR]     (default data/maps)
"""
import argparse
import heapq
import json
import math
from pathlib import Path

_here = Path(__file__).resolve().parent.parent
ROOT = _here if (_here / "data" / "units.json").exists() else Path.cwd()

# (passable, moveCost) — mirror terrainDefaultMoveCost() in src/world/MapTile.h.
# "crag" is impassable highland (cliffs); it is written as terrain "highland",
# passable false. Forest that is not a trail is a thicket (HoMM3 trees).
TERRAIN = {
    "grass":    (True,  1.0),
    "highland": (True,  1.25),
    "swamp":    (True,  1.75),
    "snow":     (True,  1.5),
    "bridge":   (True,  0.5),
    "forest":   (False, 1.5),
    "crag":     (False, 1.25),
    "mountain": (False, 1.8),
    "lake":     (False, 0.0),
    "river":    (False, 0.0),
}
CLEARABLE = {"forest": "grass", "crag": "highland"}   # a trail can be cut through these
DIRS = [(1, 0), (1, -1), (0, -1), (-1, 0), (-1, 1), (0, 1)]


# ── hex helpers ──────────────────────────────────────────────────────────────
def at(q, y):
    """(q, screen-row Y) → axial (q, r)."""
    return (q, math.floor(y - q / 2 + 0.5))


def row(cell):
    return cell[1] + cell[0] / 2


def dist(a, b):
    dq, dr = a[0] - b[0], a[1] - b[1]
    return max(abs(dq), abs(dr), abs(dq + dr))


def nbrs(cell):
    return [(cell[0] + dq, cell[1] + dr) for dq, dr in DIRS]


def noise(q, r, seed):
    h = (q * 374761393 + r * 668265263 + seed * 1442695040888963407) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 0xFFFF


def clump(q, r, seed, scale=3):
    total = 0.0
    for dq in (-1, 0, 1):
        for dr in (-1, 0, 1):
            total += noise((q + dq * scale) // scale, (r + dr * scale) // scale, seed)
    return total / 9


def walk(start, goal, seed):
    """A gap-free line of cells from start to goal; every step gets closer."""
    cell, out = start, [start]
    while cell != goal:
        options = [n for n in nbrs(cell) if dist(n, goal) < dist(cell, goal)]
        cell = min(options, key=lambda n: noise(*n, seed))
        out.append(cell)
        assert len(out) < 400, "walk lost its way"
    return out


def passable(tiles, cell):
    return cell in tiles and TERRAIN[tiles[cell]][0]


def reach(tiles, start, blocked=frozenset()):
    seen, stack = {start}, [start]
    while stack:
        c = stack.pop()
        for n in nbrs(c):
            if n not in seen and n not in blocked and passable(tiles, n):
                seen.add(n)
                stack.append(n)
    return seen


def route(tiles, a, b):
    """Cheapest passable path a → b (Dijkstra on move cost)."""
    best, prev, pq = {a: 0.0}, {}, [(0.0, a)]
    while pq:
        c, cell = heapq.heappop(pq)
        if cell == b:
            path = [cell]
            while cell in prev:
                cell = prev[cell]
                path.append(cell)
            return path[::-1]
        if c > best[cell]:
            continue
        for n in nbrs(cell):
            if not passable(tiles, n):
                continue
            nc = c + TERRAIN[tiles[n]][1]
            if nc < best.get(n, 1e9):
                best[n], prev[n] = nc, cell
                heapq.heappush(pq, (nc, n))
    return None


def carve(tiles, start, goal, locked):
    """Cut the cheapest trail to an unreachable goal through clearable thickets."""
    seen = reach(tiles, start)
    if goal in seen:
        return
    best, prev = {c: 0.0 for c in seen}, {}
    pq = [(0.0, c) for c in seen]
    heapq.heapify(pq)
    while pq:
        cost, cell = heapq.heappop(pq)
        if cell == goal:
            break
        if cost > best.get(cell, 1e9):
            continue
        for n in nbrs(cell):
            if n not in tiles or (n in locked and n != goal):
                continue
            t = tiles[n]
            step = 4.0 if t in CLEARABLE else (TERRAIN[t][1] if TERRAIN[t][0] or n == goal else None)
            if step is None:
                continue
            if cost + step < best.get(n, 1e9):
                best[n], prev[n] = cost + step, cell
                heapq.heappush(pq, (cost + step, n))
    assert goal in prev, f"no trail can reach {goal}"
    cell = goal
    while cell in prev:
        if tiles[cell] in CLEARABLE:
            tiles[cell] = CLEARABLE[tiles[cell]]
        cell = prev[cell]


# ── one map ──────────────────────────────────────────────────────────────────
class Land:
    """One map: an outline, an edge rule, a ground rule, then water, sites and roads."""

    def __init__(self, name, stem, seed, qrange, yrange):
        self.name, self.stem, self.seed = name, stem, seed
        self.qrange, self.yrange = qrange, yrange
        self.tiles = {}
        self.locked = set()       # cells the trail cutter must not touch
        self.bridges = set()
        self.gates = set()        # shut bridges: road on the river, closed until a story "open"
        self.gate = None          # the shut bridge the story opens, if any
        self.objects = []
        self.road_cells = set()
        self.exit_q = None        # the map ends when the hero walks to q >= exit_q
        self.chokes = {}          # object name → what it must stand in front of

    def shape(self, inside, edge, ground, open_edge=lambda q, y: False, ragged=0.3):
        land = set()
        for q in range(self.qrange[0], self.qrange[1] + 1):
            for r in range(self.yrange[0] - q // 2 - 2, self.yrange[1] - q // 2 + 3):
                if inside(q, row((q, r)), noise(q, r, self.seed)):
                    land.add((q, r))
        border = {c for c in land if any(n not in land for n in nbrs(c))}
        second = {c for c in land - border if any(n in border for n in nbrs(c))}
        for c in land:
            q, y = c[0], row(c)
            n = noise(*c, self.seed + 3)
            if c in border and not open_edge(q, y):
                t = edge(q, y, n)
            elif c in second and n < ragged and edge(q, y, n) in CLEARABLE:
                t = edge(q, y, n)                        # ragged inner fringe of woods / crags
            else:
                t = ground(q, y, n, clump(*c, self.seed + 11))
            self.tiles[c] = t
        self.border = border

    # ── labyrinth maps (Warcraft 2 style): fill with wall, then carve trails ──
    # fill() makes every cell of the outline a wall (thicket or crag); trail() and
    # pocket() carve the only open ground. A map built this way is a handful of
    # narrow trails, a few forks, short dead-end pockets and chokepoints; finish()
    # then proves every site is reachable and every pocket is a real dead end.

    def fill(self, inside, wall):
        """Outline as in shape(), but solid: wall(q, y, n) names each cell's wall
        ('forest' thicket or 'crag', or water / mountain for a hard edge)."""
        land = set()
        for q in range(self.qrange[0], self.qrange[1] + 1):
            for r in range(self.yrange[0] - q // 2 - 2, self.yrange[1] - q // 2 + 3):
                if inside(q, row((q, r)), noise(q, r, self.seed)):
                    land.add((q, r))
        self.border = {c for c in land if any(n not in land for n in nbrs(c))}
        for c in land:
            self.tiles[c] = wall(c[0], row(c), noise(*c, self.seed + 3))
        self.trail_cells = set()
        self.centre = set()       # trail centre lines: the way itself, never built on
        self.pockets = {}         # name → (junction cell, the pocket's own cells)

    def _open(self, cell, ground, strict):
        """Open one cell of a trail. strict: a centre-line cell, which must be openable."""
        if cell not in self.tiles or cell in self.border:
            assert not strict, f"{self.stem}: trail runs off the land at {cell}"
            return False
        t = self.tiles[cell]
        if t == "bridge" or cell in self.trail_cells:
            self.trail_cells.add(cell)
            return True
        if cell in self.locked or t in ("river", "lake", "mountain"):
            assert not strict, f"{self.stem}: trail runs into {t} at {cell} (cross water only at a bridge)"
            return False
        self.tiles[cell] = ground
        self.trail_cells.add(cell)
        return True

    def trail(self, waypoints, width=1, ground="grass", sides=None):
        """Carve a trail through the walls from waypoint to waypoint (gap-free).
        width 1: one hex; width 2: a second hex on the trail's left as it runs.
        sides: repaint the walls along it (e.g. 'forest' for a wooded stretch);
        only walls are repainted, so no open cell is ever closed. Returns the centre line."""
        line = []
        for a, b in zip(waypoints, waypoints[1:]):
            seg = walk(a, b, self.seed + 7)
            line.extend(seg if not line else seg[1:])
        self.centre.update(line)
        for i, c in enumerate(line):
            self._open(c, ground, strict=True)
            if width >= 2 and self.tiles[c] != "bridge":
                nxt = line[i + 1] if i + 1 < len(line) else c
                prv = line[i - 1] if i else c
                step = (nxt[0] - prv[0], nxt[1] - prv[1])
                d = min(range(6), key=lambda k: (DIRS[k][0] - step[0]) ** 2 + (DIRS[k][1] - step[1]) ** 2)
                side = (c[0] + DIRS[(d + 2) % 6][0], c[1] + DIRS[(d + 2) % 6][1])
                if self.tiles.get(side) not in ("river", "bridge") and not any(
                        self.tiles.get(n) == "bridge" for n in nbrs(side)):
                    self._open(side, ground, strict=False)
        if sides:
            for c in line:
                for n in nbrs(c):
                    if (n in self.tiles and n not in self.locked
                            and self.tiles[n] in CLEARABLE and n not in self.trail_cells):
                        self.tiles[n] = sides
        return line

    def pocket(self, name, junction, waypoints, room=0, ground="grass"):
        """A short dead end off a trail: from junction (a trail cell) along waypoints,
        ending in a room of the given radius. Returns the far end (where its prize sits).
        finish() proves nothing else opens into it."""
        assert junction in self.trail_cells, f"{self.stem}: pocket {name} does not start on a trail"
        before = set(self.trail_cells)
        line = self.trail([junction] + list(waypoints), 1, ground)
        end = line[-1]
        if room:
            for c in self.tiles:
                if dist(c, end) <= room:
                    self._open(c, ground, strict=False)
        self.pockets[name] = (junction, (self.trail_cells - before) - {junction})
        return end

    def wayside(self, line, index, ground="grass"):
        """A cell beside a trail for a site the hero visits on his way (a lodge, a shrine):
        next to line[index], off every centre line, so the way past stays open. Uses the
        trail's own second hex if it has one, else opens a one-hex niche into the wall."""
        c = line[index]
        def free(n):
            return (n in self.tiles and n not in self.centre and n not in self.border
                    and n not in self.locked and self.tiles[n] not in ("river", "bridge", "lake", "mountain"))
        open_side = [n for n in nbrs(c) if free(n) and n in self.trail_cells]
        if open_side:
            return min(open_side, key=lambda n: noise(*n, self.seed + 13))
        niche = [n for n in nbrs(c) if free(n)
                 and not any(m in self.trail_cells for m in nbrs(n) if m != c and m not in nbrs(c))]
        assert niche, f"{self.stem}: no room beside the trail at {c}"
        n = min(niche, key=lambda n: noise(*n, self.seed + 13))
        self._open(n, ground, strict=True)
        return n

    def clearing(self, center, radius=1, ground="grass"):
        """A small open place (a start, a fork, a den), carved like a trail."""
        for c in list(self.tiles):
            if dist(c, center) <= radius:
                self._open(c, ground, strict=False)

    def check_labyrinth(self):
        """Every pocket is a dead end: shut its junction and none of it is reachable."""
        for name, (junction, cells) in getattr(self, "pockets", {}).items():
            assert cells, f"{self.stem}: pocket {name} carved nothing"
            leak = cells & reach(self.tiles, self.start, frozenset({junction}))
            assert not leak, f"{self.stem}: pocket {name} is not a dead end (opens at {sorted(leak)[:3]})"

    def paint(self, cells, terrain, lock=False):
        for c in cells:
            if c in self.tiles:
                self.tiles[c] = terrain
                if lock:
                    self.locked.add(c)

    def stream(self, waypoints, bridges=()):
        """A river from waypoint to waypoint (gap-free); bridge cells stay passable road."""
        cells = []
        for a, b in zip(waypoints, waypoints[1:]):
            seg = walk(a, b, self.seed + 5)
            cells.extend(seg if not cells else seg[1:])
        self.paint(cells, "river", lock=True)
        for b in bridges:
            assert b in cells, f"bridge {b} is not on the stream"
            self.bridges.add(b)
            self.tiles[b] = "bridge"
        return cells

    def place(self, typ, name, cell, faction=0, kind="", exact=False):
        """Put a site at cell; unless exact, nudge it off the edge and out of water."""
        taken = {(o["q"], o["r"]) for o in self.objects}
        def fits(c):
            return (c in self.tiles and c not in self.border and c not in taken
                    and c not in self.locked and self.tiles[c] not in ("river", "bridge", "lake", "mountain"))
        if not exact and not fits(cell):
            near = sorted((c for c in self.tiles if fits(c)), key=lambda c: (dist(c, cell), c))
            assert near and dist(near[0], cell) <= 3, f"{self.stem}: no room for {name} near {cell}"
            cell = near[0]
        assert cell in self.tiles, f"{self.stem}: {name} at {cell} is outside the map"
        assert self.tiles[cell] not in ("river", "bridge", "lake"), f"{name} at {cell} sits in water"
        assert cell not in taken, f"{name} shares {cell}"
        if not TERRAIN[self.tiles[cell]][0]:
            self.tiles[cell] = "highland" if self.tiles[cell] in ("crag", "mountain") else "grass"
        o = {"q": cell[0], "r": cell[1], "type": typ, "name": name}
        if faction:
            o["factionId"] = faction
        if kind:
            o["kind"] = kind
        self.objects.append(o)
        self.locked.add(cell)
        return cell

    def finish(self, start, road=()):
        self.start = start
        if not TERRAIN[self.tiles[start]][0]:
            self.tiles[start] = "grass"
        for g in [(o["q"], o["r"]) for o in self.objects] + list(road):
            carve(self.tiles, start, g, self.locked)
        for a, b in zip(road, road[1:]):
            path = route(self.tiles, a, b)
            assert path, f"{self.stem}: no road {a} → {b}"
            self.road_cells.update(path)
        self.check_labyrinth()

    def to_json(self):
        out_tiles = []
        for (q, r), t in sorted(self.tiles.items()):
            ok, cost = TERRAIN[t]
            tile = {"q": q, "r": r, "terrain": {"crag": "highland", "bridge": "river"}.get(t, t),
                    "passable": ok, "moveCost": cost, "variant": 0}
            if ((q, r) in self.road_cells and ok) or t == "bridge":
                tile["road"] = True
                tile["moveCost"] = min(cost, 0.5)
            if (q, r) in self.gates:
                tile["passable"] = False             # the story opens it ({"open": [q, r]})
            out_tiles.append(tile)
        radius = max(dist(c, (0, 0)) for c in self.tiles)
        return {"version": 1, "name": self.name, "radius": radius, "ground": "grass",
                # Not read by the engine yet (see the chapter 1 report): where the hero
                # starts. Today he spawns at the player's town, else on any passable tile.
                "start": {"q": self.start[0], "r": self.start[1]},
                "tiles": out_tiles, "objects": self.objects}


def capsule(x, y, a, b, rad):
    (ax, ay), (bx, by) = a, b
    t = max(0.0, min(1.0, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / ((bx - ax) ** 2 + (by - ay) ** 2)))
    return math.hypot(x - ax - t * (bx - ax), y - ay - t * (by - ay)) / rad


# ── 1.0 The Last Hunt ────────────────────────────────────────────────────────
def hunt():
    """Hunting woods a day across, built as a labyrinth: thicket and crag everywhere,
    trails cut through them. From the pool in the west one trail runs east and forks:
    north into a narrow wooded stretch where the Thicket Dogs wait (their den beyond),
    south-east to the stream's one bridge and the Stream Dogs on the far bank. Two short
    dead ends hold something: a cold fire up a side path in the north woods, and a pile
    on the near bank that looks across the water at the Stream Dogs."""
    L = Land("The Last Hunt", "ch1_1_0_hunt", 101, (-13, 11), (-9, 9))

    def inside(q, y, n):
        x = 0.866 * q
        return (x / 10.4) ** 2 + ((y - 0.2) / 7.8) ** 2 < 1.0 + 0.15 * (n - 0.5)

    def wall(q, y, n):
        if 0.866 * q < -8.6 and -1.5 < y < 2.5:
            return "lake"                                 # the reedy pool at the west end
        c = clump(q, round(y - q / 2), 101 + 11)
        return "crag" if c > 0.56 and y < 1.5 else "forest"

    L.fill(inside, wall)
    # The stream: out of the pool, east along the south; one bridge.
    bridge = at(1, 3.0)
    bq, br = bridge
    L.stream([at(-9, 2.5), at(-6, 3.0), at(-3, 3.0), (bq - 1, br), bridge, (bq + 1, br),
              at(3, 3.0), at(6, 3.0), at(9, 3.0), at(11, 2.5)], [bridge])

    start = at(-9, 0)                                     # on the shore of the pool
    L.clearing(start, 1)
    fork = at(-4, 0.5)                                    # north to the woods, south-east to the stream
    main = L.trail([start, at(-6, 0.5), fork], width=2)
    # North: up to a bend, through the narrow wooded stretch (the Thicket Dogs), to their den.
    bend = at(-3, -4)
    L.trail([fork, at(-3, -2), bend], width=1)
    narrow = L.trail([bend, at(-1, -5), at(2, -5.5)], width=1, sides="forest")
    den = at(8, -2)
    L.trail([narrow[-1], at(4, -4.5), at(6, -3), den], width=1)
    L.clearing(den, 1)
    # South-east: down to the stream, over its one bridge, along the far bank to the Stream Dogs.
    turn = at(0, 1)                                       # the bridge, or the bank pocket
    near_bank, far_bank = (bq, br - 1), (bq, br + 1)
    L.trail([fork, at(-2, 1), turn], width=2)
    L.trail([turn, at(1, 2), near_bank, bridge, far_bank], width=1)
    lair = at(7, 4.5)
    L.trail([far_bank, at(3, 4.5), at(5, 5), lair], width=2)
    L.clearing(at(8, 4), 1)

    # The two dead ends: a cold fire up a side path, a pile on the near bank.
    fire = L.pocket("cold fire", bend, [at(-5, -5), at(-6, -5)], room=1)
    bank = L.pocket("near bank", turn, [at(2, 0), at(4, 0.5), at(6, 2.0)])

    # On the main trail before the fork: the lodge sells slingers for the corridor fight.
    L.place("dwelling", "Hunters' Lodge", L.wayside(main, len(main) - 2), 0, "cruth_slinger", exact=True)
    L.place("guard", "Thicket Dogs", narrow[len(narrow) // 2], exact=True)
    L.place("guard", "Stream Dogs", lair, exact=True)
    L.place("pickup", "Poachers' Fire", fire, 0, "campfire", exact=True)
    L.place("pickup", "Cut Willow", bank, 0, "wood", exact=True)
    L.finish(start)
    return L


# ── 1.1 Varenhold ────────────────────────────────────────────────────────────
def varenhold():
    """A bowl of hills round the castle; the only gap runs east."""
    L = Land("Varenhold", "ch1_1_1_varenhold", 111, (-14, 18), (-10, 10))

    def inside(q, y, n):
        x = 0.866 * q
        bowl = (x / 9.6) ** 2 + (y / 8.2) ** 2
        neck = capsule(x, y, (7.5, 1.0), (16.5, 1.0), 2.3) ** 2
        return min(bowl, neck) < 1.0 + 0.22 * (n - 0.5)

    def edge(q, y, n):
        if q >= 10:
            return "crag" if n < 0.6 else "forest"       # the gap between the hills
        if y < -4:
            return "forest"                              # the north woods
        if q < -8:
            return "lake" if n < 0.7 else "forest"       # the tarn under the castle
        if y > 4:
            return "crag"                                # the southern hills
        return "crag" if n < 0.5 else "forest"

    def ground(q, y, n, c):
        x = 0.866 * q
        if (x / 9.6) ** 2 + (y / 8.2) ** 2 > 0.62 and q < 9 and n < 0.55:
            return "highland"                            # the hillsides of the bowl
        if c > 0.66:
            return "forest"
        return "grass"

    L.shape(inside, edge, ground, open_edge=lambda q, y: q >= 18 and abs(y - 1) <= 1.2)
    # A brook across the gap, bank to bank; its one bridge is shut until the dogs are dead.
    bridge = at(13, 1)
    bq, br = bridge
    L.stream([at(13, -6), (bq, br - 2), (bq, br - 1), bridge, (bq, br + 1), (bq, br + 2), at(13, 8)], [bridge])
    L.gates.add(bridge)
    home = L.place("town", "Varenhold", at(-5, 0), 1)
    L.place("sawmill", "Varen Sawmill", at(-6, -5), 1)
    L.place("quarry", "Varen Quarry", at(-2, 5), 1)
    L.place("mill", "Varen Windmill", at(-1, -2), 1, "windmill")
    L.place("pickup", "Log Pile", at(3, -5.5), 0, "wood")
    L.place("pickup", "Fallen Timber", at(-8, 3), 0, "wood")
    L.place("pickup", "Cut Stone", at(5, 5.5), 0, "stone")
    L.place("pickup", "Mason's Heap", at(-4, 5.5), 0, "stone")
    L.place("pickup", "Shepherd's Purse", at(9, -3), 0, "gold")
    L.place("dungeon", "Varen Barrow", at(-9, -2), 0, "sealed")
    L.place("quest_giver", "Vale Rune Stone", at(2, 1))
    L.place("guard", "Orchard Dogs", at(1, -4.5))
    L.place("guard", "Ford Dogs", at(6, 3))
    hollow = L.place("guard", "Hollow Dogs", at(7, -2.5))
    L.place("quest_giver", "Hooded Stranger", (hollow[0] + 1, hollow[1] - 1), 0, "druid", exact=True)
    L.place("quest_giver", "Bridge Keeper", (bq - 1, br + 1), exact=True)   # west bank, by the bridge
    L.finish(home, road=[home, at(1, 0.5), at(9, 1), bridge, at(18, 1)])
    L.exit_q = 15
    L.gate = bridge
    return L


# ── 1.2 The Camps ────────────────────────────────────────────────────────────
def camps():
    """River fields between the north woods and the wide river; the burn cuts them at one bridge."""
    L = Land("The Camps", "ch1_1_2_camps", 121, (-19, 19), (-11, 11))

    def inside(q, y, n):
        x = 0.866 * q
        fields = (x / 14.0) ** 2 + ((y - 0.5) / 8.4) ** 2
        necks = (abs(y) / 2.4) ** 2 if abs(q) >= 13 else 9
        return min(fields, necks) < 1.0 + 0.2 * (n - 0.5)

    def edge(q, y, n):
        if y > 3.0 and abs(q) < 13:
            return "river"                               # the wide river along the south
        if y < -3.5 and abs(q) < 13:
            return "forest" if n < 0.85 else "lake"     # the north woods, a mere or two
        return "forest"

    def ground(q, y, n, c):
        if c > 0.7:
            return "forest"
        if y > 5.0 and n < 0.4:
            return "swamp"                               # water meadows by the river
        return "grass"

    L.shape(inside, edge, ground, open_edge=lambda q, y: abs(q) >= 18 and abs(y) <= 1.0, ragged=0.25)
    # The wide river: a second band of water inside the southern edge.
    L.paint([c for c in L.tiles if c not in L.border and row(c) > 3.0 and abs(c[0]) < 13
             and any(n in L.border and L.tiles[n] == "river" for n in nbrs(c))], "river", lock=True)
    # The burn: from the north woods down to the river, one bridge on the road.
    bridge = at(5, 0)
    bq, br = bridge
    L.stream([at(4, -12), (bq, br - 2), (bq, br - 1), bridge, (bq, br + 1), (bq, br + 2), at(6, 12)], [bridge])
    camp = (bq + 1, br)
    L.paint([(bq + 1, br - 1)], "forest", lock=True)    # the camp holds the only east bank
    # Where the burn comes in from the north: two short ridges with the water running
    # down between them, and the first snow on the west bank under them.
    L.paint([(3, -9), (2, -8), (3, -8)], "mountain", lock=True)              # west ridge
    L.paint([(6, -10), (7, -10), (6, -9), (7, -9)], "mountain", lock=True)   # east ridge
    L.paint([(4, -9), (4, -8), (4, -7), (3, -7), (0, -7), (1, -8), (1, -7), (1, -6), (2, -6)], "snow")

    start = at(-18, 0)
    L.place("sawmill", "North Field Sawmill", at(-10, -5), 1)
    L.place("quarry", "River Quarry", at(-7, 4.5), 1)
    L.place("quest_giver", "Riverside Camp", at(-4, 3))
    L.place("quest_giver", "Meadow Camp", at(-1, -3.5))
    strangers = L.place("quest_giver", "Strangers' Camp", at(2, -6))
    L.tiles[strangers] = "grass"                         # only the Empty Camp sits on snow
    L.place("quest_giver", "Red Banner Camp", camp, exact=True)
    L.place("guard", "Copse Dogs", at(-12, -3.5))
    L.place("pickup", "Woodcutters' Stack", at(-13, -4.5), 0, "wood")
    L.place("pickup", "Mason's Cart", at(-12, 3), 0, "stone")
    L.place("guard", "Field Brigands", at(11, 3))
    L.place("pickup", "Brigand Purse", at(12, 3.5), 0, "gold")
    L.place("pickup", "Drover's Cart", at(9, -4), 0, "wood")
    L.place("quest_giver", "Empty Camp", (4, -6), exact=True)  # where the dogs came from: the foot of the snow, by the burn
    # The dogs' owner, come down the burn between the ridges to look for them. Two cells
    # above the Empty Camp, on the snow, so his zone does not cover the camp or the road.
    L.place("guard", "Northern Warrior", (4, -8), exact=True)
    # Behind you, off the road in the west: where riders who want the sword make camp.
    # The story removes it at the start; with the sword, a war-band rides out of it first.
    L.place("quest_giver", "Riders' Camp", at(-14, 3))
    L.finish(start, road=[start, at(-6, 0), at(-2, 0.5), bridge, camp, at(12, 0), at(18, 0)])
    L.exit_q = 16
    L.chokes = {"Red Banner Camp": "exit"}
    return L


# ── 1.3 The East Road ────────────────────────────────────────────────────────
def east_road():
    """A ribbon up a gorge into the foothills; the Greyfangs close it at the mine."""
    L = Land("The East Road", "ch1_1_3_east_road", 131, (-24, 24), (-14, 14))
    yc = lambda q: -0.3 * q                                  # the road climbs north-east
    pinches = (-11, -2, 12)                                   # dogs, the broken camp, trolls

    def half(q, side):
        h = 3.0 + 0.7 * math.sin(0.5 * q)
        if side < 0:
            h += 3.0 * math.exp(-((q + 6) / 2.0) ** 2)       # the northern dell (the cold fire)
        else:
            h += 3.0 * math.exp(-((q - 8) / 2.0) ** 2)       # the southern scree (the bears)
        if q >= 16:
            h = 2.0
        for p in pinches:
            if abs(q - p) <= 1:
                h = min(h, 1.6)
        return h

    def inside(q, y, n):
        d = y - yc(q)
        return -half(q, -1) < d < half(q, 1) and q <= 22

    def edge(q, y, n):
        if q >= 15:
            return "mountain"                                 # the Greyfangs, here and only here
        if y < yc(q):
            return "forest" if q < 1 else "crag"
        return "river" if q < 13 else "crag"                 # the gorge, then crags

    def ground(q, y, n, c):
        if q < -6:
            return "forest" if c > 0.66 else "grass"
        if q < 6:
            return "highland" if n < 0.4 + 0.05 * q else ("forest" if c > 0.72 else "grass")
        return "highland" if n < 0.85 else "grass"

    L.shape(inside, edge, ground, open_edge=lambda q, y: q <= -23 and abs(y - yc(q)) <= 1.0, ragged=0.25)
    # The side stream comes down from the north crags to the gorge; one bridge.
    bridge = at(3, yc(3))
    bq, br = bridge
    L.stream([at(2, yc(2) - 8), (bq, br - 2), (bq, br - 1), bridge, (bq, br + 1), (bq, br + 2), at(4, yc(4) + 8)], [bridge])
    # The last stretch: a single cut between the peaks to the mine mouth.
    yend = yc(19)
    corridor = [at(q, yend) for q in range(17, 22)]
    L.paint([c for c in L.tiles if c[0] >= 17 and c not in corridor], "mountain", lock=True)
    L.paint(corridor, "highland")

    start = at(-23, yc(-23))
    L.place("sawmill", "East Road Sawmill", at(-17, yc(-17) - 1.5))
    L.place("pickup", "Fallen Pine", at(-14, yc(-14) + 1.5), 0, "wood")
    L.place("guard", "Road Dogs", at(-11, yc(-11)))
    L.place("quest_giver", "Cold Fire", at(-6, yc(-6) - 3.5))
    L.place("quest_giver", "Broken Camp", at(-2, yc(-2)))
    L.place("quest_giver", "Bridge Post", (bq + 1, br - 1), exact=True)
    # Only while the hero carries the sword: a band holds the far end of the bridge
    # (the stream's one crossing), so the Bridge Post beside it is theirs too.
    L.place("guard", "Sword Hunters", (bq + 1, br), exact=True)
    L.place("guard", "Ridge Bears", at(8, yc(8) + 3))
    L.place("pickup", "Scree Stone", at(9, yc(9) + 3.5), 0, "stone")
    L.place("guard", "Stone Trolls", at(12, yc(12)))
    L.place("quarry", "Greyfang Quarry", at(14, yc(14) - 1.5))
    L.place("guard", "Root Wardens", corridor[2], exact=True)
    L.place("dungeon", "Root-Grown Mine", corridor[4], 0, "sealed", exact=True)
    L.finish(start, road=[start, at(-11, yc(-11)), at(-2, yc(-2)), bridge, (bq + 1, br), at(12, yc(12)), corridor[1]])
    L.chokes = {"Road Dogs": "Root-Grown Mine", "Broken Camp": "Root-Grown Mine", "Sword Hunters": "Root-Grown Mine",
                "Stone Trolls": "Root-Grown Mine", "Root Wardens": "Root-Grown Mine"}
    return L


MAPS = [hunt, varenhold, camps, east_road]


def build_all():
    return [f() for f in MAPS]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--out", type=Path, default=ROOT / "data" / "maps")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    for land in build_all():
        m = land.to_json()
        path = args.out / f"{land.stem}.json"
        path.write_text(json.dumps(m, indent=2) + "\n")
        walk_n = sum(1 for t in m["tiles"] if t["passable"])
        qs = [t["q"] for t in m["tiles"]]
        print(f"Wrote {path.name}: {len(m['tiles'])} tiles ({walk_n} walkable), "
              f"{len(m['objects'])} objects, {max(qs) - min(qs) + 1} columns wide")


if __name__ == "__main__":
    main()
