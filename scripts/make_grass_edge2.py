#!/usr/bin/env python3
"""
Generates grass_sand_edge_2/ from grass_sand_edge/:
- Sand-coloured pixels (R > G+15 and R > 60) → transparent
- Dark outline pixels adjacent ONLY to transparent/sand pixels → also transparent
- Grass pixels and their outlines → kept opaque
Result: clean grass silhouette with no sand or orphaned outline pixels.
The transparent areas will show the tile's base terrain colour (sand)
since GrassSandEdge2 is registered with terrainColor = sand colour.
"""
import pathlib
import numpy as np
from PIL import Image

SRC = pathlib.Path("assets/textures/terrain/grass_sand_edge")
DST = pathlib.Path("assets/textures/terrain/grass_sand_edge_2")
DST.mkdir(parents=True, exist_ok=True)

for src_path in sorted(SRC.glob("*.png")):
    img = Image.open(src_path).convert("RGBA")
    arr = np.array(img, dtype=np.uint8)

    R = arr[:,:,0].astype(int)
    G = arr[:,:,1].astype(int)
    A = arr[:,:,3].copy()

    # Step 1: mark sand pixels transparent (warm orange/brown, not dark outline)
    is_sand = ((R - G) > 15) & (R > 60) & (A > 0)
    transparent = (A == 0) | is_sand  # seed: already-transparent + sand

    # Step 2: expand transparency into dark outline pixels that touch the transparent region.
    # Dark outline: R < 35, G < 35, B < 35, A > 0.
    # We iteratively remove any dark pixel that has >=1 transparent neighbour.
    B_ch = arr[:,:,2].astype(int)
    is_dark = (R < 35) & (G < 35) & (B_ch < 35) & (A > 0)

    for _ in range(4):  # a few passes removes the sand-side outline ring
        # Build a boolean mask: "has a transparent neighbour" using 8-connectivity
        t = transparent.astype(np.uint8)
        has_transparent_neighbour = (
            np.roll(t,  1, axis=0) | np.roll(t, -1, axis=0) |
            np.roll(t,  1, axis=1) | np.roll(t, -1, axis=1) |
            np.roll(t,  1, axis=0) * np.roll(t,  1, axis=1) |  # diagonals
            np.roll(t,  1, axis=0) * np.roll(t, -1, axis=1) |
            np.roll(t, -1, axis=0) * np.roll(t,  1, axis=1) |
            np.roll(t, -1, axis=0) * np.roll(t, -1, axis=1)
        ).astype(bool)
        newly_transparent = is_dark & has_transparent_neighbour
        transparent |= newly_transparent

    # Apply transparency
    result = arr.copy()
    result[transparent, 3] = 0

    dst_path = DST / src_path.name
    Image.fromarray(result).save(dst_path)
    n = int(transparent.sum()) - int((A == 0).sum())
    print(f"{src_path.name}: {n} pixels removed → {dst_path}")
