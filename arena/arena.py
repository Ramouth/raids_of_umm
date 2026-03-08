# arena.py — combat mechanics scratchpad
# Throwaway. Run: python arena.py   (requires: pip install pygame)
#
# Keys: M=move  S=special-AoE  P=push  SPACE=end-turn  ESC=cancel  Q=quit
# Mechanics under test:
#   - Commander : passive defense aura (green ring), optional small AoE
#   - Spellcaster: push enemy N hexes (cooldown)
#   - Spec.Char  : AoE special attack (cooldown, hover-preview)
#   - Items      : move_bonus / radius_bonus affect aura + AoE radius

import pygame
import math
from collections import deque
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional
from sc_defs import USHARI, EffectKind

# ── Config ────────────────────────────────────────────────────────────────────
W, H = 1140, 760
HEX_SIZE = 50
FPS = 60

C_BG = (18, 14, 10)
C_HEX = (45, 38, 28)
C_HEX_LINE = (70, 58, 42)
C_HOVER = (90, 78, 55)
C_MOVE = (30, 70, 130)
C_AOE_PRE = (90, 22, 15)
C_PUSH_TGT = (130, 90, 0)
C_ATTACK_TGT = (180, 50, 50)  # Red for attack targets
C_AURA_A = (40, 160, 60)
C_AURA_B = (160, 40, 40)
C_TEAM_A = (100, 180, 255)
C_TEAM_B = (255, 100, 90)
C_HP_BG = (30, 0, 0)
C_HP_FG = (0, 200, 60)
C_TEXT = (210, 200, 180)
C_DIM = (130, 120, 105)
C_CD = (220, 140, 20)
C_ACTIVE = (255, 240, 80)
C_WIN = (255, 220, 60)

# ── Hex math (odd-r offset coordinates - HoMM style rectangular) ─────────────────
SQRT3 = math.sqrt(3)

GRID_W = 9
GRID_H = 6


def h2p(col, row, ox, oy):
    """Offset hex (col,row) → screen pixel. Odd-r offset, pointy-top."""
    x = HEX_SIZE * SQRT3 * (col + 0.5 * (row & 1))
    y = HEX_SIZE * 3 / 2 * row
    return int(ox + x), int(oy + y)


def p2h(px, py, ox, oy):
    """Screen pixel → offset hex (rounded)."""
    px -= ox
    py -= oy
    row = int(py / (HEX_SIZE * 3 / 2))
    row = max(0, min(GRID_H - 1, row))
    col_float = (px / (HEX_SIZE * SQRT3)) - 0.5 * (row & 1)
    col = int(round(col_float))
    col = max(0, min(GRID_W - 1, col))
    return (col, row)


DIRS_OFFSET = [(1, 0), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1)]


def neighbors(col, row):
    """Odd-r offset neighbors."""
    return [(col + DIRS_OFFSET[i][0], row + DIRS_OFFSET[i][1]) for i in range(6)]


def hdist(a, b):
    """Hex distance in offset coordinates."""
    col1, row1 = a
    col2, row2 = b
    ax = col1 - (row1 & 1) // 2
    ay = row1
    bx = col2 - (row2 & 1) // 2
    by = row2
    return (abs(ax - bx) + abs(ay - by) + abs(ax + ay - bx - by)) // 2


def in_radius(center, radius):
    """All hex coords within `radius` steps of center (inclusive), within grid bounds."""
    col, row = center
    result = []
    for dr in range(-radius, radius + 1):
        for dc in range(-radius, radius + 1):
            new_col, new_row = col + dc, row + dr
            if 0 <= new_col < GRID_W and 0 <= new_row < GRID_H:
                if hdist((col, row), (new_col, new_row)) <= radius:
                    result.append((new_col, new_row))
    return result


def reachable_bfs(start, move_range, blocked):
    """BFS: all hexes reachable within move_range, not passing through blocked."""
    visited = {start: 0}
    q = deque([start])
    while q:
        pos = q.popleft()
        d = visited[pos]
        if d >= move_range:
            continue
        for n in neighbors(*pos):
            if 0 <= n[0] < GRID_W and 0 <= n[1] < GRID_H:
                if n not in visited and n not in blocked:
                    visited[n] = d + 1
                    q.append(n)
    visited.pop(start)
    return set(visited)


