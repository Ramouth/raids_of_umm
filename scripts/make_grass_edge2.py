#!/usr/bin/env python3
"""
Generates grass_sand_edge_2/ from grass_sand_edge/:
- Sand-coloured pixels (R > G+15 and R > 60) → fully transparent
- Everything else (grass, dark outline) → kept as-is
This creates a grass-only overlay that composites cleanly over any terrain.
"""
import pathlib, numpy as np
from PIL import Image

SRC = pathlib.Path("assets/textures/terrain/grass_sand_edge")
DST = pathlib.Path("assets/textures/terrain/grass_sand_edge_2")
DST.mkdir(parents=True, exist_ok=True)

for src_path in sorted(SRC.glob("*.png")):
    img = Image.open(src_path).convert("RGBA")
    arr = np.array(img, dtype=np.uint8)

    R, G, A = arr[:,:,0].astype(int), arr[:,:,1].astype(int), arr[:,:,3]

    # Sand mask: warm colour (R > G by margin) and bright enough to not be outline.
    is_sand = (R - G > 15) & (R > 60) & (A > 0)

    arr[is_sand, 3] = 0  # make sand pixels transparent

    dst_path = DST / src_path.name
    Image.fromarray(arr).save(dst_path)
    n_sand = int(is_sand.sum())
    print(f"{src_path.name}: {n_sand} sand pixels removed → {dst_path}")
