# Raids of Umm'Natur — Art Direction

## Vision

**"Diablo II Act II meets The Dig."**

The surface world is a brutal, sun-scorched Egyptian desert — bleached bone and amber gold
baking under a hazy sky. Descend into a dungeon and everything changes: cold, airless dark,
ancient stone, and the purple-black glow of necromantic power. That contrast — warm/bright
above, cold/dark below — is the single most important visual idea in the game.

Reference mood:
- Lut Gholein and its surroundings (Diablo II Act II) — dusty heat, ancient ruins, oppressive weight
- The Tombs of Tal Rasha — extreme darkness, cold stone, forbidden magic
- The Dig (LucasArts) — atmospheric depth, limited palette used with precision, mystery
- HoMM3 — readable hex grid, chunky units with clear silhouettes

**We stay with pixel art.** The Diablo II art team proved you can achieve cinematic atmosphere
in pixel art with a disciplined palette and extreme value contrast. That is the model.

---

## The 26-Color Canonical Palette

Every sprite and tile must draw ONLY from these colors (or close enough that a viewer
wouldn't notice). Consistency across all assets is more important than any individual
sprite looking good in isolation.

### Surface — Warm Desert

| Role               | Hex       | Usage |
|--------------------|-----------|-------|
| Bleached highlight | `#F2E0A0` | Hottest sand, sun-struck stone edges |
| Sandy gold         | `#D4A030` | Main sand tone, dune faces |
| Warm sand mid      | `#B87828` | Shadow side of dunes, packed ground |
| Burnt sienna       | `#884010` | Deep sand shadows, crevice fill |
| Dark earth         | `#4A200A` | Darkest surface shadow, soil |

### Stone & Ruins

| Role               | Hex       | Usage |
|--------------------|-----------|-------|
| Pale limestone     | `#C8B888` | Sun-lit stone faces, ruined columns |
| Warm sandstone     | `#A08860` | Stone mid-tone, carved walls |
| Shadow stone       | `#685040` | Stone shadow side |
| Deep stone crack   | `#352018` | Mortar lines, crevices, outlines |

### Underground / Dungeon

| Role               | Hex       | Usage |
|--------------------|-----------|-------|
| Tomb black         | `#080610` | Deep background, void |
| Midnight indigo    | `#12102A` | Dungeon walls far from light |
| Cold stone dark    | `#1E1C38` | Dungeon floor, unlit surfaces |
| Dungeon grey       | `#3A3850` | Stone in dim torchlight |
| Lit stone          | `#5A5870` | Stone edges catching torch glow |

### Necromancy & Magic ← Purple is sacred

| Role               | Hex       | Usage |
|--------------------|-----------|-------|
| Bone white         | `#EAE0C0` | Skeleton/mummy bone, undead highlight |
| Pale violet        | `#C080E0` | Spell particle tips, magical sheen |
| Magic purple       | `#8030B0` | Necromantic glow core, SC magic fx |
| Deep purple        | `#501880` | Shadow of necromantic objects |
| Void purple        | `#1E0840` | Darkest necromantic shadow, auras |

> **Rule:** Purple ONLY appears on undead units, necromantic SCs, and magical artifacts.
> Never on terrain, buildings, or mundane objects. This makes it immediately readable
> as "something arcane is happening here."

### Fire & Torches

| Role               | Hex       | Usage |
|--------------------|-----------|-------|
| Flame white-tip    | `#FFF0A0` | Brightest flame tip |
| Fire orange        | `#E87020` | Main flame body, torch glow |
| Ember amber        | `#A04010` | Dying flame, warm shadow on walls |

### Water & Oasis

| Role               | Hex       | Usage |
|--------------------|-----------|-------|
| Water highlight    | `#80C8C0` | Water surface glint |
| Teal water         | `#3A8080` | Oasis pool body |
| Deep water         | `#1A5058` | Water shadow, river depth |
| Dusty sage         | `#607840` | Palm fronds, desert scrub |

### UI & Gold Accent

| Role               | Hex       | Usage |
|--------------------|-----------|-------|
| UI gold highlight  | `#F8C840` | HUD borders, selected state |
| Tarnished gold     | `#A07820` | Inactive UI elements, coins |

---

## Value Contrast — The Diablo II Secret

Diablo II Act II looks dramatic because of **extreme contrast**: near-white highlights
sit directly next to near-black shadows with almost nothing in between.

**Rule of thumb for every sprite:**
- The lightest pixel should be close to `#F2E0A0` or brighter
- The darkest shadow should be `#080610` or close to it
- The mid-tones should be used sparingly — mostly outlines and transition pixels
- **No grey fog.** Every shadow is either warm-tinted or cold-purple-tinted.

Surface shadows lean **warm** (burnt sienna, dark earth).
Underground shadows lean **cold** (midnight indigo, void purple).

---

## Lighting Direction

**All sprites: soft top-left ambient, single warm key light from upper-left.**

- Top-left faces: lightest value
- Bottom-right faces: darkest shadow
- Dungeon objects get a secondary cool purple rim light from below (undead glow)

This must be consistent across ALL units and objects or the scene looks incoherent.

---

## Unit Design Rules

1. **Strong silhouette** — the unit must read as a clear shape at 32×32 (it will be
   displayed smaller than 128×128 on the map). If the silhouette is ambiguous, the
   sprite has failed.

2. **One dominant color** — each unit type "owns" a color:
   - Warriors / human units: sandy gold + sandstone
   - Undead (mummy, skeleton): bone white + void purple accent
   - Djinn / spirits: teal + pale violet
   - Ancient Guardian: cold grey stone + ember amber cracks
   - Pharaoh Lich: tarnished gold + deep purple + bone

3. **1px dark outline** — always `#352018` (deep stone crack) or `#080610` (tomb black)
   for undead. Never pure RGB black — it reads as a scan artifact.

4. **No anti-aliasing** — hard edges only. Dithering is allowed for gradients but
   must use only adjacent palette colors.

---

## Terrain Rules (Surface)

- **Sand** edges must be `#D4A030` ± 1 tone — this is what makes adjacent tiles blend.
- **Dune** is one tone darker/more orange than sand — `#B87828` at edges.
- **Rock/Mountain** edges shade to `#A08860` — cooler, not warmer than sand.
- **Oasis** edges must match Sand tone — the water is contained in the center only.
- **Obsidian** is the only surface terrain with cold tones — it's volcanic, not desert.
- **Ruins** = stone poking through sand. Edges are sand-toned. Stone only in center.

## Terrain Rules (Underground / Dungeon)

- Base is always `#080610` to `#1E1C38`.
- Torch lit areas introduce `#E87020` warmth on nearby stone.
- Necromantic areas introduce `#501880` / `#8030B0` glow.
- No warm sand tones underground. The contrast must be total.

---

## Object Sprites (Map Icons)

| Object       | Dominant colors | Accent |
|--------------|-----------------|--------|
| Town         | `#C8B888` stone, `#D4A030` sand base | `#F8C840` gold roof details |
| Dungeon      | `#1E1C38` dark stone, `#352018` cracks | `#E87020` torch glow at entrance |
| Gold Mine    | `#A08860` stone, `#D4A030` sand | `#F8C840` gold ore veins |
| Crystal Mine | `#B87828` sand base | `#80C8C0` / `#C080E0` crystal formations |
| Artifact     | `#C8B888` stone pedestal | `#F8C840` glow + `#8030B0` magical aura |

---

## PixelLab Prompt Prefix

Prepend this block to EVERY sprite generation prompt:

```
Pixel art sprite for Raids of Umm'Natur, an Egyptian fantasy strategy game.
Art style: Diablo II Act II meets LucasArts — dramatic value contrast, limited
24-color desert palette, 1px dark outline on all shapes, no anti-aliasing, no blur.

PALETTE (use ONLY these tones):
Surface warm: #F2E0A0 #D4A030 #B87828 #884010 #4A200A
Stone/ruins:  #C8B888 #A08860 #685040 #352018
Underground:  #080610 #12102A #1E1C38 #3A3850 #5A5870
Necromancy:   #EAE0C0 #C080E0 #8030B0 #501880 #1E0840
Fire:         #FFF0A0 #E87020 #A04010
Water/oasis:  #80C8C0 #3A8080 #1A5058 #607840
UI/gold:      #F8C840 #A07820

LIGHTING: soft top-left key light. Surface shadows are warm (burnt sienna).
Underground shadows are cold (indigo/purple). Extreme highlight-to-shadow contrast.
```

---

## What Purple Means

Purple in this game is **always necromantic or magical**. Players will learn this fast:

- See purple glow → undead, cursed, or arcane
- Mummy wrappings: bone + void purple seeping through
- Pharaoh Lich: tarnished gold costume, deep purple aura, pale violet spell glow
- Sekhara (mage SC): fire school = orange/amber, NOT purple
- Djinn: teal + pale violet (spirits are adjacent to magic but not necromantic)
- Tomb/dungeon entrances: dark stone with purple seeping from inside

This is consistent with Diablo II: Necromancer and undead areas had that deep purple-black
that made them feel different from normal dangerous (which was red).