def push_dest(unit_pos, source_pos, steps, grid, blocked):
    """Move unit_pos away from source_pos by up to `steps`, staying in grid."""
    pos = unit_pos
    for _ in range(steps):
        ns = neighbors(*pos)
        valid = [
            n
            for n in ns
            if 0 <= n[0] < GRID_W and 0 <= n[1] < GRID_H and n not in blocked
        ]
        if not valid:
            break
        pos = max(valid, key=lambda n: hdist(n, source_pos))
    return pos


def hex_pts(cx, cy):
    """Six corner points of a pointy-top hex centered at (cx, cy)."""
    pts = []
    for i in range(6):
        a = math.radians(60 * i + 30)
        pts.append((cx + HEX_SIZE * math.cos(a), cy + HEX_SIZE * math.sin(a)))
    return pts


# ── Data ──────────────────────────────────────────────────────────────────────
class Kind(Enum):
    COMMANDER = auto()  # passive aura + optional small AoE
    SPELLCASTER = auto()  # push/pull enemies
    SPECIAL = auto()  # AoE special attack with cooldown


@dataclass
class Item:
    name: str
    move_bonus: int = 0
    radius_bonus: int = 0


@dataclass
class Unit:
    name: str
    kind: Kind
    team: int  # 0 = A (player), 1 = B (AI)
    pos: tuple

    hp: int
    max_hp: int
    initiative: int
    move: int

    # Basic attack
    attack: int = 0
    attack_range: int = 0  # 0 = no attack, 1 = melee

    # AoE special (SPECIAL + optionally COMMANDER)
    sp_damage: int = 0
    sp_radius: int = 0
    sp_cd_max: int = 0  # 0 = no special
    sp_cd: int = 0  # 0 = ready

    # Aura (COMMANDER)
    aura_radius: int = 0
    aura_def: int = 0  # defense bonus to friendlies inside

    # Push (SPELLCASTER)
    push_range: int = 0  # 0 = no push
    push_steps: int = 2
    push_cd_max: int = 0
    push_cd: int = 0

    items: list = field(default_factory=list)

    moved: bool = False
    acted: bool = False
    alive: bool = True

    @property
    def eff_move(self):
        return self.move + sum(i.move_bonus for i in self.items)

    @property
    def eff_sp_radius(self):
        return self.sp_radius + sum(i.radius_bonus for i in self.items)

    @property
    def eff_aura_radius(self):
        return self.aura_radius + sum(i.radius_bonus for i in self.items)

    @property
    def sp_ready(self):
        return self.sp_cd == 0 and self.sp_cd_max > 0

    @property
    def push_ready(self):
        return self.push_cd == 0 and self.push_range > 0 and self.push_cd_max > 0


# ── SC runtime state ──────────────────────────────────────────────────────────


@dataclass
class SCState:
    defn: object  # SCDefinition — untyped to avoid circular import issues
    unit: object  # the linked Unit in combat
    level: int = 1
    xp: int = 0
    unlocked_actions: set = field(default_factory=set)

    def xp_needed(self):
        """XP required to reach next level, or None if at max."""
        if self.level >= self.defn.max_level:
            return None
        return self.defn.xp_thresholds[self.level - 1]

    def add_xp(self, amount: int) -> bool:
        """Award XP. Returns True if a level-up threshold was crossed."""
        if self.xp_needed() is None:
            return False
        self.xp += amount
        return self.xp >= self.xp_needed()

    def apply_level_up(self) -> list:
        """Advance one level: apply stat growth, heal, unlock abilities. Returns log lines."""
        self.level += 1
        msgs = [f"*** {self.defn.name} reached level {self.level}! ***"]

        # Stat growth — only touch fields the Unit actually has
        for stat, delta in self.defn.stat_growth.items():
            if hasattr(self.unit, stat):
                old = getattr(self.unit, stat)
                setattr(self.unit, stat, old + delta)
                msgs.append(f"  {stat}: {old} → {old + delta}")

        # Flat heal on level-up
        healed = min(self.defn.level_up_heal, self.unit.max_hp - self.unit.hp)
        self.unit.hp += healed
        msgs.append(f"  Healed {healed} HP")

        # Unlock abilities defined for this level
        for ab in self.defn.abilities:
            if ab.level == self.level:
                for eff in ab.effects:
                    if eff.kind == EffectKind.UNLOCK:
                        self.unlocked_actions.add(eff.action_key)
                        msgs.append(f"  Unlocked: {eff.action_key}")

        return msgs


# ── Scene setup ───────────────────────────────────────────────────────────────
def make_grid():
    """Rectangular hex grid (HoMM style)."""
    return {(col, row) for col in range(GRID_W) for row in range(GRID_H)}


def make_units():
    # ── Team A — player (left side of rectangle) ────────────────────────────────
    commander = Unit(
        name="Commander",
        kind=Kind.COMMANDER,
        team=0,
        pos=(1, 2),
        hp=40,
        max_hp=40,
        initiative=5,
        move=2,
        attack=8,
        attack_range=1,
        aura_radius=2,
        aura_def=3,
        # Also has a small rallying-shout AoE (special)
        sp_damage=5,
        sp_radius=1,
        sp_cd_max=3,
        # Ring of Reach: +1 radius → aura 2→3, sp_radius 1→2
        items=[Item("Ring of Reach", radius_bonus=1)],
    )
    spellcaster = Unit(
        name="Spellcaster",
        kind=Kind.SPELLCASTER,
        team=0,
        pos=(2, 3),
        hp=22,
        max_hp=22,
        initiative=8,
        move=3,
        attack=4,
        attack_range=1,
        push_range=5,
        push_steps=3,
        push_cd_max=2,
    )
    special = Unit(
        name="Spec.Char",
        kind=Kind.SPECIAL,
        team=0,
        pos=(1, 4),
        hp=32,
        max_hp=32,
        initiative=7,
        move=3,
        attack=6,
        attack_range=1,
        sp_damage=12,
        sp_radius=2,
        sp_cd_max=3,
        # Boots of Speed: +1 move
        items=[Item("Boots of Speed", move_bonus=1)],
    )

    # ── Team B — AI (right side of rectangle) ──────────────────────────────────
    en_cmd = Unit(
        name="En.Cmdr",
        kind=Kind.COMMANDER,
        team=1,
        pos=(7, 2),
        hp=38,
        max_hp=38,
        initiative=4,
        move=2,
        attack=7,
        attack_range=1,
        aura_radius=2,
        aura_def=3,
        sp_damage=5,
        sp_radius=1,
        sp_cd_max=3,
    )
    en_sc = Unit(
        name="En.Spec",
        kind=Kind.SPECIAL,
        team=1,
        pos=(6, 3),
        hp=28,
        max_hp=28,
        initiative=6,
        move=3,
        attack=5,
        attack_range=1,
        sp_damage=10,
        sp_radius=2,
        sp_cd_max=3,
    )
    en_mage = Unit(
        name="En.Mage",
        kind=Kind.SPELLCASTER,
        team=1,
        pos=(6, 2),
        hp=20,
        max_hp=20,
        initiative=9,
        move=3,
        attack=3,
        attack_range=1,
        push_range=4,
        push_steps=2,
        push_cd_max=2,
    )

    return [commander, spellcaster, special, en_cmd, en_sc, en_mage]


# ── Arena ─────────────────────────────────────────────────────────────────────
class Arena:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((W, H))
        pygame.display.set_caption("Raid Combat - Turn-Based Tactical Arena")
        self.clock = pygame.time.Clock()
        self.fsm = pygame.font.SysFont("monospace", 11)
        self.fmd = pygame.font.SysFont("monospace", 14)
        self.flg = pygame.font.SysFont("monospace", 16, bold=True)

        # Grid centered - HoMM style rectangular with units on left/right edges
        grid_pixel_w = GRID_W * HEX_SIZE * SQRT3
        grid_pixel_h = GRID_H * HEX_SIZE * 1.5
        self.ox = (W - grid_pixel_w) // 2 + HEX_SIZE // 2
        self.oy = (H - grid_pixel_h) // 2 + HEX_SIZE // 2

        self.grid = make_grid()
        self.units = make_units()

        # Static initiative order (ties broken alphabetically for consistency)
        self.order = sorted(self.units, key=lambda u: (-u.initiative, u.name))
        self.tidx = 0
        self.turn_count = 1
        self.round = 1

        self.active: Optional[Unit] = None
        self.mode: str = "idle"
        self.hovered: Optional[tuple] = None
        self.game_over: bool = False

        self.move_set: set = set()
        self.aoe_set: set = set()  # preview cells for special
        self.push_set: set = set()  # valid push target positions
        self.attack_set: set = set()  # valid attack target positions

        # Link SC to the player Special Character unit
        sc_unit = next(u for u in self.units if u.kind == Kind.SPECIAL and u.team == 0)
        self.sc = SCState(defn=USHARI, unit=sc_unit)

        self.log: list = [
            "═══ RAID COMBAT SIMULATOR ═══",
            "",
            "Combat uses turn-based initiative.",
            "Your team: Blue | Enemy team: Red",
            "Defeat all enemies to win!",
            "",
            "Round 1, Turn 1: Combat begins!",
        ]

        self._next_turn()

    # ── Turn management ───────────────────────────────────────────────────────
    def _next_turn(self):
        if self.game_over:
            return
        for _ in range(len(self.order)):
            u = self.order[self.tidx % len(self.order)]
            self.tidx += 1
            if u.alive:
                self._begin_turn(u)
                return

        self.round += 1
        self.turn_count = 1

    def _begin_turn(self, u: Unit):
        u.moved = u.acted = False
        self.active = u
        self._cancel()
        self.turn_count += 1
        self.log.append(
            f"--- Round {self.round}, Turn {self.turn_count}: {u.name} ({'A' if u.team == 0 else 'B'}) ---"
        )
        if u is self.sc.unit:
            if self.sc.add_xp(self.sc.defn.per_turn_xp):
                for msg in self.sc.apply_level_up():
                    self.log.append(msg)
        if u.team == 1:
            self._ai_turn(u)

    def _end_turn(self):
        u = self.active
        if u:
            if u.sp_cd > 0:
                u.sp_cd -= 1
            if u.push_cd > 0:
                u.push_cd -= 1
        self._cancel()
        self._check_win()
        self._next_turn()

    def _check_win(self):
        a = any(u.alive and u.team == 0 for u in self.units)
        b = any(u.alive and u.team == 1 for u in self.units)
        if not a:
            self.log.append("=== TEAM B WINS ===")
            self.game_over = True
        elif not b:
            self.log.append("=== TEAM A WINS ===")
            self.game_over = True

    # ── Mechanics ─────────────────────────────────────────────────────────────
    def _occupied(self, exclude=None):
        return {u.pos for u in self.units if u.alive and u is not exclude}

    def _aura_def_at(self, pos, team):
        """Sum defense bonus from all living friendly commanders whose aura covers pos."""
        return sum(
            u.aura_def
            for u in self.units
            if u.alive
            and u.team == team
            and u.kind == Kind.COMMANDER
            and hdist(pos, u.pos) <= u.eff_aura_radius
        )

    def _fire_aoe(self, caster: Unit, center: tuple):
        area = {h for h in in_radius(center, caster.eff_sp_radius) if h in self.grid}
        targets = [
            e for e in self.units if e.alive and e.team != caster.team and e.pos in area
        ]
        self.log.append(
            f"  AoE r={caster.eff_sp_radius} @ {center} ({len(targets)} hits)"
        )
        if not targets:
            self.log.append(f"    (no enemies in AoE range)")
        for e in targets:
            defense = self._aura_def_at(e.pos, e.team)
            dmg = max(0, caster.sp_damage - defense)
            e.hp -= dmg
            self.log.append(f"    {e.name}: -{dmg} dmg (aura def={defense})")
            if e.hp <= 0:
                e.alive = False
                self.log.append(f"    {e.name} defeated!")
                if caster is self.sc.unit:
                    if self.sc.add_xp(self.sc.defn.kill_bonus_xp):
                        for msg in self.sc.apply_level_up():
                            self.log.append(msg)
        caster.sp_cd = caster.sp_cd_max
        caster.acted = True

    def _do_push(self, caster: Unit, target: Unit):
        blocked = self._occupied(exclude=target)
        new_pos = push_dest(
            target.pos, caster.pos, caster.push_steps, self.grid, blocked
        )

        # Ensure new position is valid
        if new_pos == target.pos:
            self.log.append(f"  {target.name} cannot be pushed (blocked or at edge)")
        else:
            self.log.append(
                f"  {caster.name} pushes {target.name}: {target.pos} → {new_pos}"
            )
            target.pos = new_pos

        caster.push_cd = caster.push_cd_max
        caster.acted = True

    def _do_attack(self, attacker: Unit, target: Unit):
        defense = self._aura_def_at(target.pos, target.team)
        dmg = max(0, attacker.attack - defense)
        target.hp -= dmg
        self.log.append(
            f"  {attacker.name} attacks {target.name}: -{dmg} dmg (def={defense})"
        )
        if target.hp <= 0:
            target.alive = False
            self.log.append(f"  {target.name} defeated!")
            if attacker is self.sc.unit:
                if self.sc.add_xp(self.sc.defn.kill_bonus_xp):
                    for msg in self.sc.apply_level_up():
                        self.log.append(msg)
        attacker.acted = True

    # ── Simple AI ─────────────────────────────────────────────────────────────
    def _ai_turn(self, u: Unit):
        enemies = [e for e in self.units if e.alive and e.team != u.team]
        if not enemies:
            self._end_turn()
            return

        nearest = min(enemies, key=lambda e: hdist(u.pos, e.pos))

        # Move toward nearest enemy if it closes the gap
        blocked = self._occupied(exclude=u)
        reach = reachable_bfs(u.pos, u.eff_move, blocked) & self.grid
        if reach:
            best = min(reach, key=lambda h: hdist(h, nearest.pos))
            if hdist(best, nearest.pos) < hdist(u.pos, nearest.pos):
                u.pos = best
                u.moved = True
                self.log.append(f"  AI moves to {best}")

        # Use special if ready and worth firing
        if u.sp_ready and not u.acted:
            best_center = max(
                [u.pos] + [e.pos for e in enemies],
                key=lambda c: sum(
                    1 for e in enemies if hdist(c, e.pos) <= u.eff_sp_radius
                ),
            )
            hits = sum(
                1 for e in enemies if hdist(best_center, e.pos) <= u.eff_sp_radius
            )
            if hits > 0:
                self._fire_aoe(u, best_center)

        # Push if ready and a target is in range
        elif u.push_ready and not u.acted:
            in_push = [e for e in enemies if hdist(u.pos, e.pos) <= u.push_range]
            if in_push:
                # Push the one closest to their own commander (to pull them out of aura)
                target = min(in_push, key=lambda e: hdist(u.pos, e.pos))
                self._do_push(u, target)

        pygame.time.delay(300)
        self._end_turn()

    # ── Mode entry ────────────────────────────────────────────────────────────
    def _enter_move(self):
        u = self.active
        if not u or u.moved or u.team == 1:
            return
        blocked = self._occupied(exclude=u)
        self.move_set = reachable_bfs(u.pos, u.eff_move, blocked) & self.grid
        self.mode = "move"
        self.log.append("  Move mode  (click hex, ESC=cancel)")

    def _enter_special(self):
        u = self.active
        if not u:
            return
        if u.team == 1:
            self.log.append("  AI controls this unit")
            return
        if u.acted:
            self.log.append("  Already acted this turn")
            return
        if u.sp_cd_max == 0:
            self.log.append(f"  {u.name} has no special attack")
            return
        if not u.sp_ready:
            self.log.append(f"  Special on cooldown ({u.sp_cd} turns left)")
            return
        self.mode = "special"
        self.aoe_set = set()
        self.log.append(
            f"  AoE mode (r={u.eff_sp_radius}, dmg={u.sp_damage}) - hover to preview, click to fire"
        )

    def _enter_push(self):
        u = self.active
        if not u or u.acted or u.team == 1:
            return
        if not u.push_ready:
            self.log.append(f"  Push on cooldown ({u.push_cd} turns left)")
            return
        self.push_set = {
            e.pos
            for e in self.units
            if e.alive and e.team != u.team and hdist(u.pos, e.pos) <= u.push_range
        }
        self.mode = "push"
        self.log.append("  Push mode  (click an enemy, ESC=cancel)")

    def _enter_attack(self):
        u = self.active
        if not u:
            return
        if u.team == 1:
            self.log.append("  AI controls this unit")
            return
        if u.acted:
            self.log.append("  Already acted this turn")
            return
        if u.attack_range == 0:
            self.log.append(f"  {u.name} cannot attack")
            return
        self.attack_set = {
            e.pos
            for e in self.units
            if e.alive and e.team != u.team and hdist(u.pos, e.pos) <= u.attack_range
        }
        if not self.attack_set:
            self.log.append(f"  No enemies in attack range ({u.attack_range})")
            return
        self.mode = "attack"
        self.log.append(f"  Attack mode (ATK={u.attack}) - click enemy to attack")

    def _cancel(self):
        self.mode = "idle"
        self.move_set = self.aoe_set = self.push_set = self.attack_set = set()

    # ── Input ─────────────────────────────────────────────────────────────────
    def handle(self, ev) -> bool:
        if ev.type == pygame.QUIT:
            return False

        if ev.type == pygame.KEYDOWN:
            k = ev.key
            if k == pygame.K_q:
                return False
            elif k == pygame.K_ESCAPE:
                self._cancel()
            elif k == pygame.K_SPACE:
                self._end_turn()
            elif k == pygame.K_m:
                self._enter_move()
            elif k == pygame.K_a:
                self._enter_attack()
            elif k == pygame.K_s:
                self._enter_special()
            elif k == pygame.K_p:
                self._enter_push()

        if ev.type == pygame.MOUSEMOTION:
            mx, my = ev.pos
            h = p2h(mx, my, self.ox, self.oy)
            self.hovered = h if h in self.grid else None
            if self.mode == "special" and self.hovered and self.active:
                self.aoe_set = {
                    x
                    for x in in_radius(self.hovered, self.active.eff_sp_radius)
                    if x in self.grid
                }

        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self._on_click()

        return True

    def _on_click(self):
        h = self.hovered
        u = self.active
        if h is None or u is None or u.team == 1 or self.game_over:
            return

        if self.mode == "move" and h in self.move_set:
            u.pos = h
            u.moved = True
            self._cancel()
            self.log.append(f"  Moved to {h}")
            if u.acted:
                self._end_turn()

        elif self.mode == "special":
            if h is None:
                self.log.append("  No hex selected!")
            elif h not in self.grid:
                self.log.append("  Cannot fire AoE outside the grid!")
            else:
                self._fire_aoe(u, h)
                self._cancel()
                if u.moved:
                    self._end_turn()

        elif self.mode == "push" and h in self.push_set:
            target = next(
                (e for e in self.units if e.alive and e.pos == h and e.team != u.team),
                None,
            )
            if target:
                self._do_push(u, target)
                self._cancel()
                if u.moved:
                    self._end_turn()

        elif self.mode == "attack" and h in self.attack_set:
            target = next(
                (e for e in self.units if e.alive and e.pos == h and e.team != u.team),
                None,
            )
            if target:
                self._do_attack(u, target)
                self._cancel()
                if u.moved:
                    self._end_turn()

    # ── Drawing ───────────────────────────────────────────────────────────────
    def draw(self):
        self.screen.fill(C_BG)
        self._draw_grid()
        self._draw_auras()
        self._draw_units()
        self._draw_hud()
        if self.game_over:
            self._draw_game_over()
        pygame.display.flip()

    def _draw_grid(self):
        for h in self.grid:
            cx, cy = h2p(*h, self.ox, self.oy)
            if h in self.aoe_set:
                col = C_AOE_PRE
            elif h in self.move_set:
                col = C_MOVE
            elif h in self.push_set:
                col = C_PUSH_TGT
            elif h in self.attack_set:
                col = C_ATTACK_TGT
            elif h == self.hovered:
                col = C_HOVER
            else:
                col = C_HEX
            pts = hex_pts(cx, cy)
            pygame.draw.polygon(self.screen, col, pts)
            pygame.draw.polygon(self.screen, C_HEX_LINE, pts, 1)

    def _draw_auras(self):
        for u in self.units:
            if not u.alive or u.kind != Kind.COMMANDER:
                continue
            col = C_AURA_A if u.team == 0 else C_AURA_B
            for h in in_radius(u.pos, u.eff_aura_radius):
                if h in self.grid:
                    cx, cy = h2p(*h, self.ox, self.oy)
                    pygame.draw.polygon(self.screen, col, hex_pts(cx, cy), 2)

    def _draw_units(self):
        for u in self.units:
            if not u.alive:
                continue
            cx, cy = h2p(*u.pos, self.ox, self.oy)
            col = C_TEAM_A if u.team == 0 else C_TEAM_B
            r = HEX_SIZE // 2 - 5

            if u is self.active:
                pygame.draw.circle(self.screen, C_ACTIVE, (cx, cy), r + 5, 3)

            pygame.draw.circle(self.screen, col, (cx, cy), r)

            k_char = {Kind.COMMANDER: "C", Kind.SPELLCASTER: "S", Kind.SPECIAL: "X"}[
                u.kind
            ]
            surf = self.fmd.render(k_char, True, (0, 0, 0))
            self.screen.blit(
                surf, (cx - surf.get_width() // 2, cy - surf.get_height() // 2)
            )

            # HP bar
            bw = HEX_SIZE - 10
            bx, by = cx - bw // 2, cy + r + 3
            pygame.draw.rect(self.screen, C_HP_BG, (bx, by, bw, 5))
            fill = max(0, int(bw * u.hp / u.max_hp))
            pygame.draw.rect(self.screen, C_HP_FG, (bx, by, fill, 5))

            ns = self.fsm.render(u.name, True, C_TEXT)
            self.screen.blit(ns, (cx - ns.get_width() // 2, by + 7))

            # Cooldown badges
            badges = []
            if u.sp_cd > 0:
                badges.append(f"SP:{u.sp_cd}")
            if u.push_cd > 0:
                badges.append(f"PU:{u.push_cd}")
            if badges:
                bs = self.fsm.render(" ".join(badges), True, C_CD)
                self.screen.blit(bs, (cx - bs.get_width() // 2, cy - r - 15))

    def _draw_hud(self):
        u = self.active

        # ── Left panel — active unit stats ────────────────────────────────────
        lx, ly = 10, 10
        if u:
            kind_desc = {
                Kind.COMMANDER: "Commander - Aura + AoE (S)",
                Kind.SPELLCASTER: "Spellcaster - Push (P)",
                Kind.SPECIAL: "Special Char - AoE (S)",
            }
            lines = [
                f"╔══════════════════════════════════════╗",
                f"║ ROUND {self.round}  |  Turn {self.turn_count}           ║",
                f"╠══════════════════════════════════════╣",
                f"║ ACTIVE: {u.name:<25} ║",
                f"║ Team  : {'Player (A)' if u.team == 0 else 'Enemy (B)':<25} ║",
                f"║ Type  : {kind_desc.get(u.kind, ''):<25} ║",
                f"╠══════════════════════════════════════╣",
                f"║ HP     : {u.hp}/{u.max_hp}{' ' * (32 - len(f'{u.hp}/{u.max_hp}'))}║",
                f"║ Initiative: {u.initiative}{' ' * (25 - len(str(u.initiative)))}║",
                f"║ Move   : {u.eff_move} {'(done)' if u.moved else '':<20}║",
                f"║ Acted  : {'Yes' if u.acted else 'No'}{' ' * 27}║",
            ]
            if u.sp_cd_max > 0:
                s = f"CD:{u.sp_cd}" if u.sp_cd else "READY"
                lines.append(f"Special: r={u.eff_sp_radius}  dmg={u.sp_damage}  [{s}]")
            if u.push_range > 0:
                s = f"CD:{u.push_cd}" if u.push_cd else "READY"
                lines.append(
                    f"Push   : range={u.push_range}  steps={u.push_steps}  [{s}]"
                )
            if u.attack_range > 0:
                lines.append(f"Attack : ATK={u.attack}  range={u.attack_range}")
            if u.kind == Kind.COMMANDER:
                lines.append(f"Aura   : r={u.eff_aura_radius}  def+{u.aura_def}")
            if u.items:
                lines.append(f"Items  : {', '.join(i.name for i in u.items)}")
            # SC level / XP
            sc = self.sc
            xp_needed = sc.xp_needed()
            xp_bar = ""
            if xp_needed:
                filled = int(10 * sc.xp / xp_needed)
                xp_bar = (
                    "[" + "=" * filled + "-" * (10 - filled) + f"] {sc.xp}/{xp_needed}"
                )
            else:
                xp_bar = "[MAX LEVEL]"
            lines += [
                "",
                "═══════════ LEGEND ════════════",
                "",
                "■ Blue     = Your move range",
                "■ Dark Red = AoE preview (special)",
                "■ Orange   = Push targets",
                "■ Red      = Attack targets",
                "■ Green    = Your Commander aura",
                "■ Red      = Enemy Commander aura",
                "■ Hover    = Highlighted hex",
                "",
                f"MODE: {self.mode.upper()}",
            ]
            if sc.unlocked_actions:
                lines.append(f"    {' | '.join(sc.unlocked_actions)}")

            lines += [
                "",
                "═══════════ HOW TO PLAY ═══════════",
                "",
                "TURN FLOW:",
                "  1. Unit with highest INIT goes first",
                "  2. Move (M) - click blue hex to move",
                "  3. Act - Attack (A), Special (S), or Push (P)",
                "  4. Press SPACE to end turn",
                "",
                "COMBAT RULES:",
                "  • Commanders: Green aura gives +3 DEF",
                "  • All units can melee attack adjacent enemies",
                "  • AoE damage reduced by aura defense",
                "  • Push moves enemies away from caster",
                "  • Kill enemies for bonus XP",
                "",
                "CONTROLS:",
                "  M = Show move range (blue)",
                "  A = Attack enemy (red targets)",
                "  S = Special AoE attack (dark red preview)",
                "  P = Push enemy (orange targets)",
                "  SPACE = End turn",
                "  ESC = Cancel action",
                "  Q = Quit game",
                "",
                "TIPS:",
                "  • Stay in ally auras for defense",
                "  • Push enemies OUT of enemy auras",
                "  • Cluster enemies for bigger AoE hits",
                "  • Use Push to disrupt enemy positioning",
            ]
            for i, line in enumerate(lines):
                surf = self.fmd.render(line, True, C_TEXT)
                self.screen.blit(surf, (lx, ly + i * 18))

        # ── Right panel — initiative order ────────────────────────────────────
        rx = W - 220
        self.screen.blit(self.flg.render("Initiative", True, C_TEXT), (rx, 10))
        for i, ou in enumerate(self.order):
            if not ou.alive:
                continue
            col = (
                C_ACTIVE
                if ou is self.active
                else (C_TEAM_A if ou.team == 0 else C_TEAM_B)
            )
            line = f"{ou.initiative:2d}  {ou.name}"
            surf = self.fmd.render(line, True, col)
            self.screen.blit(surf, (rx, 30 + i * 18))

        # ── Aura defense tooltip at hovered hex ───────────────────────────────
        if self.hovered:
            ty = H // 2 + 80
            for team in (0, 1):
                d = self._aura_def_at(self.hovered, team)
                if d:
                    col = C_AURA_A if team == 0 else C_AURA_B
                    label = f"{'A' if team == 0 else 'B'} defense here: +{d}"
                    surf = self.fmd.render(label, True, col)
                    self.screen.blit(surf, (W // 2 - surf.get_width() // 2, ty))
                    ty += 18

        # ── Log (right panel) ────────────────────────────────────────────────────
        rx = W - 220
        ry = 10
        self.screen.blit(self.flg.render("Combat Log", True, C_TEXT), (rx, ry))

        # Draw box background for log
        log_bg_rect = (rx - 5, ry + 20, 205, H - 30)
        pygame.draw.rect(self.screen, (25, 20, 15), log_bg_rect)
        pygame.draw.rect(self.screen, C_HEX_LINE, log_bg_rect, 1)

        log_y = ry + 25
        for i, entry in enumerate(self.log[-12:]):
            surf = self.fsm.render(entry, True, C_DIM)
            self.screen.blit(surf, (rx, log_y + i * 14))

    def _draw_game_over(self):
        msg = self.log[-1]  # "=== TEAM X WINS ==="
        surf = self.flg.render(msg + "  (Q to quit)", True, C_WIN)
        bx = W // 2 - surf.get_width() // 2
        by = H // 2 - surf.get_height() // 2
        pygame.draw.rect(
            self.screen,
            (0, 0, 0),
            (bx - 8, by - 6, surf.get_width() + 16, surf.get_height() + 12),
        )
        self.screen.blit(surf, (bx, by))

    # ── Main loop ─────────────────────────────────────────────────────────────
    def run(self):
        running = True
        while running:
            for ev in pygame.event.get():
                running = self.handle(ev)
            self.draw()
            self.clock.tick(FPS)
        pygame.quit()


if __name__ == "__main__":
    Arena().run()
